# Development

Practical notes for contributors. Licensing and review expectations live in
[CONTRIBUTING.md](../CONTRIBUTING.md).

## Local loop

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSWEETPDF_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/sweetpdf
```

## Where to change what

| Goal | Start here |
| --- | --- |
| MuPDF open/save/render/text | `src/core/pdf/` |
| Annotations / forms / images / links | `PdfAnnotations`, `PdfForms`, `PdfContent` |
| OCR / export | `PdfExport` |
| Undo / tools / modified flag | `src/core/session/` |
| Ribbon layout and tabs | `src/ui/ribbon/` |
| Theme colours / stylesheet | `src/ui/theme/Theme.cpp` |
| Home screen | `src/ui/window/WelcomeView.*` |
| Page interaction / inline text edit | `src/ui/viewer/PageView.*` |
| CI / packages | `.github/workflows/`, `packaging/` |

## Tests and screenshots

```bash
# Full suite
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

# UI screenshot (light document view)
QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  build/tests/sample.pdf docs/screenshots/viewer-light.png \
  --tool=toolHighlight --size=1514x900

# Home screen
QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  build/tests/sample.pdf docs/screenshots/home.png --welcome --size=1440x900
```

See [screenshots/README.md](screenshots/README.md) for the full set.

## Releases and CI

| Workflow | When | What |
| --- | --- | --- |
| `ci.yml` | PRs and non-`main` pushes | Build + test, warnings as errors, no publish |
| `release.yml` | Push to `main` (or manual) | Version, multi-OS build, then tag + GitHub release |

Put `[skip release]` in a `main` commit message to land docs/fixes without a
new tag. Manual Actions runs always release.

Version bumps use `scripts/next-version.sh` (components roll 0–9).

Artifacts: Linux `.deb` + `.tar.gz`, Windows `-setup.exe` + portable `.zip`,
plus `SHA256SUMS`. macOS packaging is not enabled in the matrix yet.

## Security reports

Do not open public issues for vulnerabilities. Follow [SECURITY.md](../SECURITY.md).
