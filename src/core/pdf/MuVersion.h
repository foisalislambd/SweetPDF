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
