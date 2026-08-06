/// Headless smoke test.
///
/// Opens a document in a real MainWindow on the offscreen platform, waits for
/// the render queue to settle and writes a screenshot. Catches the failures that
/// only appear once the whole stack runs together: MuPDF context misuse, empty
/// bitmaps, layout that collapses, and stylesheet mistakes.
///
/// Usage: sweetpdf_smoke <document> [screenshot.png] [--dark]

#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"
#include "ui/window/MainWindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QTimer>

#include <cstdio>

namespace {

void spin(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

int fail(const char *message)
{
    std::fprintf(stderr, "smoke: %s\n", message);
    return 1;
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SweetPDF"));
    QApplication::setOrganizationName(QStringLiteral("SweetPDFTest"));

    const QStringList arguments = QApplication::arguments();
    if (arguments.size() < 2)
        return fail("usage: sweetpdf_smoke <document> [screenshot.png]");

    QString documentPath;
    QString screenshotPath;
    for (const QString &argument : arguments.mid(1)) {
        if (argument.startsWith(QLatin1String("--")))
            continue;
        if (documentPath.isEmpty())
            documentPath = argument;
        else if (screenshotPath.isEmpty())
            screenshotPath = argument;
    }
    if (documentPath.isEmpty())
        return fail("usage: sweetpdf_smoke <document> [screenshot.png] [--dark]");

    // Rendering on its own first: a failure here is in core, not in the UI.
    try {
        auto document = sweet::core::PdfDocument::open(documentPath);
        if (document->pageCount() <= 0)
            return fail("the document reports no pages");

        auto page = document->page(0);
        const QImage bitmap = page->render(1.5);
        if (bitmap.isNull())
            return fail("rendering the first page produced a null image");

        std::printf("smoke: %d pages, first page rendered at %dx%d\n", document->pageCount(),
                    bitmap.width(), bitmap.height());
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "smoke: core failed: %s\n", failure.what());
        return 1;
    }

    if (arguments.contains(QStringLiteral("--dark")))
        sweet::ui::Theme::instance().setMode(sweet::ui::ThemeMode::Dark);
    sweet::ui::Theme::instance().apply();

    int status = 0;

    // Scoped so the window is destroyed while QApplication is still alive, which
    // is the only order the widgets support.
    {
        sweet::ui::MainWindow window;
        window.resize(1440, 900);
        window.show();
        spin(200);

        // The welcome screen is a screen like any other, so it can be reviewed the
        // same way instead of only being seen on a fresh install.
        const bool welcomeOnly = arguments.contains(QStringLiteral("--welcome"));

        if (!welcomeOnly && !window.openFile(documentPath)) {
            status = fail("MainWindow could not open the document");
        } else {
            // Give the worker pool time to deliver page and thumbnail bitmaps.
            spin(welcomeOnly ? 0 : 2500);

            if (!screenshotPath.isEmpty()) {
                const QPixmap shot = window.grab();
                if (shot.isNull() || !shot.save(screenshotPath)) {
                    status = fail("could not save the screenshot");
                } else {
                    std::printf("smoke: %sscreenshot written to %s\n",
                                welcomeOnly ? "welcome " : "", qUtf8Printable(screenshotPath));
                }
            }
        }
    }

    // Shutting down is part of what this test covers. Closing a document with
    // renders still in flight, and freeing themed pixmaps, both used to crash
    // after the last line of output rather than during the test itself.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    spin(0);

    if (status == 0)
        std::printf("smoke: ok\n");
    return status;
}
