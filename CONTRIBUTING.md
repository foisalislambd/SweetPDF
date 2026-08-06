# Contributing to SweetPDF

Thanks for taking the time. SweetPDF is a desktop PDF editor in C++20, so most
contributions touch either the MuPDF boundary in `src/core` or a widget in
`src/ui`. This page explains how to get a build going, what the review looks
for, and where things live.

By contributing you agree that your work is licensed under the
[AGPL-3.0-or-later](LICENSE), the same terms as the rest of the project.

## Ways to help

- **Report a bug.** Use the [bug report
  form](https://github.com/foisalislambd/sweetPDF/issues/new?template=bug_report.yml).
  A document that reproduces the problem is worth more than any description, so
  attach one when you can share it.
- **Suggest a feature.** Check the roadmap in the [README](README.md#roadmap)
  first; several big items are already planned and it helps to know if you want
  to take one on.
- **Fix or build something.** Small fixes can go straight to a pull request. For
  anything larger, open an issue first so we can agree on the approach before
  you spend an evening on it.
- **Improve the guide.** If something in the README misled you, that is a bug in
  the README.

## Getting a build

Install the dependencies for your platform from
[Build from source](README.md#build-from-source), then:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSWEETPDF_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/sweetpdf
```

`SWEETPDF_WARNINGS_AS_ERRORS=ON` is what CI uses for pull requests, so turning
it on locally saves a round trip. The tests need Ghostscript to generate the
sample document; without it they are skipped rather than failed.

Two test binaries do the checking:

- `sweetpdf_edit_test` drives `core` directly. This is where a bug would corrupt
  a document rather than merely look wrong, so new core behaviour belongs here.
- `sweetpdf_smoke` builds the real window offscreen and can write a screenshot:

  ```bash
  QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke build/tests/sample.pdf shot.png
  QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke build/tests/sample.pdf home.png --welcome --dark
  ```

  Attaching a before/after screenshot to a UI pull request makes review much
  quicker.

## Where code goes

```
src/core/pdf/       The MuPDF boundary: documents, pages, text, annotations,
                    forms, content, export
src/core/render/    Bitmap cache and the worker queue
src/core/session/   Per-document state and undo commands
src/ui/theme/       Palette, typography, icons, stylesheet
src/ui/ribbon/      The command surface
src/ui/viewer/      Page canvas, overlays, floating controls
src/ui/panels/      Sidebar panels
src/ui/dialogs/     One modal per command
src/ui/window/      Main window and home screen
```

[`src/core/README.md`](src/core/README.md) and
[`src/ui/README.md`](src/ui/README.md) go into more detail, including how a page
reaches the screen.

Rules that reviews will hold you to:

- **`core` never includes a widget header.** It may use Qt Core and Qt Gui
  (`QImage`, `QRectF`), never Qt Widgets. This keeps the document model testable
  and reusable.
- **No PDF work on the GUI thread.** Rasterising goes through the render queue.
  Each worker owns a cloned MuPDF context.
- **Every MuPDF call is wrapped.** Use `fz_try`/`fz_catch`, and `fz_var` for any
  local a `longjmp` could clobber, or the compiler will warn and CI will fail.
- **Edits are undoable.** A new document-changing command needs a `QUndoCommand`,
  so `Ctrl+Z` keeps working. Structural changes use the snapshot command.
- **Widgets read state from `DocumentSession`** rather than keeping a copy, so
  the canvas, thumbnails and inspector cannot disagree.
- **Colours and fonts come from the theme.** Never hardcode a colour; add a
  palette entry or use an existing one, and use `TextRole` instead of point
  sizes. New icons go in `IconLibrary` so they tint with the theme.

## Style

The code aims to read like one person wrote it:

- 4 spaces, no tabs, roughly 100 columns.
- Braces on their own line for functions, classes and namespaces of substance;
  attached for `if`, `for` and `while`.
- `m_` for members, camelCase for functions and variables, PascalCase for types.
- Prefer `const`, prefer early returns, prefer standard containers at the API
  boundary only when Qt has no equivalent.
- A `.clang-format` file is in the repository. It is not enforced by CI, so
  please only format the lines you touch rather than reflowing whole files.

Comments explain **why**, not what. A comment that restates the next line will
be removed in review; a comment that records a constraint, a trade-off, or a
MuPDF quirk is welcome and often requested.

## Commits and pull requests

- Write commit subjects in the imperative mood: "Fix crash when saving a
  password-protected file".
- Keep one concern per pull request. Two unrelated fixes are two pull requests.
- Say what you tested. "Opened a 400 page scan, ran OCR, saved, reopened" tells
  a reviewer far more than "works".
- CI has to be green. It builds and tests Linux and Windows with warnings as
  errors; a failure on either platform blocks the merge.

Pushing to `main` publishes a release, so pull requests are the normal route
even for maintainers. Version numbers are worked out automatically — never edit
a version by hand. When a change belongs on `main` but does not deserve a release
of its own, put `[skip release]` in the commit message: the build and the tests
still run, only the tag and the release are skipped. See [Releases and
CI](README.md#releases-and-ci).

## Questions

Open a [discussion or an
issue](https://github.com/foisalislambd/sweetPDF/issues/new/choose). Uncertainty
about the right approach is a good reason to ask before writing code, not after.

Everyone taking part is expected to follow the
[Code of Conduct](CODE_OF_CONDUCT.md).
