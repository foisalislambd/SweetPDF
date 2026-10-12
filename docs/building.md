# Building from source

Requirements shared by all platforms: **CMake**, **Ninja**, **Qt 6** (Widgets +
SVG), **MuPDF** (1.27 or newer; 1.28.5 is what `scripts/build-mupdf.sh` installs), **SQLite**, and ideally
**Tesseract** + language data for OCR.

Ghostscript is only needed to generate the sample PDF for tests.

## Linux (Ubuntu 26.04+, Debian 14+)

```bash
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build git pkg-config \
  qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools \
  libgl1-mesa-dev libmupdf-dev libtesseract-dev libleptonica-dev libsqlite3-dev \
  libfreetype-dev libharfbuzz-dev libjpeg-dev libopenjp2-7-dev libjbig2dec0-dev \
  zlib1g-dev libbrotli-dev libmujs-dev libgumbo-dev \
  tesseract-ocr tesseract-ocr-eng ghostscript

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/bin/sweetpdf
```

Older Ubuntu MuPDF packages (for example 1.24) are missing APIs SweetPDF needs.
`./scripts/build-mupdf.sh` builds MuPDF 1.28.5 into `mupdf/install`, which
CMake prefers over the distribution package.

### Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtsvg-devel \
  mupdf-devel tesseract-devel leptonica-devel sqlite-devel ghostscript
```

## Windows (MSYS2 UCRT64)

Open the UCRT64 shell. MINGW64 no longer ships MuPDF.

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{toolchain,cmake,ninja,qt6-base,qt6-svg,qt6-tools} \
  mingw-w64-ucrt-x86_64-{libmupdf,sqlite3,tesseract-ocr,tesseract-data-eng,ghostscript}

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/sweetpdf.exe
```

## macOS

```bash
brew install cmake ninja qt mupdf tesseract sqlite ghostscript
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/sweetpdf
```

CI does not yet ship macOS packages; local builds are supported for development.

## CMake options

| Option | Default | Effect |
| --- | --- | --- |
| `SWEETPDF_ENABLE_OCR` | `ON` | Tesseract OCR; auto-disables if Tesseract is missing |
| `SWEETPDF_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors (CI turns this on for PRs) |
| `SWEETPDF_VERSION` | `0.0.0` | App and package version (set by release workflow) |

Example with CI-like flags:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSWEETPDF_WARNINGS_AS_ERRORS=ON
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

| Test | Binary | Role |
| --- | --- | --- |
| `smoke` | `sweetpdf_smoke` | Opens UI offscreen, optional screenshot |
| `editing` | `sweetpdf_edit_test` | Annotations, pages, undo, export, OCR |
| `drawing` | `sweetpdf_smoke --draw` | Ink stroke survives render |

OCR checks need English traineddata (`tesseract-ocr-eng` /
`tesseract-data-eng`). Without it the edit suite skips the OCR block instead of
crashing.

## Packaging notes

Release artifacts are produced by GitHub Actions (`.deb` / `.tar.gz` on Linux,
installer + zip on Windows). See [Development → Releases](development.md#releases-and-ci).
