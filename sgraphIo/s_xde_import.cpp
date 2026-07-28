#include "s_xde_import.h"

#include "s_kernel_shape_access.h"

#include <BRep_Builder.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <Message_ProgressRange.hxx>
#include <QFileInfo>
#include <QMap>
#include <Quantity_ColorRGBA.hxx>
#include <RWObj_CafReader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFPrs.hxx>
#include <XCAFPrs_IndexedDataMapOfShapeStyle.hxx>
#include <XCAFPrs_Style.hxx>
#include <algorithm>
#include <cmath>
#include <vector>

namespace smartGraphics3D
{
namespace
{
struct SStyleSetting
{
    TopoDS_Shape shape;
    XCAFPrs_Style style;
};

struct SFaceStyleState
{
    SAppearanceStyle style;
    bool from_source = false;
};

struct SXdeReadRequest
{
    const QString& file_path;
    const QString& extension;
};

Handle(TDocStd_Document) createDocument()
{
    Handle(TDocStd_Document) document;
    XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
    return document;
}

QColor toQColor(const Quantity_ColorRGBA& color)
{
    return QColor::fromRgbF(color.GetRGB().Red(), color.GetRGB().Green(), color.GetRGB().Blue());
}

bool appearanceStyle(const XCAFPrs_Style& source, SAppearanceStyle& target)
{
    Quantity_ColorRGBA color;
    if (source.IsSetColorSurf())
    {
        color = source.GetColorSurfRGBA();
    }
    else if (!source.Material().IsNull())
    {
        color = source.Material()->BaseColor();
    }
    else
    {
        return false;
    }
    target.color = toQColor(color);
    target.transparency = std::clamp(1.0 - static_cast<double>(color.Alpha()), 0.0, 1.0);
    return target.color.isValid();
}

QString styleKey(const SAppearanceStyle& style)
{
    return QStringLiteral("%1|%2")
        .arg(style.color.name(QColor::HexRgb))
        .arg(style.transparency, 0, 'f', 9);
}

bool sameStyle(const SAppearanceStyle& first, const SAppearanceStyle& second)
{
    return first.color.rgb() == second.color.rgb() &&
           std::abs(first.transparency - second.transparency) <= 1.0e-9;
}

SAppearanceStyle mostFrequentStyle(const QVector<SFaceStyleState>& faces, bool source_only)
{
    QMap<QString, int> counts;
    QMap<QString, SAppearanceStyle> styles;
    for (const SFaceStyleState& face : faces)
    {
        if (source_only && !face.from_source)
        {
            continue;
        }
        const QString key = styleKey(face.style);
        counts[key] = counts.value(key) + 1;
        styles.insert(key, face.style);
    }

    QString selected;
    int selected_count = -1;
    for (auto iterator = counts.cbegin(); iterator != counts.cend(); ++iterator)
    {
        if (iterator.value() > selected_count)
        {
            selected = iterator.key();
            selected_count = iterator.value();
        }
    }
    return styles.value(selected);
}

TopoDS_Shape documentShape(const Handle(TDocStd_Document) & document,
                           TDF_LabelSequence& free_labels)
{
    const Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    shape_tool->GetFreeShapes(free_labels);
    if (free_labels.IsEmpty())
    {
        return {};
    }
    if (free_labels.Length() == 1)
    {
        return XCAFDoc_ShapeTool::GetShape(free_labels.Value(1));
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (Standard_Integer index = 1; index <= free_labels.Length(); ++index)
    {
        const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(free_labels.Value(index));
        if (!shape.IsNull())
        {
            builder.Add(compound, shape);
        }
    }
    return compound;
}

SImportedAppearance collectAppearance(const TopoDS_Shape& shape,
                                      const TDF_LabelSequence& free_labels, bool& has_texture)
{
    SImportedAppearance appearance;
    TopTools_IndexedMapOfShape final_faces;
    TopExp::MapShapes(shape, TopAbs_FACE, final_faces);
    if (final_faces.IsEmpty())
    {
        return appearance;
    }

    SAppearanceStyle default_style;
    QVector<SFaceStyleState> face_styles(final_faces.Extent());
    for (SFaceStyleState& face : face_styles)
    {
        face.style = default_style;
    }

    XCAFPrs_IndexedDataMapOfShapeStyle settings;
    for (Standard_Integer index = 1; index <= free_labels.Length(); ++index)
    {
        XCAFPrs::CollectStyleSettings(free_labels.Value(index), TopLoc_Location(), settings);
    }

    std::vector<SStyleSetting> ordered_settings;
    ordered_settings.reserve(static_cast<std::size_t>(settings.Extent()));
    for (Standard_Integer index = 1; index <= settings.Extent(); ++index)
    {
        ordered_settings.push_back({settings.FindKey(index), settings.FindFromIndex(index)});
        if (!settings.FindFromIndex(index).BaseColorTexture().IsNull())
        {
            has_texture = true;
        }
    }
    std::stable_sort(ordered_settings.begin(), ordered_settings.end(),
                     [](const SStyleSetting& first, const SStyleSetting& second)
                     {
                         return static_cast<int>(first.shape.ShapeType()) <
                                static_cast<int>(second.shape.ShapeType());
                     });

    bool has_source_style = false;
    for (const SStyleSetting& setting : ordered_settings)
    {
        SAppearanceStyle style;
        if (!appearanceStyle(setting.style, style))
        {
            continue;
        }
        TopTools_IndexedMapOfShape setting_faces;
        TopExp::MapShapes(setting.shape, TopAbs_FACE, setting_faces);
        for (Standard_Integer face_index = 1; face_index <= setting_faces.Extent(); ++face_index)
        {
            const Standard_Integer final_index =
                final_faces.FindIndex(setting_faces.FindKey(face_index));
            if (final_index > 0)
            {
                face_styles[final_index - 1] = {style, true};
                has_source_style = true;
            }
        }
    }
    if (!has_source_style)
    {
        return appearance;
    }

    appearance.valid = true;
    appearance.base_style = mostFrequentStyle(face_styles, false);
    appearance.fallback_style = mostFrequentStyle(face_styles, true);
    for (int index = 0; index < face_styles.size(); ++index)
    {
        if (!sameStyle(face_styles.at(index).style, appearance.base_style))
        {
            appearance.face_overrides.push_back({index + 1, face_styles.at(index).style});
        }
    }
    return appearance;
}

SResult<Handle(TDocStd_Document)> readDocument(const SXdeReadRequest& request)
{
    const QByteArray path = QFileInfo(request.file_path).absoluteFilePath().toUtf8();
    Handle(TDocStd_Document) document = createDocument();
    bool success = false;
    if (request.extension == QStringLiteral("step") || request.extension == QStringLiteral("stp"))
    {
        STEPCAFControl_Reader reader;
        reader.SetColorMode(Standard_True);
        reader.SetMatMode(Standard_True);
        success =
            reader.ReadFile(path.constData()) == IFSelect_RetDone && reader.Transfer(document);
    }
    else if (request.extension == QStringLiteral("iges") ||
             request.extension == QStringLiteral("igs"))
    {
        IGESCAFControl_Reader reader;
        reader.SetColorMode(Standard_True);
        success =
            reader.ReadFile(path.constData()) == IFSelect_RetDone && reader.Transfer(document);
    }
    else if (request.extension == QStringLiteral("obj"))
    {
        RWObj_CafReader reader;
        reader.SetDocument(document);
        success =
            reader.Perform(TCollection_AsciiString(path.constData()), Message_ProgressRange());
    }
    if (!success)
    {
        return SResult<Handle(TDocStd_Document)>::failure(SErrorCode::FileFailure,
                                                          QObject::tr("彩色模型读取失败"));
    }
    return SResult<Handle(TDocStd_Document)>::success(std::move(document));
}
} // namespace

SResult<SImportedShape> importXdeFile(const QString& file_path, const QString& extension,
                                      SFileCompatibilityReport report)
{
    try
    {
        const auto document_result = readDocument({file_path, extension});
        if (!document_result)
        {
            return SResult<SImportedShape>::failure(
                document_result.errorCode(), document_result.message(), document_result.details());
        }

        TDF_LabelSequence free_labels;
        const TopoDS_Shape shape = documentShape(document_result.value(), free_labels);
        SKernelShape wrapped = SKernelShapeAccess::fromNative(shape);
        if (wrapped.isNull())
        {
            return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                    QObject::tr("文件中没有可用几何"));
        }

        bool has_texture = false;
        SImportedShape result;
        result.shape = std::move(wrapped);
        result.suggested_name = QFileInfo(file_path).completeBaseName();
        result.appearance = collectAppearance(shape, free_labels, has_texture);
        result.report = std::move(report);
        if (has_texture)
        {
            result.report.warnings.push_back(
                QObject::tr("检测到纹理贴图；本版本仅保留纯色和透明度。"));
        }
        return SResult<SImportedShape>::success(std::move(result));
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                QObject::tr("彩色模型导入失败"),
                                                QString::fromLocal8Bit(failure.GetMessageString()));
    }
}
} // namespace smartGraphics3D
