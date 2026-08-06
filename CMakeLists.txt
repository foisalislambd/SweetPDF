cmake_minimum_required(VERSION 3.21)

project(SweetPDF
    VERSION 0.1.0
    DESCRIPTION "All-in-one open source PDF editor"
    HOMEPAGE_URL "https://github.com/sweetpdf/sweetpdf"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SWEETPDF_ENABLE_OCR "Build OCR support (Tesseract)" ON)
option(SWEETPDF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(sweetpdf_warnings INTERFACE)
if(MSVC)
    target_compile_options(sweetpdf_warnings INTERFACE /W4 /permissive-)
    if(SWEETPDF_WARNINGS_AS_ERRORS)
        target_compile_options(sweetpdf_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(sweetpdf_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(SWEETPDF_WARNINGS_AS_ERRORS)
        target_compile_options(sweetpdf_warnings INTERFACE -Werror)
    endif()
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent PrintSupport)
qt_standard_project_setup()

find_package(MuPDF REQUIRED)
find_package(SQLite3 REQUIRED)

if(SWEETPDF_ENABLE_OCR)
    find_package(Tesseract)
    if(NOT Tesseract_FOUND)
        message(WARNING "Tesseract not found - OCR support disabled")
        set(SWEETPDF_ENABLE_OCR OFF)
    endif()
endif()

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
