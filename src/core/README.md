# `src/core` — document model and rendering

Everything here is UI-free. `sweetpdf_core` links Qt Core, Qt Gui and Qt
Concurrent, but **never** Qt Widgets, so the model can be driven from a test, a
command line tool or a future alternative front end.

```
core/
  pdf/      the MuPDF boundary: documents, pages, text, annotations, forms,
            pictures and links, export
  render/   getting bitmaps off the GUI thread: cache and worker queue
  session/  per-document state, undo commands and the history
  util/     logging categories
```

## What lives where

| Folder | Files | Responsibility |
| --- | --- | --- |
| `pdf/` | `MuContext`, `PdfDocument`, `PdfPage`, `PageText`, `Geometry`, `DocumentOutline` | The only place `fz_*` types appear. Opens files, authenticates passwords, reads metadata and the outline, rasterises pages, and builds the character-level text layer that selection and search work from. |
| `pdf/` | `PdfAnnotations`, `PdfContent`, `PdfForms` | Editing that is not structural: markup and notes, pictures placed as stamps, links, and AcroForm reading, filling and flattening. Stateless free functions — the document is the state. |
| `pdf/` | `PdfExport` | Pages to images, plain text, HTML, and searchable PDF through MuPDF's OCR writer. |
| `render/` | `RenderKey`, `PageCache`, `RenderQueue` | Turns "I need page 4 at 1.5x" into a bitmap, off the GUI thread, with an LRU cache under a memory budget and priorities so visible pages beat thumbnails. |
| `session/` | `DocumentSession`, `DocumentCommand`, `EditCommands` | One open document as the UI sees it: current page, zoom and zoom mode, rotation, layout, active tool, markup style, modified flag, undo stack, and the commands that go on it. |
| `util/` | `Logging` | `qCDebug` categories (`sweetpdf.pdf`, `sweetpdf.render`, …) so output can be filtered per subsystem. |

## How editing is undone

There are two kinds of edit and they are undone differently.

*Annotation-level* changes — add, delete, or change a comment, fill in a form
field — happen in place on the open document, and the matching command reverses
them by doing the opposite. These are cheap, so they can be pushed per keystroke
and merged.

*Structural* changes — pages inserted, deleted, reordered, redactions applied,
the document flattened, a picture or link added — are undone by keeping the
serialised document from before and after the edit and re-opening it
(`StructuralEditCommand`). Re-opening costs a copy of the file, but it is the
only way to guarantee that no cached page, display list or queued render still
points at an object that has been removed, and it makes undo exact rather than an
attempt to reverse the operation. `DocumentSession::applyStructuralEdit` is the
single door to this: it snapshots, runs the edit, snapshots again, and rolls back
to the first snapshot if the edit throws.

## Rules that keep this layer honest

1. **No widgets.** If a file needs `QWidget`, it belongs in `src/ui`.
2. **MuPDF stays in `pdf/`.** Other folders talk to it through `PdfDocument` and
   `PdfPage`. If you need a new MuPDF capability, add a method there rather than
   including `mupdf/fitz.h` elsewhere.
3. **One parse lock per document.** MuPDF allows concurrent rendering but not
   concurrent parsing of the same document. `PdfDocument` owns a parse mutex and
   hands it to each `PdfPage`; page loading, display-list construction and text
   extraction all take it. Skipping it produces corrupt output and
   `invalid key in dict` warnings, not a clean crash.
4. **`fz_var` around anything a `fz_try` block assigns.** MuPDF's error handling
   is `longjmp`-based, so locals written inside `fz_try` must be declared with
   `fz_var` or they are undefined after `fz_catch`.
5. **Signals, not callbacks.** `DocumentSession` emits; the UI listens. Core code
   never reaches upwards.

## How a page reaches the screen

```
PageItem::paint
  -> RenderQueue::request(page, scale, rotation, priority)
       -> PageCache hit?  --> return the bitmap immediately
       -> otherwise queue a task on QThreadPool, return null
            -> PdfPage::render on a worker thread
                 -> PageCache::insert, RenderQueue::rendered(key, image)
                      -> PageItem::update, painted next frame
```

`request()` returning null is normal, not an error: the item draws a placeholder
and repaints when `rendered` arrives. Each request carries a generation counter,
so work queued for a document state that no longer exists is dropped instead of
being painted late.
