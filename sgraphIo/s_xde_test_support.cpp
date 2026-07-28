#include "s_xde_test_support.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <QFileInfo>
#include <Quantity_ColorRGBA.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

namespace smartGraphics3D
{
SResult<void> writeColoredXdeTestFixture(const QString& file_path, const QString& extension)
{
    try
    {
        Handle(TDocStd_Document) document;
        XCAFApp_Application::GetApplication()->NewDocument("BinXCAF", document);
        const Handle(XCAFDoc_ShapeTool) shape_tool =
            XCAFDoc_DocumentTool::ShapeTool(document->Main());
        const Handle(XCAFDoc_ColorTool) color_tool =
            XCAFDoc_DocumentTool::ColorTool(document->Main());
        const TopoDS_Shape shape = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
        const TDF_Label shape_label = shape_tool->AddShape(shape);
        color_tool->SetColor(
            shape_label, Quantity_ColorRGBA(Quantity_Color(0.1, 0.3, 0.9, Quantity_TOC_RGB), 0.8F),
            XCAFDoc_ColorSurf);

        int face_index = 0;
        for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
        {
            ++face_index;
            if (face_index > 2)
            {
                break;
            }
            const TDF_Label face_label = shape_tool->AddSubShape(shape_label, explorer.Current());
            const Quantity_Color color = face_index == 1
                                             ? Quantity_Color(0.9, 0.1, 0.1, Quantity_TOC_RGB)
                                             : Quantity_Color(0.1, 0.8, 0.2, Quantity_TOC_RGB);
            color_tool->SetColor(face_label, Quantity_ColorRGBA(color, 0.65F), XCAFDoc_ColorSurf);
        }

        const QByteArray path = QFileInfo(file_path).absoluteFilePath().toUtf8();
        bool success = false;
        if (extension == QStringLiteral("step"))
        {
            STEPCAFControl_Writer writer;
            success = writer.Perform(document, path.constData());
        }
        else if (extension == QStringLiteral("iges"))
        {
            IGESCAFControl_Writer writer;
            success = writer.Perform(document, path.constData());
        }
        else
        {
            return SResult<void>::failure(SErrorCode::Unsupported,
                                          QStringLiteral("测试夹具格式不受支持"));
        }
        return success ? SResult<void>::success()
                       : SResult<void>::failure(SErrorCode::FileFailure,
                                                QStringLiteral("彩色测试夹具写入失败"));
    }
    catch (const Standard_Failure& failure)
    {
        return SResult<void>::failure(SErrorCode::FileFailure,
                                      QStringLiteral("彩色测试夹具写入失败"),
                                      QString::fromLocal8Bit(failure.GetMessageString()));
    }
}
} // namespace smartGraphics3D
