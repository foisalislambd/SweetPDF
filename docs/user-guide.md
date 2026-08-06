# User guide

SweetPDF is a local PDF editor: open, annotate, organise, OCR, protect and
export without uploading anything. This page is a short operating guide. For
package downloads see the [README install section](../README.md#install).

## Open a document

- Home screen: **Open PDF**, a task tile, or drop a file on the window.
- Ribbon / Menu: **Open** (`Ctrl+O`).
- Recent list on the home screen: click a path to reopen it.

Encrypted files ask for a password once per open. SweetPDF does not store it.

## The window

| Part | Purpose |
| --- | --- |
| **Home** | Task tiles and recent files when nothing is open. Ribbon and panels stay hidden. |
| **Ribbon** | Commands by job: Home, Edit, Comment, Page, View, Protect, Tools. **Menu** holds file commands and preferences. |
| **Left panel** (`Ctrl+B`) | Pages, bookmarks, comments, forms (attachments and layers are placeholders). |
| **Canvas** | The document. Floating bar: select / pan, zoom, fit. |
| **Right panel** (`Ctrl+J`) | Active tool options, then document properties. |
| **Status bar** | Page number, zoom, layout, theme. |
| **Tabs** | Several documents at once; session restore can bring them back. |

## Everyday tasks

**Select and copy** — Select tool, drag over text. `Ctrl+C` copies. Double-click
a word, triple-click a line where supported.

**Find** — `Ctrl+F`. Matches highlight across the document; `F3` / `Shift+F3`
walk through them.

**Markup** — Comment or Home ribbon: Highlight, Underline, Strike, Draw, Note.
Colours and opacity live in the right-hand tool options.

**Edit text** — Edit → **Edit** tool, click a line (or drag a range), type, then
**Enter** or click away. The old glyphs are redacted and the new wording is
written as a FreeText annotation (not a full re-flow of the original font run).

**Pages** — Insert, delete, extract, duplicate, rotate; drag thumbnails to
reorder. Merge and split live under Page / Tools.

**Forms** — Forms panel lists fields; fill values in place. Flatten bakes them
into page content.

**Sign** — Draw, type or import a signature and place it on one page or all.

**Protect** — Encrypt with permissions; mark redactionsations then **Apply**;
watermarks.

**OCR** — Tools → OCR. Needs Tesseract language data (for example
`tesseract-ocr-eng` on Debian/Ubuntu).

**Export** — Images, plain text or HTML from Tools / export actions.

Nothing is written to disk until you **Save** (`Ctrl+S`). Undo with `Ctrl+Z`.

## Shortcuts (common)

| Action | Keys |
| --- | --- |
| Open | `Ctrl+O` |
| Save | `Ctrl+S` |
| Find | `Ctrl+F` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` |
| Fit page / width | `Ctrl+1` / `Ctrl+2` |
| Navigation panel | `Ctrl+B` |
| Properties | `Ctrl+J` |
| Zoom | `Ctrl` + mouse wheel |

More bindings are listed in the [README](../README.md#keyboard-shortcuts).

## Settings

**Menu → Settings** (or the home screen Settings control): author name for new
annotations, incremental save preference, theme, session restore.

Config location: `~/.config/SweetPDF/` on Linux, registry on Windows,
`~/Library/Preferences` on macOS.
