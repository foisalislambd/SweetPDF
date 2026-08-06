# Security Policy

SweetPDF parses PDF files, which are attacker-controlled input by nature. A
malformed document that crashes the program, reads memory it should not, or
executes code is a security issue, and we would like to hear about it.

## Supported versions

Only the newest release is supported. Fixes go into the next release rather than
being backported, and releases are frequent because they are automated.

| Version | Supported |
| --- | --- |
| Newest release | Yes |
| Anything older | No — please upgrade first and check the problem is still there |

## Reporting a vulnerability

**Please do not open a public issue for a vulnerability.** Use GitHub's private
reporting instead:

1. Go to [Report a
   vulnerability](https://github.com/sweetpdf/sweetpdf/security/advisories/new).
2. Describe what happens and what you think the impact is.
3. Attach the document that triggers it, or a script that generates one. A
   reproducer is the single most useful thing in the report.
4. Tell us the version (*Menu → Help → About*), your operating system, and how
   you installed SweetPDF.

You should get a first response within a week. If a fix is needed, we will agree
a disclosure timeline with you and credit you in the release notes unless you
would rather stay anonymous.

## What is in scope

- Memory-safety problems reached through a document: crashes, out-of-bounds
  reads or writes, use-after-free, integer overflows in size calculations.
- Anything that escapes the document sandbox: writing files outside a path the
  user chose, running an external program, or making a network connection.
- Redaction or encryption that does not do what it claims — for example content
  still recoverable from a file after *Apply Redactions*, or a saved document
  that is weaker than the chosen encryption level. These are correctness bugs
  with security consequences and we treat them as vulnerabilities.
- Passwords or document content ending up somewhere unexpected on disk.

## What is not in scope

- Crashes in the underlying libraries that we merely pass through. Report those
  to [MuPDF](https://bugs.ghostscript.com/) or the relevant project; tell us too
  so we can add a workaround or bump the requirement.
- A document that renders incorrectly, or a feature that is missing. Those are
  ordinary bugs — please use the [issue
  tracker](https://github.com/sweetpdf/sweetpdf/issues).
- Weak protection that the PDF format itself provides. Permission flags such as
  "printing not allowed" are advisory in every PDF reader, and a document
  encrypted with an old algorithm stays weak; neither is a flaw in SweetPDF.
- Attacks that need an already-compromised machine or an attacker who can change
  the installed program.

## What SweetPDF does not do

Useful context when judging impact: there is no network code in the program
apart from opening a link you click, no telemetry, no auto-updater, and no
account. Documents are never uploaded anywhere. Passwords you type to open a
document are used for that document and are not written to disk.
