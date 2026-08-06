# Screenshots

Images used in the root README. Keep them current when the chrome changes.

| File | Contents |
| --- | --- |
| `viewer-dark.png` | Editor (README hero), dark, Highlight tool |
| `viewer-light.png` | Editor, light (shown under *The window*) |
| `home.png` | Welcome screen, light (optional / docs only) |
| `home-dark.png` | Welcome screen, dark (optional / docs only) |

## Regenerate

Build `sweetpdf_smoke`, then:

```bash
cd /path/to/sweetPDF

QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  ./build/tests/sample.pdf ./docs/screenshots/viewer-light.png \
  --tool=toolHighlight --size=1514x900

QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  ./build/tests/sample.pdf ./docs/screenshots/viewer-dark.png \
  --dark --tool=toolHighlight --size=1514x900

QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  ./build/tests/sample.pdf ./docs/screenshots/home.png \
  --welcome --size=1440x900

QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke \
  ./build/tests/sample.pdf ./docs/screenshots/home-dark.png \
  --welcome --dark --size=1440x900
```

The sample PDF is produced at configure/test time (needs Ghostscript). If
`build/tests/sample.pdf` is missing, run `ctest --test-dir build` once first.
