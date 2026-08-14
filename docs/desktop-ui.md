# CodeQA desktop UI

CodeQA Desktop is the Windows Qt 6/QML companion to the existing `codeqa` CLI. It runs the same C++17 application facade and analysis pipeline locally. The desktop UI has three English-language pages:

- **Analyze** selects a project, edits the supported semantic `codeqa.json` fields, explicitly saves or resets the form, and starts analysis.
- **History** lists `<project>/.codeqa/snapshots` newest first. Opening a snapshot is read-only and creates no snapshot. Deletion affects only the selected snapshot and requires a confirmation that names its timestamp.
- **Results** shows imported, snapshot, or fresh-analysis reports as read-only findings and file summaries. It does not edit report or source content.

There is no raw JSON editor, network service, telemetry, or analytics integration.

## Requirements and configuration

The desktop target requires a Windows Qt 6 installation with these runtime modules:

- Core and Concurrent;
- Gui, Qml, and Quick;
- QuickControls2 and QuickDialogs2.

The test build additionally uses Test and QuickTest. A Qt-enabled strict configure with the repository's supported commands is:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build `
  -DCMAKE_PREFIX_PATH='C:\Qt\6.11.0\msvc2022_64' `
  -DCODEQA_BUILD_DESKTOP=ON `
  -DCODEQA_WARNINGS_AS_ERRORS=ON
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --config Debug
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build -C Debug --output-on-failure
```

To build only the Qt-free core and CLI, no Qt installation is needed:

```powershell
cmake -S . -B build-cli -DCODEQA_BUILD_DESKTOP=OFF -DCODEQA_WARNINGS_AS_ERRORS=ON
cmake --build build-cli --config Debug
ctest --test-dir build-cli -C Debug --output-on-failure
```

The desktop executable is normally `build/Debug/codeqa_desktop.exe` for a multi-configuration Debug build.

## Local storage and privacy

Project-owned files stay in their established CLI-compatible locations:

- semantic configuration: `<project>/codeqa.json`;
- version-1 JSON snapshots: `<project>/.codeqa/snapshots/<timestamp>.json`.

Analysis creates a snapshot through the existing pipeline. Importing a report or opening a History entry does not. Configuration is written only by the explicit **Save** action, and a History entry is deleted only after confirmation.

The desktop shell uses `QSettings` for recent project/report paths, the last selected page, and Results filters. With the standard Windows backend these preferences are stored under the current user's profile (normally the current-user registry), not beside the executable. Report contents, findings, source contents, configuration contents, and snapshot contents are not stored in `QSettings`.

All analysis, report import, settings, and History operations are local. CodeQA Desktop sends no files or usage data over the network and has no telemetry.

## Create a Windows deployment directory

Configure with a Qt-enabled build, then request the deployment target:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --config Release --target codeqa_desktop_deploy
```

The target copies the executable to `build/deploy/codeqa_desktop/codeqa_desktop.exe` and runs the selected Qt kit's `windeployqt` with the desktop QML source directory. For MSVC builds it also supplies the selected compiler installation so a Release deployment can include the redistributable compiler runtime. It is defined even when deployment tooling is unavailable: ordinary configure/build still works, but requesting `codeqa_desktop_deploy` fails with an actionable diagnostic.

The selected Qt kit is the only automatic search location. If its deployment tool has a nonstandard location, configure the full path explicitly:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build `
  -DCMAKE_PREFIX_PATH='C:\Qt\6.11.0\msvc2022_64' `
  -DCODEQA_BUILD_DESKTOP=ON `
  -DCODEQA_WINDEPLOYQT_EXECUTABLE='C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe'
```

Copy the entire `build/deploy/codeqa_desktop` directory when delivering the app; the executable alone does not include its deployed Qt libraries and QML modules.

## Clean Windows machine check

On a Windows machine without a developer Qt installation:

1. Copy the complete deployment directory to a new local folder and record its contents.
2. If the machine does not already have the Microsoft Visual C++ runtime, run the packaged `vc_redist.x64.exe` once.
3. Start `codeqa_desktop.exe` and confirm the Analyze, History, and Results pages open without an import/plugin error.
4. Choose **Open report…**, select a known valid schema-version-1 CodeQA JSON report, and confirm its findings and file summary appear on Results.
5. Close the app and compare the deployment folder with the recorded contents. Starting the app and importing the report must create no file next to the executable; UI preferences belong to the current user's `QSettings` location.

Selecting a project and running analysis is a separate, explicitly mutating check because it writes the project's snapshot directory by design.
