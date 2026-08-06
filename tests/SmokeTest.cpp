/// Headless smoke test.
///
/// Opens a document in a real MainWindow on the offscreen platform, waits for
/// the render queue to settle and writes a screenshot. Catches the failures that
/// only appear once the whole stack runs together: MuPDF context misuse, empty
/// bitmaps, layout that collapses, and stylesheet mistakes.
///
/// Usage: sweetpdf_smoke <document> [screenshot.png] [--dark]

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfDocument.h"
#include "core/session/DocumentSession.h"
#include "ui/theme/Theme.h"
#include "ui/viewer/PageView.h"
#include "ui/window/MainWindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QMouseEvent>
#include <QTimer>

#include <algorithm>
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

void sendMouse(QWidget *target, QEvent::Type type, const QPointF &pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, target->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

/// Draws a short stroke with the pen and reports whether it stuck: an ink
/// annotation on the page, and a page bitmap that changed because of it.
bool drawCheck(sweet::ui::MainWindow *window)
{
    auto *view = window->findChild<sweet::ui::PageView *>();
    if (!view)
        return fail("no page view to draw on") == 0;

    sweet::core::DocumentSession *session = view->session();
    const int before = sweet::core::annotations::onPage(*session->document(), 0).size();

    session->setTool(sweet::core::Tool::Draw);
    spin(50);

    QWidget *canvas = view->viewport();
    // Down the middle of the viewport, where the first page is bound to be.
    const QPointF start(canvas->width() / 2.0 - 60, canvas->height() / 2.0);
    sendMouse(canvas, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 6; ++step) {
        sendMouse(canvas, QEvent::MouseMove, start + QPointF(step * 15, (step % 2) * 20 - 10),
                  Qt::NoButton, Qt::LeftButton);
    }
    sendMouse(canvas, QEvent::MouseButtonRelease, start + QPointF(90, 0), Qt::LeftButton,
              Qt::NoButton);
    spin(1500);

    const QList<sweet::core::Annotation> after =
        sweet::core::annotations::onPage(*session->document(), 0);
    if (after.size() != before + 1) {
        std::fprintf(stderr, "smoke: the stroke was not committed (%lld annotations, expected %d)\n",
                     static_cast<long long>(after.size()), before + 1);
        return false;
    }

    const bool isInk = std::any_of(after.cbegin(), after.cend(),
                                   [](const sweet::core::Annotation &annotation) {
                                       return annotation.type
                                           == sweet::core::AnnotationType::Ink;
                                   });
    if (!isInk) {
        std::fprintf(stderr, "smoke: the committed annotation is not ink\n");
        return false;
    }

    // And it has to reach the screen. Committing the stroke but serving the
    // viewer a bitmap cached from before the edit is exactly how this looked when
    // it was broken: the ink showed under the pointer and disappeared on release.
    session->setTool(sweet::core::Tool::Select);
    spin(100);
    const QImage shown = view->grab().toImage();
    const QImage expected = session->document()->page(0)->render(1.0);
    if (shown.isNull() || expected.isNull())
        return false;

    const QColor ink = session->markupColor();
    const auto carriesInk = [&ink](const QImage &image) {
        // The pen colour is unlike anything on a page of black text, so counting
        // pixels near it is enough to tell a drawn page from a clean one.
        int matches = 0;
        for (int y = 0; y < image.height(); y += 2) {
            for (int x = 0; x < image.width(); x += 2) {
                const QColor pixel = image.pixelColor(x, y);
                if (std::abs(pixel.red() - ink.red()) < 60
                    && std::abs(pixel.green() - ink.green()) < 60
                    && std::abs(pixel.blue() - ink.blue()) < 60) {
                    ++matches;
                }
            }
        }
        return matches > 20;
    };

    if (!carriesInk(expected)) {
        std::fprintf(stderr, "smoke: the stroke is not in the rendered page\n");
        return false;
    }
    if (!carriesInk(shown)) {
        std::fprintf(stderr, "smoke: the stroke is in the document but not on screen\n");
        return false;
    }

    std::printf("smoke: the stroke was committed and is on screen\n");
    return true;
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
    // Lets a screenshot show a tool's settings, which only appear once that tool
    // is picked: --tool=toolHighlight, --tool=toolDraw, and so on.
    QString toolId;
    QSize windowSize(1440, 900);
    for (const QString &argument : arguments.mid(1)) {
        if (argument.startsWith(QLatin1String("--tool="))) {
            toolId = argument.section(QLatin1Char('='), 1);
            continue;
        }
        if (argument.startsWith(QLatin1String("--size="))) {
            const QStringList parts = argument.section(QLatin1Char('='), 1).split(QLatin1Char('x'));
            if (parts.size() == 2) {
                const int width = parts[0].toInt();
                const int height = parts[1].toInt();
                if (width >= 640 && height >= 480)
                    windowSize = QSize(width, height);
            }
            continue;
        }
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
        window.resize(windowSize);
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

            if (!toolId.isEmpty()) {
                window.runHomeTask(toolId);
                spin(200);
            }

            // Drawing with the pointer is the one thing no other test covers, and
            // the one users notice first when it breaks: the stroke has to end up
            // in the document and in the next bitmap of the page, not just in the
            // preview overlay.
            if (arguments.contains(QStringLiteral("--draw")) && !drawCheck(&window))
                status = fail("drawing did not survive the release");

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
