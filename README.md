# SweetPDF

An all-in-one, open source PDF editor for Linux, Windows and macOS. Written in
C++20 with Qt 6 and MuPDF, aiming to be a genuine alternative to Acrobat rather
than another read-only viewer.

> Status: early development. The viewer core (open, render, navigate, zoom,
> rotate, thumbnails, bookmarks, document properties) works. Editing, markup,
> OCR and the conversion tools are scaffolded in the UI and marked as planned
> until they are implemented.

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
    pdf/          The MuPDF boundary: documents, pages, rasterising, text
    render/       Bitmap cache and worker queue
    session/      Per-document state and undo history
    util/         Logging categories
  ui/             Everything visual
    theme/        Colour, typography, icons, stylesheet
    ribbon/       The command surface
    viewer/       Page canvas and floating controls
    panels/       Sidebar, thumbnails, outline, properties
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
2. **Text selection and search** — selection layer, find bar, cross-page results.
3. **Markup** — highlight, underline, strikeout, ink, notes, saved as real PDF annotations.
4. **Page organisation** — insert, delete, reorder, rotate, extract, merge, split via QPDF.
5. **Content editing** — edit existing text runs and images, add text boxes and images.
6. **OCR** — Tesseract with an invisible text layer, per page or per document.
7. **Forms and signatures** — AcroForm filling, appearance streams, signing.
8. **Protection** — encryption, permissions, true redaction.
9. **Conversion** — export to image, HTML and Office formats.

## Contributing

Issues and pull requests are welcome. Please keep `core` free of widget
dependencies, run `cmake --build build` with `SWEETPDF_WARNINGS_AS_ERRORS=ON`
before opening a pull request, and match the surrounding style.

## License

GNU Affero General Public License v3.0 or later. See [LICENSE](LICENSE).
