# The document model and rendering pipeline. Depends on Qt Core and Qt Gui only:
# no widget may ever be included from here. See README.md in this directory.

qt_add_library(sweetpdf_core STATIC
    # pdf/ - the MuPDF boundary. Nothing outside this folder touches fz_* types.
    pdf/Geometry.h
    pdf/DocumentOutline.h
    pdf/MuContext.h            pdf/MuContext.cpp
    pdf/PdfDocument.h          pdf/PdfDocument.cpp
    pdf/PdfPage.h              pdf/PdfPage.cpp
    pdf/PageText.h             pdf/PageText.cpp
    pdf/PdfAnnotations.h       pdf/PdfAnnotations.cpp
    pdf/PdfContent.h           pdf/PdfContent.cpp
    pdf/PdfForms.h             pdf/PdfForms.cpp
    pdf/PdfLayers.cpp
    pdf/PdfFiles.cpp
    pdf/MuVersion.h
    pdf/PdfExport.h            pdf/PdfExport.cpp

    # render/ - turning pages into bitmaps off the GUI thread.
    render/RenderKey.h
    render/PageCache.h         render/PageCache.cpp
    render/RenderQueue.h       render/RenderQueue.cpp

    # session/ - per-document state and the undo history.
    session/DocumentSession.h  session/DocumentSession.cpp
    session/DocumentCommand.h
    session/EditCommands.h     session/EditCommands.cpp

    # util/ - cross-cutting helpers.
    util/Logging.h             util/Logging.cpp
)

target_include_directories(sweetpdf_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(sweetpdf_core
    PUBLIC  Qt6::Core Qt6::Gui
    PRIVATE Qt6::Concurrent MuPDF::MuPDF sweetpdf_warnings
)

if(SWEETPDF_ENABLE_OCR)
    target_compile_definitions(sweetpdf_core PRIVATE SWEETPDF_HAS_OCR=1)
    target_link_libraries(sweetpdf_core PRIVATE Tesseract::Tesseract)
endif()
