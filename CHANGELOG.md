# Changelog

Notable changes, newest first. The format follows [Keep a
Changelog](https://keepachangelog.com/en/1.1.0/).

The number in [`version.md`](version.md) is what a push to `main` releases, and
only when it is greater than the newest tag. GitHub release notes are generated
from commits; this file records the changes worth knowing about as a user.

## Unreleased

Everything below is in `main` and will go out with the first tagged release.

### Added

- **MuPDF 1.28.6** — `scripts/build-mupdf.sh` now builds MuPDF 1.28.6 (previously 1.28.5).
  The upstream change in 1.28.6 is the vendored Brotli 1.2.1. The script still
  links system libraries, so that Brotli bump applies when MuPDF is built
  without `USE_SYSTEM_LIBS`. The same script now installs `libmupdf-pkcs7` and
  the OpenSSL helper headers, which certificate signing links when OpenSSL is
  present. Saves can ask MuPDF 1.28.1+ for a reproducible file id, and compress
  uses the highest compression effort.
- **Certificate signatures** — Protect → Certificate signs the current page with
  a PKCS#12 file. The forms panel names the signer and treats a matching
  self-signed certificate as signed rather than simply invalid. Builds that
  only have a distribution MuPDF, without the PKCS#7 helper, keep the previous
  behaviour and say so.
- **Search and redact** — the find bar and Protect → Redact Matches mark every
  hit, then Apply Redactions removes the text.
- **Bookmarks** — Move up and Move down reorder an entry among its siblings and
  take its children along. The move is one undo step.
- **Attachments** — a file can be embedded from the Files panel, not only saved
  out.
- **Page labels** — Page → Labels sets the printed numbering (1, i, A, plus a
  prefix) from a chosen page.
- **MuPDF 1.28** — SweetPDF builds against MuPDF 1.28.6 when `scripts/build-mupdf.sh`
  has installed it, and still compiles with the 1.27 packages distributions
  ship. Opening a file now includes Markdown, HTML, plain text and FictionBook.
  Text extraction turns on MuPDF's dehyphenation and accurate glyph boxes.
  Before an edit or a save, the file is scanned with `pdf_check_document`; when
  1.28 repairs the structure, the status bar says so.
- **Find** — the find bar has a regular-expression mode (`.*`) that uses
  MuPDF's search matcher, including case-insensitive patterns.
- **Signatures** — signed form fields are checked with MuPDF and reported as
  valid, invalid, or out of date after unsaved changes. Creating a PKCS#7
  signature is described above.
- **Files and layers** — the sidebar lists embedded files, including file
  attachments that live on a page, and can save one out. Optional-content
  layers can be shown or hidden, and a document's named layer presets can be
  applied.
- **Bookmarks** — outline entries keep the colour, bold and italic the PDF
  specifies. A page can be bookmarked, renamed or removed, and that is saved
  with the file.
- **Saving** — full saves regenerate annotation appearances and pack objects
  into object streams. Linearisation is no longer requested: MuPDF 1.26 removed
  it and later versions refuse the save if it is asked for. Changing an
  annotation's colour, text or box rebuilds its appearance stream.
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
- **Packaging and CI** — `.deb` and `.tar.gz` for Linux, an installer and a
  portable `.zip` for Windows, built and released automatically once every
  platform passes. Linux packages carry a desktop entry, AppStream metadata and
  the icon at every size the theme spec lists.

### Known limitations

- Editing existing text replaces it rather than re-flowing the original runs
  with their fonts. Search-and-redact removes matched glyphs; it does not
  rewrite the surrounding line.
- Cryptographic signatures need the MuPDF PKCS#7 helper from
  `scripts/build-mupdf.sh`. A distribution package of MuPDF can still validate
  a signature it understands, but it cannot create one.
- macOS builds are not produced by CI yet; build from source in the meantime.
