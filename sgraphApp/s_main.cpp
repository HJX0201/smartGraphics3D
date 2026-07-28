#include "s_icon_factory.h"
#include "s_main_window.h"
#include "s_version.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QSurfaceFormat>

int main(int argument_count, char* argument_values[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argument_count, argument_values);
    QCoreApplication::setOrganizationName(QStringLiteral("smartGraphics3D"));
    QCoreApplication::setApplicationName(QStringLiteral("smartGraphics3D"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(SMARTGRAPHICS3D_VERSION));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    QApplication::setWindowIcon(smartGraphics3D::applicationIcon());

    smartGraphics3D::SMainWindow window;
    window.showMaximized();
    return QApplication::exec();
}
