add_library(codeqa_desktop_support STATIC
    AppController.cpp
    AppController.h
    ReportModels.cpp
    ReportModels.h
)

target_include_directories(codeqa_desktop_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(codeqa_desktop_support PUBLIC
    codeqa_core
    Qt6::Core
    Qt6::Concurrent
)
codeqa_enable_warnings(codeqa_desktop_support)

qt_add_executable(codeqa_desktop main.cpp)

target_link_libraries(codeqa_desktop PRIVATE
    codeqa_desktop_support
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Concurrent
)

set_source_files_properties(qml/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml
)

set_source_files_properties(qml/pages/ResultsPage.qml PROPERTIES
    QT_RESOURCE_ALIAS pages/ResultsPage.qml
)

qt_add_qml_module(codeqa_desktop
    URI CodeQA.Desktop
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/ResultsPage.qml
)
