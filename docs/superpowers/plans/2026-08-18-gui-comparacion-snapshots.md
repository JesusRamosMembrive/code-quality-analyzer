# GUI: Arbitrary Snapshot Comparison Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the desktop GUI compare any two saved snapshots for the current project and show new/existing/resolved finding counts between them.

**Architecture:** One new `ApplicationFacade::compareSnapshots` method is pure wiring over three already-existing functions (`LoadSnapshot`, `codeqa::history::extractFindingKeys(json)`, `codeqa::history::compareSnapshots`) — no changes to `codeqa::history` at all. `AppController` exposes the three counts as `Q_PROPERTY`s and reuses the existing `errorMessage` surface for failures. `HistoryPage.qml` gets two snapshot pickers and a "Compare" button.

**Tech Stack:** C++17, Qt6/QML (desktop shell), doctest (core unit tests), Qt Test (desktop unit + QML tests), nlohmann::json.

**Spec:** `docs/superpowers/specs/2026-08-18-gui-comparacion-snapshots-design.md`

## Global Constraints

- No changes to `codeqa::history::compareSnapshots`/`extractFindingKeys`/`SnapshotComparison`, the quality gate, or the automatic current-vs-previous comparison in `ApplicationFacade::analyzeProject` (spec §2).
- Aggregate counts only — no per-finding identity surfaced anywhere in this track (spec §2).
- No report/snapshot schema changes, no CLI changes (spec §2).
- `fromTimestamp` = "previous", `toTimestamp` = "current" in `compareSnapshots`'s existing vocabulary — "new" means present in `to` but not `from` (spec §4).

---

### Task 1: `ApplicationFacade::compareSnapshots`

**Files:**
- Modify: `include/codeqa/application/ApplicationFacade.h`
- Modify: `src/application/ApplicationFacade.cpp`
- Test: `tests/unit/ApplicationFacadeTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::LoadSnapshot` (existing), `codeqa::history::extractFindingKeys(const nlohmann::json&)` (existing), `codeqa::history::compareSnapshots` (existing).
- Produces: `codeqa::application::SnapshotComparisonResult{std::optional<codeqa::history::SnapshotComparison> comparison; ApplicationError error;}` and `ApplicationFacade::compareSnapshots(projectRoot, fromTimestamp, toTimestamp) -> SnapshotComparisonResult`. Task 2 calls this directly.

- [ ] **Step 1: Write the failing tests**

In `tests/unit/ApplicationFacadeTest.cpp`, add three test cases (place them after the existing `TEST_CASE("ApplicationFacade compares against the newest snapshot without changing raw report JSON")`, around line 363):

```cpp
TEST_CASE("ApplicationFacade compares two arbitrary snapshots by counting added, shared, and removed findings")
{
    const TempProject project;
    project.writeSnapshot(
        "20260101T000000Z",
        R"({"files": [{"path": "sample.c", "findings": [)"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "oldOnly"}, )"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "shared"}]}]})");
    project.writeSnapshot(
        "20260102T000000Z",
        R"({"files": [{"path": "sample.c", "findings": [)"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "shared"}, )"
        R"({"ruleId": "complexity.cyclomatic", "symbolName": "newOnly"}]}]})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "20260102T000000Z");

    REQUIRE(result.comparison.has_value());
    CHECK_FALSE(result.error.hasError());
    CHECK(result.comparison->newCount == 1);
    CHECK(result.comparison->existingCount == 1);
    CHECK(result.comparison->resolvedCount == 1);
}

TEST_CASE("ApplicationFacade reports APPLICATION003 when a compared snapshot does not exist")
{
    const TempProject project;
    project.writeSnapshot("20260101T000000Z", R"({"files": []})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "does-not-exist");

    CHECK_FALSE(result.comparison.has_value());
    REQUIRE(result.error.hasError());
    CHECK(result.error.message->find("APPLICATION003") != std::string::npos);
}

TEST_CASE("ApplicationFacade reports APPLICATION004 when a compared snapshot has an unexpected shape")
{
    const TempProject project;
    project.writeSnapshot("20260101T000000Z", R"({"files": []})");
    project.writeSnapshot("20260102T000000Z", R"({"files": [{"path": "sample.c"}]})");

    const codeqa::application::ApplicationFacade facade;
    const codeqa::application::SnapshotComparisonResult result =
        facade.compareSnapshots(project.root(), "20260101T000000Z", "20260102T000000Z");

    CHECK_FALSE(result.comparison.has_value());
    REQUIRE(result.error.hasError());
    CHECK(result.error.message->find("APPLICATION004") != std::string::npos);
}
```

(`TempProject::writeSnapshot`, `TempProject::root()`, and `TempProject` itself already exist in this file — no new test fixture needed. The second snapshot in the third test is missing the required `"findings"` key on its one file, which is exactly what makes `codeqa::history::extractFindingKeys` throw `nlohmann::json::exception` via its own `.at("findings")` call.)

- [ ] **Step 2: Run tests to verify they fail**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Expected: FAIL to compile — `class ApplicationFacade has no member named 'compareSnapshots'` / `'SnapshotComparisonResult' is not a member of 'codeqa::application'`.

- [ ] **Step 3: Add `SnapshotComparisonResult` and the method declaration**

In `include/codeqa/application/ApplicationFacade.h`, add the include and the struct near `ApplicationError` (the file already includes `codeqa/history/SnapshotComparison.h` indirectly is NOT guaranteed — add it explicitly), and the method to the class:

```cpp
#include "codeqa/history/SnapshotComparison.h"
```

(add this include near the top, alongside the file's other includes)

```cpp
struct SnapshotComparisonResult
{
    std::optional<codeqa::history::SnapshotComparison> comparison;
    ApplicationError error;
};
```

(add this struct definition right after `struct ApplicationError { ... };`)

```cpp
    [[nodiscard]] SnapshotComparisonResult compareSnapshots(
        const std::filesystem::path& projectRoot,
        const std::string& fromTimestamp,
        const std::string& toTimestamp) const;
```

(add this method declaration to `class ApplicationFacade`, after `deleteSnapshot`'s declaration)

- [ ] **Step 4: Implement the method**

In `src/application/ApplicationFacade.cpp`, add near the top of the file (check whether `codeqa/history/CompareSnapshots.h` is already included — if not, add `#include "codeqa/history/CompareSnapshots.h"`), then add the implementation after `deleteSnapshot`'s definition (or after `openSnapshot`'s — place it near the other snapshot-related methods):

```cpp
SnapshotComparisonResult ApplicationFacade::compareSnapshots(
    const std::filesystem::path& projectRoot,
    const std::string& fromTimestamp,
    const std::string& toTimestamp) const
{
    SnapshotComparisonResult result;

    const std::optional<nlohmann::json> fromReport = LoadSnapshot{}.execute(projectRoot, fromTimestamp);
    if (!fromReport.has_value())
    {
        result.error.message = "Error [APPLICATION003]: could not load snapshot '" + fromTimestamp + "'.";
        return result;
    }

    const std::optional<nlohmann::json> toReport = LoadSnapshot{}.execute(projectRoot, toTimestamp);
    if (!toReport.has_value())
    {
        result.error.message = "Error [APPLICATION003]: could not load snapshot '" + toTimestamp + "'.";
        return result;
    }

    try
    {
        const codeqa::history::FindingKeySet fromKeys = codeqa::history::extractFindingKeys(*fromReport);
        const codeqa::history::FindingKeySet toKeys = codeqa::history::extractFindingKeys(*toReport);
        result.comparison = codeqa::history::compareSnapshots(toKeys, fromKeys);
    }
    catch (const nlohmann::json::exception&)
    {
        result.error.message = "Error [APPLICATION004]: a snapshot has an unexpected shape.";
    }

    return result;
}
```

- [ ] **Step 5: Build and run tests to verify they pass**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Then: `& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build -C Debug -R codeqa_tests --output-on-failure`
Expected: PASS, including the three new test cases.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/application/ApplicationFacade.h src/application/ApplicationFacade.cpp tests/unit/ApplicationFacadeTest.cpp
git commit -m "feat: add ApplicationFacade::compareSnapshots for arbitrary snapshot pairs"
```

---

### Task 2: `AppController::compareSnapshots`

**Files:**
- Modify: `src/desktop/AppController.h`
- Modify: `src/desktop/AppController.cpp`
- Test: `tests/desktop/AppControllerTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::ApplicationFacade::compareSnapshots` (Task 1), `codeqa::application::SnapshotComparisonResult`.
- Produces: `AppController::compareSnapshots(QString, QString)` (`Q_INVOKABLE`), `Q_PROPERTY`s `snapshotComparisonNewCount`/`snapshotComparisonExistingCount`/`snapshotComparisonResolvedCount`/`snapshotComparisonAvailable` (all `int`/`bool`, `NOTIFY snapshotComparisonChanged`). Task 3's QML reads these exact property names.

- [ ] **Step 1: Write the failing test**

In `tests/desktop/AppControllerTest.cpp`, add a new slot declaration to the `private slots:` list (after `snapshotsAreNewestFirstAndDeleteRefreshesTheModel();`, around line 186):

```cpp
    void compareSnapshotsExposesCountsAndResetsOnProjectChange();
    void compareSnapshotsFailureSetsErrorMessage();
```

Add the test definitions (place them after `snapshotsAreNewestFirstAndDeleteRefreshesTheModel`'s own definition):

```cpp
void AppControllerTest::compareSnapshotsExposesCountsAndResetsOnProjectChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    writeTextFile(
        QDir(snapshots).filePath("20260101T000000Z.json"),
        QByteArray::fromStdString(reportJson(
            std::filesystem::path(directory.path().toStdWString()),
            {{"src/sample.cpp", "warning", "old.rule", "oldOnly", 2},
             {"src/sample.cpp", "warning", "shared.rule", "shared", 4}}).dump(2)));
    writeTextFile(
        QDir(snapshots).filePath("20260102T000000Z.json"),
        QByteArray::fromStdString(reportJson(
            std::filesystem::path(directory.path().toStdWString()),
            {{"src/sample.cpp", "warning", "shared.rule", "shared", 4},
             {"src/sample.cpp", "warning", "new.rule", "newOnly", 6}}).dump(2)));
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.refreshSnapshots();
    QCOMPARE(controller.snapshots()->rowCount(), 2);

    QVERIFY(!controller.snapshotComparisonAvailable());

    controller.compareSnapshots(QStringLiteral("20260101T000000Z"), QStringLiteral("20260102T000000Z"));

    QVERIFY(controller.snapshotComparisonAvailable());
    QCOMPARE(controller.snapshotComparisonNewCount(), 1);
    QCOMPARE(controller.snapshotComparisonExistingCount(), 1);
    QCOMPARE(controller.snapshotComparisonResolvedCount(), 1);
    QCOMPARE(controller.errorMessage(), QString{});

    QTemporaryDir otherDirectory;
    QVERIFY(otherDirectory.isValid());
    controller.selectProject(QUrl::fromLocalFile(otherDirectory.path()));

    QVERIFY(!controller.snapshotComparisonAvailable());
}

void AppControllerTest::compareSnapshotsFailureSetsErrorMessage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString snapshots = directory.filePath(".codeqa/snapshots");
    QVERIFY(QDir{}.mkpath(snapshots));
    writeTextFile(
        QDir(snapshots).filePath("20260101T000000Z.json"),
        QByteArray::fromStdString(reportJson(
            std::filesystem::path(directory.path().toStdWString()), oneFinding()).dump(2)));
    AppController controller(settingsPath(directory));
    controller.selectProject(QUrl::fromLocalFile(directory.path()));
    controller.refreshSnapshots();

    controller.compareSnapshots(QStringLiteral("20260101T000000Z"), QStringLiteral("does-not-exist"));

    QVERIFY(!controller.snapshotComparisonAvailable());
    QVERIFY(controller.errorMessage().contains("APPLICATION003"));
}
```

(`reportJson`, `oneFinding`, `writeTextFile`, `settingsPath` already exist in this file's anonymous namespace — no new fixture helpers needed. `reportJson` takes a `std::vector<FindingSpec>`, each `FindingSpec{path, severity, ruleId, symbolName, line}`.)

- [ ] **Step 2: Run test to verify it fails to compile**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_tests -j 4`
Expected: FAIL — `no member named 'compareSnapshots'`/`'snapshotComparisonAvailable'` in `AppController`.

- [ ] **Step 3: Add the properties and method to the header**

In `src/desktop/AppController.h`, add after the `excludePatterns` `Q_PROPERTY` (line 42):

```cpp
    Q_PROPERTY(int snapshotComparisonNewCount READ snapshotComparisonNewCount NOTIFY snapshotComparisonChanged)
    Q_PROPERTY(
        int snapshotComparisonExistingCount READ snapshotComparisonExistingCount
            NOTIFY snapshotComparisonChanged)
    Q_PROPERTY(
        int snapshotComparisonResolvedCount READ snapshotComparisonResolvedCount NOTIFY snapshotComparisonChanged)
    Q_PROPERTY(bool snapshotComparisonAvailable READ snapshotComparisonAvailable NOTIFY snapshotComparisonChanged)
```

Add getter declarations after `excludePatterns()`'s (line 67):

```cpp
    [[nodiscard]] int snapshotComparisonNewCount() const;
    [[nodiscard]] int snapshotComparisonExistingCount() const;
    [[nodiscard]] int snapshotComparisonResolvedCount() const;
    [[nodiscard]] bool snapshotComparisonAvailable() const;
```

Add the invokable method after `deleteSnapshot`'s declaration (line 88):

```cpp
    Q_INVOKABLE void compareSnapshots(const QString& fromTimestamp, const QString& toTimestamp);
```

Add the new signal after `recentPathsChanged()` (line 115):

```cpp
    void snapshotComparisonChanged();
```

Add the member fields after `configurationDirty_` (line 168):

```cpp
    int snapshotComparisonNewCount_{0};
    int snapshotComparisonExistingCount_{0};
    int snapshotComparisonResolvedCount_{0};
    bool snapshotComparisonAvailable_{false};
```

- [ ] **Step 4: Implement in the .cpp**

In `src/desktop/AppController.cpp`, add the include near the top (alongside the file's other `codeqa::application` includes):

```cpp
#include "codeqa/history/SnapshotComparison.h"
```

Add the getters after `excludePatterns()`'s implementation (find it near the other simple getters):

```cpp
int AppController::snapshotComparisonNewCount() const
{
    return snapshotComparisonNewCount_;
}

int AppController::snapshotComparisonExistingCount() const
{
    return snapshotComparisonExistingCount_;
}

int AppController::snapshotComparisonResolvedCount() const
{
    return snapshotComparisonResolvedCount_;
}

bool AppController::snapshotComparisonAvailable() const
{
    return snapshotComparisonAvailable_;
}
```

Add the invokable method implementation near `deleteSnapshot(const QUrl&, const QString&)`'s implementation:

```cpp
void AppController::compareSnapshots(const QString& fromTimestamp, const QString& toTimestamp)
{
    const codeqa::application::SnapshotComparisonResult result =
        facade_.compareSnapshots(currentProjectPath_, fromTimestamp.toStdString(), toTimestamp.toStdString());

    if (!result.comparison.has_value())
    {
        snapshotComparisonAvailable_ = false;
        setErrorMessage(QString::fromStdString(
            result.error.message.value_or("Error [APPLICATION003]: could not compare snapshots.")));
        emit snapshotComparisonChanged();
        return;
    }

    snapshotComparisonNewCount_ = static_cast<int>(result.comparison->newCount);
    snapshotComparisonExistingCount_ = static_cast<int>(result.comparison->existingCount);
    snapshotComparisonResolvedCount_ = static_cast<int>(result.comparison->resolvedCount);
    snapshotComparisonAvailable_ = true;
    setErrorMessage({});
    emit snapshotComparisonChanged();
}
```

In `AppController::setCurrentProject` (around line 675), reset the comparison state alongside the existing `snapshots_.setSnapshots({})` reset — add these two lines immediately after both `snapshots_.setSnapshots({});` calls (there are two, one in the empty-path branch and one in the normal branch; both are followed by `emit currentProjectChanged(currentProject());` inside the same `if` block):

```cpp
        snapshotComparisonAvailable_ = false;
        emit snapshotComparisonChanged();
```

The explicit `emit` is required here (not optional): `snapshotComparisonResult`'s QML `visible`/`text` bindings (Task 3) only re-evaluate when `snapshotComparisonChanged` fires, so without it, switching projects would leave the previous project's stale comparison result showing on screen.

- [ ] **Step 5: Build and run tests to verify they pass**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop codeqa_desktop_tests -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_tests --output-on-failure`
Expected: PASS, including both new tests.

- [ ] **Step 6: Commit**

```bash
git add src/desktop/AppController.h src/desktop/AppController.cpp tests/desktop/AppControllerTest.cpp
git commit -m "feat: expose snapshot comparison to the desktop GUI via AppController"
```

---

### Task 3: QML snapshot pickers on `HistoryPage.qml`

**Files:**
- Modify: `src/desktop/qml/pages/HistoryPage.qml`
- Test: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**
- Consumes: `AppController::compareSnapshots(QString, QString)` and `snapshotComparisonNewCount`/`ExistingCount`/`ResolvedCount`/`Available` (Task 2).
- Produces: nothing consumed by a later task — this is the last task.

- [ ] **Step 1: Add the fixture and failing test**

In `tests/desktop/qml/tst_DesktopShell.qml`, add to `fixtureController` (after the `suppressedFindings` property, around line 98):

```qml
        property int snapshotComparisonNewCount: 0
        property int snapshotComparisonExistingCount: 0
        property int snapshotComparisonResolvedCount: 0
        property bool snapshotComparisonAvailable: false
        property string lastCompareFrom: ""
        property string lastCompareTo: ""
```

Add a function alongside the other mocked action functions (after `deleteSnapshot`'s definition, around line 157):

```qml
        function compareSnapshots(fromTimestamp, toTimestamp) {
            lastCompareFrom = fromTimestamp
            lastCompareTo = toTimestamp
            snapshotComparisonNewCount = 1
            snapshotComparisonExistingCount = 1
            snapshotComparisonResolvedCount = 1
            snapshotComparisonAvailable = true
        }
```

Add a new test function after `test_history_delete_requires_named_confirmation` (find it in the file's History-page test group):

```qml
    function test_history_compare_snapshots_shows_counts() {
        fixtureController.snapshotComparisonAvailable = false
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("History")

        const fromCombo = shell.findChild("fromSnapshotCombo")
        const toCombo = shell.findChild("toSnapshotCombo")
        verify(fromCombo !== null)
        verify(toCombo !== null)
        fromCombo.currentIndex = 1
        toCombo.currentIndex = 0

        const compareButton = shell.findChild("compareSnapshotsButton")
        verify(compareButton !== null)
        verify(compareButton.enabled)
        mouseClick(compareButton, compareButton.width / 2, compareButton.height / 2)

        compare(fixtureController.lastCompareFrom, "20260101T000000Z")
        compare(fixtureController.lastCompareTo, "20260301T000000Z")
        const resultLabel = shell.findChild("snapshotComparisonResult")
        verify(resultLabel !== null)
        tryVerify(function() { return resultLabel.visible === true })
        verify(resultLabel.text.indexOf("1") !== -1)
    }
```

(`fixtureSnapshots` already has two `ListElement`s with timestamps `"20260301T000000Z"` and `"20260101T000000Z"`, newest first — see the top of this file. `fromCombo.currentIndex = 1`/`toCombo.currentIndex = 0` picks the two different existing timestamps so the Compare button is enabled.)

- [ ] **Step 2: Run test to verify it fails**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_qml_smoke -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_qml_smoke --output-on-failure`
Expected: FAIL — `shell.findChild("fromSnapshotCombo")` returns null.

If the rebuild doesn't pick up the QML edits, delete `buildQt/tests/desktop-qml` and `buildQt/tests/.qt/rcc` and rebuild (known gotcha, see `src/desktop/qml/pages/HistoryPage.qml`'s registration in `tests/CMakeLists.txt` — it is already in both file lists, no CMakeLists changes needed since this is an existing file's content changing, not a new file).

- [ ] **Step 3: Add the QML**

In `src/desktop/qml/pages/HistoryPage.qml`, add after the `snapshotList` `ListView`'s closing brace (currently the last child of the outer `ColumnLayout`, just before its own closing brace):

```qml
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Compare:"
            }

            ComboBox {
                id: fromSnapshotCombo
                objectName: "fromSnapshotCombo"
                Layout.preferredWidth: 200
                model: historyPage.controller ? historyPage.controller.snapshots : null
                textRole: "timestamp"
            }

            Label {
                text: "\u2192"
            }

            ComboBox {
                id: toSnapshotCombo
                objectName: "toSnapshotCombo"
                Layout.preferredWidth: 200
                model: historyPage.controller ? historyPage.controller.snapshots : null
                textRole: "timestamp"
            }

            PulseButton {
                id: compareSnapshotsButton
                objectName: "compareSnapshotsButton"
                text: "Compare"
                enabled: historyPage.controller
                    && fromSnapshotCombo.currentText !== ""
                    && toSnapshotCombo.currentText !== ""
                    && fromSnapshotCombo.currentText !== toSnapshotCombo.currentText
                onClicked: historyPage.controller.compareSnapshots(
                    fromSnapshotCombo.currentText, toSnapshotCombo.currentText)
            }

            Label {
                id: snapshotComparisonResult
                objectName: "snapshotComparisonResult"
                visible: historyPage.controller && historyPage.controller.snapshotComparisonAvailable
                text: historyPage.controller
                    ? historyPage.controller.snapshotComparisonNewCount + " new, "
                        + historyPage.controller.snapshotComparisonExistingCount + " existing, "
                        + historyPage.controller.snapshotComparisonResolvedCount + " resolved"
                    : ""
            }
        }
```

- [ ] **Step 4: Build and run test to verify it passes**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_qml_smoke -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_qml_smoke --output-on-failure`
Expected: PASS, all sub-tests including the new one.

- [ ] **Step 5: Full-suite regression check**

Run, from the repo root (not target-restricted, per this project's own established lesson that a target-restricted build silently skips CLI-integration ctest targets):
```
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug -j 4
& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build -C Debug --output-on-failure
```
Also, from `buildQt/`:
```
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop codeqa_desktop_tests -j 4
PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R "codeqa_desktop_qml_smoke|codeqa_desktop_tests" --output-on-failure
```
Expected: 100% pass on both, no regressions.

- [ ] **Step 6: Commit**

```bash
git add src/desktop/qml/pages/HistoryPage.qml tests/desktop/qml/tst_DesktopShell.qml
git commit -m "feat: add snapshot comparison pickers to the desktop History page"
```

---

## Self-review notes

- **Spec coverage:** §4 (facade contract) → Task 1. §5 (controller) → Task 2, including the project-change reset requirement. §6 (QML) → Task 3, including the disabled-when-equal/empty Compare button from §7. §8 (testing) → one test group per task, matching the spec's three bullets. §9 acceptance criteria are each covered: real added/removed/unchanged counts (Task 1+2 tests), self-comparison unreachable (Task 3's `enabled` binding), deleted/corrupted snapshot surfaces `errorMessage` (Task 1's APPLICATION003 test + Task 2's failure test), no `codeqa::history`/CLI changes (verified by file scope — no task touches `src/history/`, `include/codeqa/history/`, or `src/cli/`).
- **No `codeqa::history` changes** — confirmed no task lists any file under that directory.
- **Type consistency:** `SnapshotComparisonResult` (Task 1) is used identically in Task 2's `AppController::compareSnapshots` implementation. The four `Q_PROPERTY` names and the `compareSnapshots(QString, QString)` signature (Task 2) are used identically in Task 3's QML (`snapshotComparisonNewCount`/`ExistingCount`/`ResolvedCount`/`Available`, `objectName`s `fromSnapshotCombo`/`toSnapshotCombo`/`compareSnapshotsButton`/`snapshotComparisonResult`).
