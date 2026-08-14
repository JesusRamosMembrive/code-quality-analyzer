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

set_source_files_properties(qml/pages/AnalyzePage.qml PROPERTIES
    QT_RESOURCE_ALIAS pages/AnalyzePage.qml
)

set_source_files_properties(qml/pages/HistoryPage.qml PROPERTIES
    QT_RESOURCE_ALIAS pages/HistoryPage.qml
)

qt_add_qml_module(codeqa_desktop
    URI CodeQA.Desktop
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/AnalyzePage.qml
        qml/pages/HistoryPage.qml
        qml/pages/ResultsPage.qml
)

get_target_property(_codeqa_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_codeqa_qt_bin_directory "${_codeqa_qmake_executable}" DIRECTORY)
find_program(CODEQA_WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt.exe windeployqt
    HINTS "${_codeqa_qt_bin_directory}"
    NO_DEFAULT_PATH
    DOC "windeployqt executable from the selected Qt installation"
)

set(_codeqa_desktop_deploy_directory "${CMAKE_BINARY_DIR}/deploy/codeqa_desktop")
if(CODEQA_WINDEPLOYQT_EXECUTABLE AND EXISTS "${CODEQA_WINDEPLOYQT_EXECUTABLE}")
    set(_codeqa_windeployqt_command "${CODEQA_WINDEPLOYQT_EXECUTABLE}")
    if(MSVC)
        get_filename_component(_codeqa_msvc_arch_bin_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_codeqa_msvc_vc_directory
            "${_codeqa_msvc_arch_bin_directory}/../../../../../.."
            ABSOLUTE
        )
        if(EXISTS "${_codeqa_msvc_vc_directory}/Redist")
            set(_codeqa_windeployqt_command
                ${CMAKE_COMMAND} -E env
                "VCINSTALLDIR=${_codeqa_msvc_vc_directory}/"
                "${CODEQA_WINDEPLOYQT_EXECUTABLE}"
            )
        endif()
    endif()

    add_custom_target(codeqa_desktop_deploy
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_codeqa_desktop_deploy_directory}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:codeqa_desktop>"
            "${_codeqa_desktop_deploy_directory}"
        COMMAND ${_codeqa_windeployqt_command}
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            --dir "${_codeqa_desktop_deploy_directory}"
            "$<TARGET_FILE:codeqa_desktop>"
        DEPENDS codeqa_desktop
        COMMENT "Deploying codeqa_desktop with the selected Qt installation"
        VERBATIM
    )
else()
    add_custom_target(codeqa_desktop_deploy
        COMMAND ${CMAKE_COMMAND} -E echo
            "Error: codeqa_desktop_deploy requires windeployqt from the selected Qt installation. CODEQA_WINDEPLOYQT_EXECUTABLE='${CODEQA_WINDEPLOYQT_EXECUTABLE}'. Set it to the tool's full path."
        COMMAND ${CMAKE_COMMAND} -E false
        DEPENDS codeqa_desktop
        VERBATIM
    )
endif()
