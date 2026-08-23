if(NOT DEFINED QML_MODULE_DIR)
    message(FATAL_ERROR "QML_MODULE_DIR is required")
endif()

if(NOT EXISTS "${QML_MODULE_DIR}")
    message(FATAL_ERROR "Generated QML module directory does not exist: ${QML_MODULE_DIR}")
endif()

file(READ "${QML_MODULE_DIR}" _qmldir_contents)

set(_expected_entries
    "ResultsOverview 1.0 pages/ResultsOverview.qml"
    "ResultsFindings 1.0 pages/ResultsFindings.qml"
    "ResultsFilesMetrics 1.0 pages/ResultsFilesMetrics.qml"
    "ResultsSuppressed 1.0 pages/ResultsSuppressed.qml"
)

foreach(_entry IN LISTS _expected_entries)
    string(FIND "${_qmldir_contents}" "${_entry}" _entry_index)
    if(_entry_index EQUAL -1)
        message(FATAL_ERROR "Missing expected QML module entry: ${_entry}")
    endif()
endforeach()

message(STATUS "QML module page aliases are valid")
