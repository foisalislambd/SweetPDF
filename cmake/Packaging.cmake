# Packaging.cmake
#
# Turns an install tree into the archives the release workflow uploads. Linux
# gets a .deb (dependencies resolved by dpkg-shlibdeps) plus a .tar.gz; Windows
# gets a .zip that the CI job first fills with Qt and the third-party DLLs.

set(CPACK_PACKAGE_NAME "sweetpdf")
set(CPACK_PACKAGE_VENDOR "SweetPDF")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SweetPDF")
set(CPACK_STRIP_FILES ON)

# Architecture in the file name matters here: the same release carries x86_64 and
# arm64 builds, and a user downloading the wrong one gets a confusing failure.
if(SWEETPDF_PACKAGE_ARCH)
    set(_arch "${SWEETPDF_PACKAGE_ARCH}")
else()
    set(_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${_arch}" _arch)

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "sweetpdf-${PROJECT_VERSION}-windows-${_arch}")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "sweetpdf-${PROJECT_VERSION}-macos-${_arch}")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_PACKAGE_FILE_NAME "sweetpdf-${PROJECT_VERSION}-linux-${_arch}")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SweetPDF contributors")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

unset(_arch)

include(CPack)
