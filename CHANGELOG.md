# Changelog

Notable changes, newest first. The format follows [Keep a
Changelog](https://keepachangelog.com/en/1.1.0/).

Version numbers are produced by `scripts/next-version.sh`: the first release is
`1.0.0` and every component stops at nine, so `1.0.9` is followed by `1.1.0`.
GitHub release notes are generated from commits; this file records the changes
worth knowing about as a user.

## Unreleased

Everything below is in `main` and will go out with the first tagged release.

### Added

- **Viewer** — PDF, XPS, EPUB, CBZ and SVG; single, continuous and two-page
  layouts; fit page and fit width; animated zoom and scrolling; thumbnails,
  bookmarks and document properties; clickable links; tabs, recent files and
  session restore; light and dark themes.
- **Text** — character-accurate selection with word and line snapping, copy,
  select all, and a find bar with whole-word and case-sensitive matching that
  highlights every match in the document.
- **Markup** — highlight, underline, strikeout, freehand ink, sticky notes, text
  boxes, rectangles and ellipses with a shared colour and opacity, all saved as
  real PDF annotations, plus a panel that lists them.
- **Pages** — insert blank pages or pages from a file, delete, duplicate,
  reorder by dragging thumbnails, rotate, extract, merge and split.
- **Content editing** — add pictures, replace a picture you placed, add links to
  a URL or another page, and replace selected text by redacting the old glyphs
  and typing over them.
- **Forms** — a panel listing every field with the right editor for its type,
  values written straight into the document, reset, and flattening.
- **Signatures** — draw, type or import a signature and place it on one page or
  all of them.
- **Protection** — AES-128 and AES-256 encryption with per-operation
  permissions, true redaction that removes the content underneath, and
  watermarks.
- **Conversion** — OCR to a searchable PDF through Tesseract, export pages as
  images, plain text or HTML, printing, and compression.
- **Home screen** — task tiles, a recent-files list with filtering, and a short
  guide. The ribbon and side panels are hidden while no document is open.
- **Undo** — every edit is one step on the undo stack, including page
  operations, which are undone by restoring the document as it was.
- **Packaging and CI** — `.deb` and `.tar.gz` for Linux and a portable `.zip`
  for Windows, built and released automatically once every platform passes.

### Known limitations

- Editing existing text replaces it rather than re-flowing the original runs
  with their fonts.
- Cryptographic (PKCS#7) signature signing and validation is not implemented.
- The attachments and layers panels are placeholders.
- macOS builds are not produced by CI yet; build from source in the meantime.
