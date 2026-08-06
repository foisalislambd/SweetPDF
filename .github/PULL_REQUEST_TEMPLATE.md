<!--
Thanks for the pull request. Keep one concern per pull request; two unrelated
fixes are easier to review as two.
-->

## What this changes

<!-- One or two sentences. Link the issue with "Fixes #123" if there is one. -->

## Why

<!-- The problem it solves, or the reason the old behaviour was wrong. -->

## How it was tested

<!--
What you actually ran. For example: "opened a 400 page scan, ran OCR on pages
10-20, saved incrementally, reopened and searched" - far more useful than
"works". Include the document type if it matters (scanned, encrypted, forms).
-->

## Screenshots

<!--
For any UI change, before and after. The smoke test can produce them without a
desktop session:

  QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke build/tests/sample.pdf after.png
  QT_QPA_PLATFORM=offscreen ./build/bin/sweetpdf_smoke build/tests/sample.pdf home.png --welcome --dark
-->

## Checklist

- [ ] Builds clean with `-DSWEETPDF_WARNINGS_AS_ERRORS=ON`
- [ ] `ctest --test-dir build` passes, and new core behaviour has a test in `sweetpdf_edit_test`
- [ ] `core` gained no Qt Widgets dependency
- [ ] Document-changing commands are undoable
- [ ] Colours, fonts and icons come from the theme rather than being hardcoded
- [ ] Comments explain why, not what
