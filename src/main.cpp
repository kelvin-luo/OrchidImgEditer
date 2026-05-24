#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QStyleFactory>
#include <QIcon>

namespace {

QIcon applicationIcon() {
    QIcon icon(QStringLiteral(":/icons/app.ico"));
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/icons/app.svg"));
    return icon;
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("kelvin"));
    QApplication::setApplicationName(QStringLiteral("kImgEdit"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setWindowIcon(applicationIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "kImgEdit - lightweight image annotation tool"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("image"),
                                 QStringLiteral("Optional image file to open"));
    parser.process(app);

    MainWindow win;
    win.show();

    QString initial;
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        initial = args.first();
    } else {
        const QString def = QStringLiteral("D:/media/xi_an_hot/w700d1q75.jpg");
        if (QFileInfo::exists(def)) initial = def;
    }
    win.openInitial(initial);

    return app.exec();
}
