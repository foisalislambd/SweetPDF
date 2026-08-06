# SweetPDF

An all-in-one, open source PDF editor for Linux, Windows and macOS. Written in
C++20 with Qt 6 and MuPDF, aiming to be a genuine alternative to Acrobat rather
than another read-only viewer.

> Status: usable. Viewing, text selection and search, markup, page organisation,
> form filling, signatures, redaction, encryption, OCR and export all work.
> Editing existing text runs is done by replacing them (redact and retype)
> rather than by re-flowing the original text.

## What it does

**Read** — open PDF, XPS, EPUB, CBZ and SVG; continuous, single and two-page
layouts; fit page or width; animated zoom and scrolling; thumbnails, bookmarks
and document properties; clickable links; tabs, recent files and session
restore; light and dark themes.

**Select and search** — character-accurate text selection with word and line
snapping, copy, select all, and a find bar with whole-word and case-sensitive
matching that highlights hits across the whole document.

**Mark up** — highlight, underline, strikeout, freehand ink, sticky notes, text
boxes, rectangles and ellipses, with a shared colour and opacity, all written as
real PDF annotations. The comments panel lists every one of them.

**Edit content** — add pictures, replace a picture you placed, add links to a URL
or another page, and replace selected text (the old glyphs are redacted away, not
just covered).

**Organise pages** — insert blank pages or pages from another file, delete,
duplicate, reorder by dragging thumbnails, rotate, extract, merge and split.

**Fill in forms** — the forms panel lists every field page by page with the right
editor for its type, values are written straight into the document, and the form
can be reset or flattened.

**Sign** — draw a signature, type your name, or import a picture of one; place it
by dragging a box or drop it on every page at once.

**Protect** — AES-128 and AES-256 encryption with per-operation permissions, true
redaction that removes the content underneath, and watermarks.

**Convert** — OCR to a searchable PDF through Tesseract, export pages as images,
plain text or HTML, print, and compress.

Every edit is one step on the undo stack, including page operations, which are
undone by restoring the document exactly as it was.

## Why these libraries

| Concern | Choice | Reason |
| --- | --- | --- |
| UI | Qt 6 Widgets | A ribbon, dock panels and a canvas with hundreds of items stay fast and feel native; QML would add a runtime and buy little for a dense desktop tool. |
| Render, text, annotations | MuPDF | One C API that covers rasterising, structured text extraction, annotations and redaction, and it is genuinely fast. |
| Structural operations | QPDF | Merge, split, encrypt, linearise and repair at the object level, which is where MuPDF is weakest. Apache-2.0. |
| OCR | Tesseract + Leptonica | The only serious open source engine, with per-word bounding boxes for an invisible text layer. |
| Local state | SQLite | Recent files, per-document session state and annotation drafts. |
| Build | CMake + Ninja | One build description for all three platforms. |

MuPDF is AGPL-3.0, so SweetPDF is AGPL-3.0 as well.

## Architecture

```
src/
  core/           Document model and rendering. Qt Core/Gui only, no widgets.
    pdf/          The MuPDF boundary: documents, pages, text, annotations,
                  forms, pictures and links, export
    render/       Bitmap cache and worker queue
    session/      Per-document state, undo commands and history
    util/         Logging categories
  ui/             Everything visual
    theme/        Colour, typography, icons, stylesheet
    ribbon/       The command surface
    viewer/       Page canvas, overlays and floating controls
    dialogs/      One modal per command: security, export, watermark, OCR,
                  signature, settings
    panels/       Sidebar, thumbnails, outline, comments, forms, properties
    window/       Main window, welcome screen
  app/            main() and platform packaging
resources/        Application icon
```

Each module has its own guide: [`src/core/README.md`](src/core/README.md) and
[`src/ui/README.md`](src/ui/README.md) explain what belongs in each folder, the
rules that keep the boundaries intact, and how a page reaches the screen.

The rules that keep this honest:

- `core` never includes a widget header, so the document model stays testable
  and reusable by a future CLI.
- Pages are rasterised on a worker pool, never on the GUI thread. Each thread
  owns a cloned MuPDF context; the base context installs locks so one document
  can serve several threads.
- Bitmaps are cached by (page, quantised scale, rotation) under a memory budget,
  and thumbnails share that cache at the lowest priority.
- Scene coordinates are PDF points, so zooming is only a view transform and
  annotations added later need no second coordinate system.
- All state for one open document lives in a `DocumentSession`: document, render
  queue, undo stack, zoom, rotation, current page and active tool. Widgets read
  from it and never keep their own copy, so the canvas, thumbnails and inspector
  cannot disagree.

## Building

### Linux (Ubuntu 24.04 or newer, Debian 13 or newer)

```bash
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build git pkg-config \
  qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools libgl1-mesa-dev \
  libmupdf-dev libqpdf-dev libtesseract-dev libleptonica-dev libsqlite3-dev \
  libfreetype-dev libharfbuzz-dev libjpeg-dev libopenjp2-7-dev libjbig2dec0-dev \
  zlib1g-dev libmujs-dev libgumbo-dev tesseract-ocr tesseract-ocr-eng

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/sweetpdf
```

### Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtsvg-devel \
  mupdf-devel qpdf-devel tesseract-devel leptonica-devel sqlite-devel
```

### Windows and macOS

Install the dependencies with [vcpkg](https://vcpkg.io) and point CMake at its
toolchain file:

```bash
vcpkg install qtbase qtsvg mupdf qpdf tesseract sqlite3
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Build options

| Option | Default | Effect |
| --- | --- | --- |
| `SWEETPDF_ENABLE_OCR` | `ON` | Build Tesseract-backed OCR. Turns itself off if Tesseract is missing. |
| `SWEETPDF_WARNINGS_AS_ERRORS` | `OFF` | Fail the build on any warning. Used in CI. |

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| Open | `Ctrl+O` |
| Save / save as | `Ctrl+S` / `Ctrl+Shift+S` |
| Print | `Ctrl+P` |
| Undo / redo | `Ctrl+Z` / `Ctrl+Shift+Z` |
| Find / next / previous | `Ctrl+F` / `F3` / `Shift+F3` |
| Copy / select all | `Ctrl+C` / `Ctrl+A` |
| Delete selected comment | `Delete` |
| Cancel placement or selection | `Esc` |
| Close document | `Ctrl+W` |
| Zoom in / out | `Ctrl++` / `Ctrl+-` |
| Actual size | `Ctrl+0` |
| Fit page / fit width | `Ctrl+1` / `Ctrl+2` |
| Select / pan tool | `V` / `H` |
| Navigation panel | `Ctrl+B` |
| Properties panel | `Ctrl+J` |
| Zoom with the wheel | `Ctrl+Wheel` |
| Pan temporarily | Middle mouse drag |

## Roadmap

1. **Viewer core** — open, render, navigate, thumbnails, bookmarks, properties. *Done.*
2. **Text selection and search** — selection layer, find bar, cross-page results. *Done.*
3. **Markup** — highlight, underline, strikeout, ink, notes, saved as real PDF annotations. *Done.*
4. **Page organisation** — insert, delete, reorder, rotate, extract, merge, split. *Done.*
5. **OCR** — Tesseract with an invisible text layer, per page or per document. *Done.*
6. **Forms and signatures** — AcroForm filling, flattening, signature stamps. *Done.*
7. **Protection** — encryption, permissions, true redaction, watermarks. *Done.*
8. **Export** — images, plain text, HTML, printing, compression. *Done.*
9. **Real text editing** — re-flow existing text runs with their original fonts,
   instead of replacing them with a text box.
10. **Cryptographic signatures** — PKCS#7 signing and validation of signature fields.
11. **Conversion** — Office formats, in and out.
12. **Attachments and layers** — the two sidebar panels that are still placeholders.

## Contributing

Issues and pull requests are welcome. Please keep `core` free of widget
dependencies, run `cmake --build build` with `SWEETPDF_WARNINGS_AS_ERRORS=ON`
before opening a pull request, and match the surrounding style.

## License

GNU Affero General Public License v3.0 or later. See [LICENSE](LICENSE).
