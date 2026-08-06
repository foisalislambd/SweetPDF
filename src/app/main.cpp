#include "ui/theme/Theme.h"
#include "ui/window/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SweetPDF"));
    QApplication::setApplicationDisplayName(QStringLiteral("SweetPDF"));
    QApplication::setApplicationVersion(QStringLiteral(SWEETPDF_VERSION));
    QApplication::setOrganizationName(QStringLiteral("SweetPDF"));
    QApplication::setOrganizationDomain(QStringLiteral("sweetpdf.org"));
    QApplication::setDesktopFileName(QStringLiteral("org.sweetpdf.SweetPDF"));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/icons/org.sweetpdf.SweetPDF.png")));

    // Fusion is the one style available on every platform, which keeps the
    // stylesheet predictable instead of fighting each platform's native style.
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("Fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "All-in-one open source PDF editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QCoreApplication::translate("main", "files"),
        QCoreApplication::translate("main", "Documents to open."),
        QStringLiteral("[files...]"));
    parser.process(app);

    sweet::ui::Theme::instance().apply();

    sweet::ui::MainWindow window;
    window.show();

    for (const QString &path : parser.positionalArguments())
        window.openFile(path);

    return QApplication::exec();
}
