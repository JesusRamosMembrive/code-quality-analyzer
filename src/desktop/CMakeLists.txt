qt_add_executable(codeqa_desktop main.cpp)

target_link_libraries(codeqa_desktop PRIVATE
    codeqa_core
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

qt_add_qml_module(codeqa_desktop
    URI CodeQA.Desktop
    VERSION 1.0
    QML_FILES qml/Main.qml
)
