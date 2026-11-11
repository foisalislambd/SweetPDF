#pragma once

#include <mupdf/fitz/version.h>

// MuPDF 1.28 changed a handful of call signatures (structure check now reports
// whether a repair happened, and signature validation is split in two). Code
// that has to compile against the 1.27 packages still shipped by distributions
// keeps the older calls behind this switch.
#if FZ_VERSION_MAJOR > 1 || (FZ_VERSION_MAJOR == 1 && FZ_VERSION_MINOR >= 28)
#define SWEETPDF_MUPDF_1_28 1
#else
#define SWEETPDF_MUPDF_1_28 0
#endif

// `pdf_write_options.reproducible` arrived in 1.28.1. 1.28.0 and the 1.27
// packages distributions still ship do not have the field.
#if FZ_VERSION_MAJOR > 1 \
    || (FZ_VERSION_MAJOR == 1 && FZ_VERSION_MINOR > 28) \
    || (FZ_VERSION_MAJOR == 1 && FZ_VERSION_MINOR == 28 && FZ_VERSION_PATCH >= 1)
#define SWEETPDF_MUPDF_REPRODUCIBLE 1
#else
#define SWEETPDF_MUPDF_REPRODUCIBLE 0
#endif
