qt_add_library(sweetpdf_core STATIC
    Geometry.h
    MuContext.h            MuContext.cpp
    PdfDocument.h          PdfDocument.cpp
    PdfPage.h              PdfPage.cpp
    RenderRequest.h
    RenderQueue.h          RenderQueue.cpp
    PageCache.h            PageCache.cpp
    DocumentOutline.h
    UndoCommand.h
    DocumentSession.h      DocumentSession.cpp
    Logging.h              Logging.cpp
)

target_include_directories(sweetpdf_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(sweetpdf_core
    PUBLIC  Qt6::Core Qt6::Gui
    PRIVATE Qt6::Concurrent MuPDF::MuPDF sweetpdf_warnings
)
