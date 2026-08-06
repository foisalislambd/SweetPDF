/// Editing test, no UI involved.
///
/// Exercises the paths where a mistake corrupts a document rather than just
/// looking wrong: annotations survive a save/reopen round trip, page operations
/// keep the page tree consistent, structural undo restores exactly what was
/// there, and export produces real files.
///
/// Usage: sweetpdf_edit_test <document> [workdir]

#include "core/pdf/PdfAnnotations.h"
#include "core/pdf/PdfContent.h"
#include "core/pdf/PdfDocument.h"
#include "core/pdf/PdfExport.h"
#include "core/pdf/PdfForms.h"
#include "core/session/DocumentSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdio>

using namespace sweet::core;

namespace {

int failures = 0;

void check(bool condition, const char *what)
{
    if (condition) {
        std::printf("  ok    %s\n", what);
        return;
    }
    std::printf("  FAIL  %s\n", what);
    ++failures;
}

template <typename Value>
void checkEqual(Value actual, Value expected, const char *what)
{
    if (actual == expected) {
        std::printf("  ok    %s\n", what);
        return;
    }
    std::printf("  FAIL  %s (got %lld, expected %lld)\n", what, static_cast<long long>(actual),
                static_cast<long long>(expected));
    ++failures;
}

AnnotationSpec highlightSpec(int pageIndex)
{
    AnnotationSpec spec;
    spec.type = AnnotationType::Highlight;
    spec.pageIndex = pageIndex;
    spec.quads = { QRectF(72, 90, 220, 14) };
    spec.color = QColor(0xFF, 0xDD, 0x4A);
    spec.opacity = 0.45;
    spec.contents = QStringLiteral("test highlight");
    spec.author = QStringLiteral("SweetPDF test");
    return spec;
}

AnnotationSpec inkSpec(int pageIndex)
{
    AnnotationSpec spec;
    spec.type = AnnotationType::Ink;
    spec.pageIndex = pageIndex;
    spec.color = QColor(0xE0, 0x3C, 0x3C);
    spec.borderWidth = 2.0;
    spec.strokes = { QPolygonF({ QPointF(100, 300), QPointF(140, 330), QPointF(180, 300) }) };
    return spec;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() < 2) {
        std::fprintf(stderr, "usage: sweetpdf_edit_test <document> [workdir]\n");
        return 2;
    }

    const QString source = arguments.at(1);
    QTemporaryDir fallback;
    const QString workDir = arguments.size() >= 3 ? arguments.at(2) : fallback.path();

    try {
        // --- Annotations survive a save and reopen ------------------------
        std::printf("annotations\n");
        {
            auto document = PdfDocument::open(source);
            check(document->isPdf(), "the sample opens as a PDF");
            const int pages = document->pageCount();
            check(pages > 1, "the sample has more than one page");

            // The page is held and rendered before the edit, exactly as the viewer
            // does. MuPDF binds annotations to the page object at load time, so a
            // page kept across an edit used to keep rendering the old appearance:
            // a drawing appeared while the pointer was down and vanished as soon
            // as the real render replaced the preview.
            auto heldPage = document->page(0);
            const QImage before = heldPage->render(1.0);
            check(!before.isNull(), "the page renders before the edit");

            const int highlight = annotations::add(*document, highlightSpec(0));
            const int ink = annotations::add(*document, inkSpec(0));
            check(highlight != 0, "a highlight can be created");
            check(ink != 0, "an ink annotation can be created");

            const QImage after = heldPage->render(1.0);
            check(!after.isNull(), "the page still renders after the edit");
            check(after != before, "a new annotation shows up in the very next render");
            checkEqual(annotations::onPage(*document, 0).size(), qsizetype(2),
                       "both annotations are listed");

            const QString saved = QDir(workDir).filePath(QStringLiteral("annotated.pdf"));
            document->saveToFile(saved);
            check(QFileInfo(saved).size() > 0, "the annotated document is written");

            auto reopened = PdfDocument::open(saved);
            const QList<Annotation> found = annotations::onPage(*reopened, 0);
            checkEqual(found.size(), qsizetype(2), "annotations survive a reopen");

            bool foundHighlight = false;
            bool foundInk = false;
            for (const Annotation &annotation : found) {
                if (annotation.type == AnnotationType::Highlight) {
                    foundHighlight = true;
                    check(!annotation.quads.isEmpty(), "the highlight keeps its quads");
                    check(annotation.contents == QStringLiteral("test highlight"),
                          "the highlight keeps its note text");
                } else if (annotation.type == AnnotationType::Ink) {
                    foundInk = true;
                    checkEqual(annotation.strokes.size(), qsizetype(1),
                               "the drawing keeps its stroke");
                }
            }
            check(foundHighlight, "the highlight is recognised after reopening");
            check(foundInk, "the drawing is recognised after reopening");

            check(annotations::remove(*reopened, 0, found.first().objectNumber),
                  "an annotation can be deleted");
            checkEqual(annotations::onPage(*reopened, 0).size(), qsizetype(1),
                       "the deletion is reflected in the list");
        }

        // --- Page operations ----------------------------------------------
        std::printf("pages\n");
        {
            auto document = PdfDocument::open(source);
            const int original = document->pageCount();

            document->setPageRotation(1, 90);
            checkEqual(document->pageRotation(1), 90, "a page can be rotated");

            document->insertBlankPage(1, QSizeF(612, 792));
            checkEqual(document->pageCount(), original, "insert needs a reopen to be visible");

            auto grown = PdfDocument::openFromBytes(document->toBytes(), QString());
            checkEqual(grown->pageCount(), original + 1, "the inserted page is there");
            checkEqual(grown->pageRotation(2), 90, "rotation followed the moved page");

            grown->duplicatePage(0);
            auto duplicated = PdfDocument::openFromBytes(grown->toBytes(), QString());
            checkEqual(duplicated->pageCount(), original + 2, "a page can be duplicated");

            duplicated->deletePages({ 0, 1 });
            auto trimmed = PdfDocument::openFromBytes(duplicated->toBytes(), QString());
            checkEqual(trimmed->pageCount(), original, "pages can be deleted");

            trimmed->movePage(0, trimmed->pageCount() - 1);
            auto moved = PdfDocument::openFromBytes(trimmed->toBytes(), QString());
            checkEqual(moved->pageCount(), original, "moving keeps the page count");

            const QByteArray extracted = moved->extractPages({ 0, 1 });
            auto slice = PdfDocument::openFromBytes(extracted, QString());
            checkEqual(slice->pageCount(), 2, "pages can be extracted into a new document");

            const int before = moved->pageCount();
            moved->appendPagesFrom(source);
            auto merged = PdfDocument::openFromBytes(moved->toBytes(), QString());
            checkEqual(merged->pageCount(), before + original, "documents can be merged");
        }

        // --- Structural undo through the session --------------------------
        std::printf("undo\n");
        {
            DocumentSession session;
            QString error;
            check(session.load(source, QString(), &error), "the session opens the document");
            const int original = session.pageCount();

            QString editError;
            const bool edited = session.applyStructuralEdit(
                QStringLiteral("Delete page"),
                [](PdfDocument &document) { document.deletePages({ 0 }); }, &editError);
            check(edited, "a structural edit runs");
            checkEqual(session.pageCount(), original - 1, "the page is gone");
            check(session.isModified(), "the document is marked modified");

            session.undoStack()->undo();
            checkEqual(session.pageCount(), original, "undo brings the page back");
            session.undoStack()->redo();
            checkEqual(session.pageCount(), original - 1, "redo removes it again");

            // Annotations go through the same stack.
            session.undoStack()->undo();
            check(session.addAnnotation(highlightSpec(0)), "an annotation is added undoably");
            checkEqual(session.annotationsOnPage(0).size(), qsizetype(1), "the annotation is there");
            session.undoStack()->undo();
            checkEqual(session.annotationsOnPage(0).size(), qsizetype(0), "undo removes it");
            session.undoStack()->redo();
            checkEqual(session.annotationsOnPage(0).size(), qsizetype(1), "redo puts it back");

            const QString saved = QDir(workDir).filePath(QStringLiteral("session-save.pdf"));
            SaveOptions options;
            options.garbageCollect = true;
            check(session.saveAs(saved, options, false, &error), "save as writes the document");
            check(!session.isModified(), "saving clears the modified flag");
        }

        // --- Encryption ---------------------------------------------------
        std::printf("encryption\n");
        {
            auto document = PdfDocument::open(source);
            const QString locked = QDir(workDir).filePath(QStringLiteral("locked.pdf"));

            SaveOptions options;
            options.encryption = SaveOptions::Encryption::Aes256;
            options.userPassword = QStringLiteral("open-me");
            options.ownerPassword = QStringLiteral("owner");
            options.permissions.copy = false;
            options.permissions.modify = false;
            document->saveToFile(locked, options);

            check(PdfDocument::needsPassword(locked), "the saved document asks for a password");
            auto unlocked = PdfDocument::open(locked, QStringLiteral("open-me"));
            check(unlocked->pageCount() == document->pageCount(),
                  "the password opens the document");
            check(!unlocked->permissions().copy, "copying is denied as requested");
        }

        // --- Export -------------------------------------------------------
        std::printf("export\n");
        {
            auto document = PdfDocument::open(source);

            const QStringList images = exporter::pagesToImages(*document, { 0 }, workDir,
                                                               QStringLiteral("page"),
                                                               QStringLiteral("png"), 96);
            checkEqual(images.size(), qsizetype(1), "a page is exported as an image");
            check(QFileInfo(images.value(0)).size() > 0, "the image file has content");

            const QString text = exporter::documentText(*document);
            check(text.contains(QStringLiteral("SweetPDF")), "the text export contains page text");

            const QString html = QDir(workDir).filePath(QStringLiteral("export.html"));
            exporter::toHtml(*document, html);
            check(QFileInfo(html).size() > 0, "an HTML export is written");
        }

        // --- OCR produces a searchable PDF --------------------------------
        std::printf("ocr\n");
        {
            auto document = PdfDocument::open(source);
            const QString tessdata = exporter::defaultTessdataDir();
            check(!tessdata.isEmpty(), "Tesseract language data is installed");
            check(exporter::installedOcrLanguages(tessdata).contains(QStringLiteral("eng")),
                  "the English language pack is available");

            const QString searchable = QDir(workDir).filePath(QStringLiteral("searchable.pdf"));
            exporter::toSearchablePdf(*document, searchable, QStringLiteral("eng"), tessdata, 150);
            check(QFileInfo(searchable).size() > 1000, "OCR writes a searchable PDF");

            auto reopened = PdfDocument::open(searchable);
            const QString text = exporter::documentText(*reopened);
            check(!text.trimmed().isEmpty(), "the OCR PDF has a selectable text layer");
        }

        // --- Pictures and links -------------------------------------------
        std::printf("content\n");
        {
            auto document = PdfDocument::open(source);

            QImage picture(48, 24, QImage::Format_ARGB32_Premultiplied);
            picture.fill(QColor(0x2E, 0x6B, 0xFF, 0xC0));
            const int stamp =
                content::addImage(*document, 0, picture, QRectF(96, 400, 96, 48));
            check(stamp != 0, "a picture can be placed on a page");

            check(content::addLink(*document, 0, QRectF(72, 500, 180, 16),
                                   QStringLiteral("https://example.org")),
                  "an external link can be added");
            const QString internal = content::internalUri(*document, 1);
            check(!internal.isEmpty(), "an internal destination can be described");
            check(content::addLink(*document, 0, QRectF(72, 530, 180, 16), internal),
                  "a link to another page can be added");

            const QString saved = QDir(workDir).filePath(QStringLiteral("with-content.pdf"));
            document->saveToFile(saved);
            auto reopened = PdfDocument::open(saved);

            int stamps = 0;
            for (const Annotation &annotation : annotations::onPage(*reopened, 0)) {
                if (annotation.type == AnnotationType::Stamp)
                    ++stamps;
            }
            checkEqual(stamps, 1, "the picture survives a reopen");

            const QList<content::Link> links = content::linksOnPage(*reopened, 0);
            checkEqual(links.size(), qsizetype(2), "both links survive a reopen");
            check(std::any_of(links.cbegin(), links.cend(),
                              [](const content::Link &link) {
                                  return link.uri == QStringLiteral("https://example.org");
                              }),
                  "the external link keeps its address");

            check(content::removeLinkAt(*reopened, 0, QPointF(90, 508)),
                  "a link can be removed by pointing at it");
            checkEqual(content::linksOnPage(*reopened, 0).size(), qsizetype(1),
                       "the removal leaves the other link alone");

            const QRectF fitted =
                content::fitToAspect(QSizeF(200, 100), QRectF(0, 0, 100, 100));
            check(qFuzzyCompare(fitted.width(), 100.0) && qFuzzyCompare(fitted.height(), 50.0),
                  "fitting to aspect keeps the picture's proportions");
        }

        // --- Forms ---------------------------------------------------------
        std::printf("forms\n");
        {
            // The sample carries no form, so the interesting checks are that
            // reading one is safe and that flattening a document keeps it valid.
            auto document = PdfDocument::open(source);
            check(forms::all(*document).isEmpty(), "a document without a form lists no fields");
            check(!forms::hasFields(*document), "and reports that it has none");

            annotations::add(*document, highlightSpec(0));
            check(forms::flatten(*document, true, true), "a document can be flattened");

            auto flattened = PdfDocument::openFromBytes(document->toBytes(), QString());
            checkEqual(flattened->pageCount(), document->pageCount(),
                       "flattening keeps every page");
            checkEqual(annotations::onPage(*flattened, 0).size(), qsizetype(0),
                       "the flattened annotation is no longer interactive");
        }
    } catch (const std::exception &failure) {
        std::fprintf(stderr, "edit test threw: %s\n", failure.what());
        return 1;
    }

    std::printf(failures == 0 ? "all edit checks passed\n" : "%d check(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
