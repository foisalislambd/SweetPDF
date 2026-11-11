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
    # INTERFACE so the link line is exactly the list below. A static
    # libmupdf-pkcs7 has to sit in the same group as libmupdf, or a
    # single-pass link drops the signature helper's references.
    add_library(MuPDF::MuPDF INTERFACE IMPORTED)
    set_target_properties(MuPDF::MuPDF PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MuPDF_INCLUDE_DIR}"
    )

    set(_mupdf_link_libs "")
    if(MuPDF_THIRD_LIBRARY)
        list(APPEND _mupdf_link_libs "${MuPDF_THIRD_LIBRARY}")
    endif()

    # A USE_SYSTEM_LIBS MuPDF build still ships libmupdf-third.a for the pieces
    # it vendors (mujs, lcms) and expects the codecs from the system. Linking
    # them is harmless for a shared libmupdf that already pulls the same
    # libraries in, and it is required for the static 1.28 build.
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

    # scripts/build-mupdf.sh installs this beside the headers. Distribution
    # packages usually do not, and then certificate signing stays compiled out.
    find_library(MuPDF_PKCS7_LIBRARY
        NAMES mupdf-pkcs7 libmupdf-pkcs7
        HINTS ${MuPDF_ROOT} ENV MuPDF_ROOT
        PATH_SUFFIXES lib lib64
    )
    set(MuPDF_HAS_PKCS7 FALSE)
    if(MuPDF_PKCS7_LIBRARY
       AND EXISTS "${MuPDF_INCLUDE_DIR}/mupdf/helpers/pkcs7-openssl.h")
        find_package(OpenSSL)
        if(OpenSSL_FOUND)
            set(MuPDF_HAS_PKCS7 TRUE)
            set_property(TARGET MuPDF::MuPDF APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS SWEETPDF_HAS_PKCS7=1)
        endif()
    endif()

    if(MuPDF_HAS_PKCS7 AND NOT APPLE)
        set(_mupdf_link_libs
            "-Wl,--start-group"
            "${MuPDF_PKCS7_LIBRARY}"
            "${MuPDF_LIBRARY}"
            ${_mupdf_link_libs}
            OpenSSL::Crypto
            "-Wl,--end-group")
    elseif(MuPDF_HAS_PKCS7)
        # Apple's linker rescans, so the helper can come first and libmupdf again after.
        set(_mupdf_link_libs
            "${MuPDF_PKCS7_LIBRARY}"
            "${MuPDF_LIBRARY}"
            ${_mupdf_link_libs}
            OpenSSL::Crypto
            "${MuPDF_LIBRARY}")
    else()
        list(INSERT _mupdf_link_libs 0 "${MuPDF_LIBRARY}")
    endif()

    set_property(TARGET MuPDF::MuPDF PROPERTY
        INTERFACE_LINK_LIBRARIES "${_mupdf_link_libs}")
endif()

mark_as_advanced(MuPDF_INCLUDE_DIR MuPDF_LIBRARY MuPDF_THIRD_LIBRARY)
