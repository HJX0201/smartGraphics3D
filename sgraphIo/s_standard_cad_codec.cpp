#include "s_standard_cad_codec.h"

#include "s_kernel_shape_access.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <Message_ProgressRange.hxx>
#include <QFileInfo>
#include <RWObj_CafWriter.hxx>
#include <RWObj_TriangulationReader.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <StlAPI_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TColStd_IndexedDataMapOfStringString.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <fstream>

namespace smartGraphics3D
{
namespace
{
std::string utf8Path(const QString& path)
{
    const QByteArray utf8 = QFileInfo(path).absoluteFilePath().toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

SFileCompatibilityReport reportFor(const QString& extension, bool writing)
{
    SFileCompatibilityReport report;
    report.format = extension.toUpper();
    if (extension == QStringLiteral("step") || extension == QStringLiteral("stp") ||
        extension == QStringLiteral("iges") || extension == QStringLiteral("igs"))
    {
        report.preserved_properties = QStringList{QStringLiteral("几何"), QStringLiteral("拓扑")};
        report.lost_properties = QStringList{QStringLiteral("smartGraphics3D 操作历史"),
                                             QStringLiteral("测量与场景状态")};
    }
    else if (extension == QStringLiteral("brep"))
    {
        report.preserved_properties = QStringList{QStringLiteral("几何"), QStringLiteral("拓扑")};
        report.lost_properties =
            QStringList{QStringLiteral("名称"), QStringLiteral("颜色"), QStringLiteral("场景层级")};
    }
    else
    {
        report.preserved_properties = QStringList{QStringLiteral("三角网格")};
        report.lost_properties = QStringList{QStringLiteral("精确曲面"), QStringLiteral("拓扑"),
                                             QStringLiteral("操作历史")};
        if (writing)
        {
            report.warnings = QStringList{QStringLiteral("导出将离散化精确 CAD 曲面。")};
        }
    }
    return report;
}

SResult<SImportedShape> importedResult(const TopoDS_Shape& shape, const QString& path,
                                       const QString& extension)
{
    SKernelShape wrapped = SKernelShapeAccess::fromNative(shape);
    if (wrapped.isNull())
    {
        return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                QObject::tr("文件中没有可用几何"));
    }
    SImportedShape result;
    result.shape = std::move(wrapped);
    result.suggested_name = QFileInfo(path).completeBaseName();
    result.report = reportFor(extension, false);
    return SResult<SImportedShape>::success(std::move(result));
}
} // namespace

QStringList SStandardCadCodec::extensions() const
{
    return {QStringLiteral("step"), QStringLiteral("stp"),  QStringLiteral("iges"),
            QStringLiteral("igs"),  QStringLiteral("brep"), QStringLiteral("stl"),
            QStringLiteral("obj")};
}

SResult<SFileCompatibilityReport>
SStandardCadCodec::compatibilityReport(const QString& file_path) const
{
    const QString extension = QFileInfo(file_path).suffix().toLower();
    if (!extensions().contains(extension))
    {
        return SResult<SFileCompatibilityReport>::failure(
            SErrorCode::Unsupported, QObject::tr("不支持的导出格式：%1").arg(extension));
    }
    return SResult<SFileCompatibilityReport>::success(reportFor(extension, true));
}

SResult<SImportedShape> SStandardCadCodec::read(const QString& file_path) const
{
    if (!QFileInfo::exists(file_path))
    {
        return SResult<SImportedShape>::failure(SErrorCode::NotFound,
                                                QObject::tr("文件不存在：%1").arg(file_path));
    }
    const QString extension = QFileInfo(file_path).suffix().toLower();
    const std::string path = utf8Path(file_path);
    try
    {
        if (extension == QStringLiteral("step") || extension == QStringLiteral("stp"))
        {
            STEPControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone || reader.TransferRoots() <= 0)
            {
                return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                        QObject::tr("STEP 文件读取失败"));
            }
            return importedResult(reader.OneShape(), file_path, extension);
        }
        if (extension == QStringLiteral("iges") || extension == QStringLiteral("igs"))
        {
            IGESControl_Reader reader;
            if (reader.ReadFile(path.c_str()) != IFSelect_RetDone || reader.TransferRoots() <= 0)
            {
                return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                        QObject::tr("IGES 文件读取失败"));
            }
            return importedResult(reader.OneShape(), file_path, extension);
        }
        if (extension == QStringLiteral("brep"))
        {
            TopoDS_Shape shape;
            BRep_Builder builder;
            if (!BRepTools::Read(shape, path.c_str(), builder))
            {
                return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                        QObject::tr("BREP 文件读取失败"));
            }
            return importedResult(shape, file_path, extension);
        }
        if (extension == QStringLiteral("stl"))
        {
            TopoDS_Shape shape;
            StlAPI_Reader reader;
            if (!reader.Read(shape, path.c_str()))
            {
                return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                        QObject::tr("STL 文件读取失败"));
            }
            return importedResult(shape, file_path, extension);
        }
        if (extension == QStringLiteral("obj"))
        {
            RWObj_TriangulationReader reader;
            if (!reader.Read(TCollection_AsciiString(path.c_str()), Message_ProgressRange()))
            {
                return SResult<SImportedShape>::failure(SErrorCode::FileFailure,
                                                        QObject::tr("OBJ 文件读取失败"));
            }
            return importedResult(reader.ResultShape(), file_path, extension);
        }
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<SImportedShape>::failure(SErrorCode::FileFailure, QObject::tr("导入失败"),
                                                QString::fromLocal8Bit(failure.GetMessageString()));
    }
    return SResult<SImportedShape>::failure(SErrorCode::Unsupported,
                                            QObject::tr("不支持的文件格式：%1").arg(extension));
}

SResult<SFileCompatibilityReport> SStandardCadCodec::write(const SKernelShape& shape,
                                                           const QString& file_path) const
{
    if (shape.isNull())
    {
        return SResult<SFileCompatibilityReport>::failure(SErrorCode::InvalidArgument,
                                                          QObject::tr("没有可导出的几何"));
    }
    const QString extension = QFileInfo(file_path).suffix().toLower();
    const std::string path = utf8Path(file_path);
    const TopoDS_Shape& native_shape = SKernelShapeAccess::native(shape);
    try
    {
        bool success = false;
        if (extension == QStringLiteral("step") || extension == QStringLiteral("stp"))
        {
            STEPControl_Writer writer;
            success = writer.Transfer(native_shape, STEPControl_AsIs) == IFSelect_RetDone &&
                      writer.Write(path.c_str()) == IFSelect_RetDone;
        }
        else if (extension == QStringLiteral("iges") || extension == QStringLiteral("igs"))
        {
            IGESControl_Writer writer;
            writer.AddShape(native_shape);
            writer.ComputeModel();
            success = writer.Write(path.c_str());
        }
        else if (extension == QStringLiteral("brep"))
        {
            success = BRepTools::Write(native_shape, path.c_str());
        }
        else if (extension == QStringLiteral("stl"))
        {
            BRepMesh_IncrementalMesh mesh(native_shape, 0.1);
            if (!mesh.IsDone())
            {
                return SResult<SFileCompatibilityReport>::failure(
                    SErrorCode::GeometryFailure, QObject::tr("STL 导出网格化失败"));
            }
            StlAPI_Writer writer;
            writer.ASCIIMode() = false;
            success = writer.Write(native_shape, path.c_str());
        }
        else if (extension == QStringLiteral("obj"))
        {
            BRepMesh_IncrementalMesh mesh(native_shape, 0.1);
            if (!mesh.IsDone())
            {
                return SResult<SFileCompatibilityReport>::failure(
                    SErrorCode::GeometryFailure, QObject::tr("OBJ 导出网格化失败"));
            }
            Handle(TDocStd_Document) document;
            XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
            Handle(XCAFDoc_ShapeTool) shape_tool =
                XCAFDoc_DocumentTool::ShapeTool(document->Main());
            shape_tool->AddShape(native_shape);
            RWObj_CafWriter writer(TCollection_AsciiString(path.c_str()));
            TColStd_IndexedDataMapOfStringString file_info;
            success = writer.Perform(document, file_info, Message_ProgressRange());
        }
        else
        {
            return SResult<SFileCompatibilityReport>::failure(
                SErrorCode::Unsupported, QObject::tr("不支持的导出格式：%1").arg(extension));
        }

        if (!success)
        {
            return SResult<SFileCompatibilityReport>::failure(SErrorCode::FileFailure,
                                                              QObject::tr("导出文件失败"));
        }
        return SResult<SFileCompatibilityReport>::success(reportFor(extension, true));
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<SFileCompatibilityReport>::failure(
            SErrorCode::FileFailure, QObject::tr("导出失败"),
            QString::fromLocal8Bit(failure.GetMessageString()));
    }
}
} // namespace smartGraphics3D
