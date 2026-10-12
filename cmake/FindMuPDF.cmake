# FindMuPDF.cmake
#
# Locates MuPDF and exposes it as the imported target `MuPDF::MuPDF`.
#
# Distributions ship MuPDF very differently: Debian/Ubuntu split the bundled
# third-party code into libmupdf-third.a, while upstream builds and vcpkg link
# system freetype/harfbuzz/jpeg/openjpeg directly. Both shapes are handled here.

include(FindPackageHandleStandardArgs)

find_path(MuPDF_INCLUDE_DIR
    NAMES mupdf/fitz.h
    HINTS ${MuPDF_ROOT} ENV MuPDF_ROOT
    PATH_SUFFIXES include
)

find_library(MuPDF_LIBRARY
    NAMES mupdf libmupdf
    HINTS ${MuPDF_ROOT} ENV MuPDF_ROOT
    PATH_SUFFIXES lib lib64
)

# Debian/Ubuntu bundle of vendored dependencies. Optional elsewhere.
find_library(MuPDF_THIRD_LIBRARY
    NAMES mupdf-third libmupdf-third
    HINTS ${MuPDF_ROOT} ENV MuPDF_ROOT
    PATH_SUFFIXES lib lib64
)

if(MuPDF_INCLUDE_DIR AND EXISTS "${MuPDF_INCLUDE_DIR}/mupdf/fitz/version.h")
    file(STRINGS "${MuPDF_INCLUDE_DIR}/mupdf/fitz/version.h" _mupdf_version_line
         REGEX "^#define[ \t]+FZ_VERSION[ \t]+\"[^\"]*\"")
    string(REGEX REPLACE "^.*FZ_VERSION[ \t]+\"([^\"]*)\".*$" "\\1"
           MuPDF_VERSION "${_mupdf_version_line}")
    unset(_mupdf_version_line)
endif()

find_package_handle_standard_args(MuPDF
    REQUIRED_VARS MuPDF_LIBRARY MuPDF_INCLUDE_DIR
    VERSION_VAR MuPDF_VERSION
)

if(MuPDF_FOUND AND NOT TARGET MuPDF::MuPDF)
    add_library(MuPDF::MuPDF UNKNOWN IMPORTED)
    set_target_properties(MuPDF::MuPDF PROPERTIES
        IMPORTED_LOCATION "${MuPDF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MuPDF_INCLUDE_DIR}"
    )

    set(_mupdf_link_libs "")
    if(MuPDF_THIRD_LIBRARY)
        list(APPEND _mupdf_link_libs "${MuPDF_THIRD_LIBRARY}")
    endif()

    # A USE_SYSTEM_LIBS MuPDF build still ships libmupdf-third.a for the pieces
    # it vendors (mujs, lcms) and expects the codecs from the system. Linking
    # them is harmless for a shared libmupdf that already pulls the same
    # libraries in, and it is required for the static 1.28.5 build.
    foreach(_dep freetype harfbuzz jpeg openjp2 jbig2dec z brotlidec brotlienc brotlicommon)
        find_library(_mupdf_dep_${_dep} NAMES ${_dep})
        if(_mupdf_dep_${_dep})
            list(APPEND _mupdf_link_libs "${_mupdf_dep_${_dep}}")
        endif()
    endforeach()

    find_library(_mupdf_mujs NAMES mujs)
    if(_mupdf_mujs)
        list(APPEND _mupdf_link_libs "${_mupdf_mujs}")
    endif()
    find_library(_mupdf_gumbo NAMES gumbo)
    if(_mupdf_gumbo)
        list(APPEND _mupdf_link_libs "${_mupdf_gumbo}")
    endif()

    if(NOT WIN32)
        list(APPEND _mupdf_link_libs m)
    endif()

    set_property(TARGET MuPDF::MuPDF PROPERTY
        INTERFACE_LINK_LIBRARIES "${_mupdf_link_libs}")
endif()

mark_as_advanced(MuPDF_INCLUDE_DIR MuPDF_LIBRARY MuPDF_THIRD_LIBRARY)
