# FindTesseract.cmake
#
# Exposes `Tesseract::Tesseract` (tesseract + leptonica).

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_TESSERACT QUIET tesseract)
    pkg_check_modules(PC_LEPTONICA QUIET lept)
endif()

find_path(Tesseract_INCLUDE_DIR
    NAMES tesseract/baseapi.h
    HINTS ${PC_TESSERACT_INCLUDE_DIRS}
)
find_library(Tesseract_LIBRARY
    NAMES tesseract tesseract55 tesseract54 tesseract53
    HINTS ${PC_TESSERACT_LIBRARY_DIRS}
)
find_path(Leptonica_INCLUDE_DIR
    NAMES leptonica/allheaders.h allheaders.h
    HINTS ${PC_LEPTONICA_INCLUDE_DIRS}
)
find_library(Leptonica_LIBRARY
    NAMES lept leptonica
    HINTS ${PC_LEPTONICA_LIBRARY_DIRS}
)

find_package_handle_standard_args(Tesseract
    REQUIRED_VARS Tesseract_LIBRARY Tesseract_INCLUDE_DIR
                  Leptonica_LIBRARY Leptonica_INCLUDE_DIR
    VERSION_VAR PC_TESSERACT_VERSION
)

if(Tesseract_FOUND AND NOT TARGET Tesseract::Tesseract)
    add_library(Tesseract::Tesseract UNKNOWN IMPORTED)
    set_target_properties(Tesseract::Tesseract PROPERTIES
        IMPORTED_LOCATION "${Tesseract_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Tesseract_INCLUDE_DIR};${Leptonica_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${Leptonica_LIBRARY}"
    )
endif()

mark_as_advanced(Tesseract_INCLUDE_DIR Tesseract_LIBRARY
                 Leptonica_INCLUDE_DIR Leptonica_LIBRARY)
