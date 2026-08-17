# GUI: Suppressed Findings Visibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make findings suppressed via an inline `// codeqa-disable-next-line <rule-id>` comment visible, read-only, in a new "Suprimidos" tab on the desktop GUI's Results page, for the run that just completed.

**Architecture:** `ApplyRules::execute` already computes suppressed findings and currently discards them — route them into a new `suppressedFindings` vector alongside the existing `findings` vector, thread that same-shaped data through the already-pass-through `RunAnalysisResult`/`ApplicationFacade` types with zero plumbing changes, populate it only on `ReportDocumentFactory::makeReportDocument`'s live-analysis path (never on `parseReportDocument`'s JSON-loading path), and surface it via a new `SuppressedFindingListModel` + a third QML tab.

**Tech Stack:** C++17, Qt6/QML (desktop shell), doctest (core unit tests), Qt Test (desktop unit + QML tests), nlohmann::json.

**Spec:** `docs/superpowers/specs/2026-08-17-gui-gestion-supresiones-design.md`

## Global Constraints

- No new JSON/report schema fields — `codeqa::report::renderText`/`toReportJson` are never touched (spec §1, §2).
- No source-file mutation from the GUI — this track is view-only (spec §1, §2).
- `suppressedFindings` must stay empty when a `ReportDocument` is reconstructed from a parsed/saved report (`parseReportDocument`) — only the live-analysis path (`makeReportDocument`) populates it (spec §1, §3).
- A suppressed finding is exactly the `codeqa::rules::Finding` a rule already produced — no new model type, no new fields on `Finding` itself (spec §4).

---

### Task 1: Route suppressed findings into `FileFindingsModel::suppressedFindings`

**Files:**
- Modify: `include/codeqa/rules/RuleModels.h`
- Modify: `src/application/ApplyRules.cpp`
- Test: `tests/unit/ApplyRulesTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `codeqa::rules::FileFindingsModel::suppressedFindings` (`std::vector<Finding>`), populated by `ApplyRules::execute`. Task 2 reads this field via `RunAnalysisResult::findings.files[i].suppressedFindings`.

- [ ] **Step 1: Add the failing assertions to the existing suppression tests**

In `tests/unit/ApplyRulesTest.cpp`, extend the two existing suppression test cases (they currently only assert on `findings`, not the new field):

```cpp
TEST_CASE("ApplyRules suppresses a finding with a matching inline disable comment")
{
    const TempProject fixture;
    fixture.addFile(
        "suppressed.c",
        "// codeqa-disable-next-line signature.parameter-count\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(hasFinding(findings.files.front().suppressedFindings, "signature.parameter-count"));
}

TEST_CASE("ApplyRules does not suppress a finding when the comment names a different rule")
{
    const TempProject fixture;
    fixture.addFile(
        "not-suppressed.c",
        "// codeqa-disable-next-line complexity.cyclomatic\n"
        "int manyParams(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a;\n"
        "}\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(hasFinding(findings.files.front().findings, "signature.parameter-count"));
    CHECK(findings.files.front().suppressedFindings.empty());
}
```

(`hasFinding` already exists in the file's anonymous namespace and works on any `std::vector<codeqa::rules::Finding>`, so it applies to `suppressedFindings` unchanged.)

- [ ] **Step 2: Run tests to verify they fail to compile**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Expected: FAIL — `no member named 'suppressedFindings' in 'codeqa::rules::FileFindingsModel'`

- [ ] **Step 3: Add the field**

In `include/codeqa/rules/RuleModels.h`, extend `FileFindingsModel`:

```cpp
struct FileFindingsModel
{
    std::filesystem::path path;
    std::vector<Finding> findings;
    std::vector<Finding> suppressedFindings;
};
```

- [ ] **Step 4: Route suppressed findings into the new field**

In `src/application/ApplyRules.cpp`, the loop currently reads (around line 257):

```cpp
                for (codeqa::rules::Finding& finding : rule->evaluate(context))
                {
                    if (!isSuppressed(finding, file.comments, sourceLines))
                    {
                        fileFindings.findings.push_back(std::move(finding));
                    }
                }
```

Change it to:

```cpp
                for (codeqa::rules::Finding& finding : rule->evaluate(context))
                {
                    if (isSuppressed(finding, file.comments, sourceLines))
                    {
                        fileFindings.suppressedFindings.push_back(std::move(finding));
                    }
                    else
                    {
                        fileFindings.findings.push_back(std::move(finding));
                    }
                }
```

No other line in `ApplyRules.cpp` changes. `isSuppressed`/`suppressedRuleIdOnLine` are untouched.

- [ ] **Step 5: Build and run tests to verify they pass**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Then: `& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build -R codeqa_tests --output-on-failure`
Expected: PASS, including the two extended `ApplyRules` test cases.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/rules/RuleModels.h src/application/ApplyRules.cpp tests/unit/ApplyRulesTest.cpp
git commit -m "feat: route suppressed findings into FileFindingsModel instead of discarding them"
```

---

### Task 2: Thread `suppressedFindings` through `ReportDocument` (live path only)

**Files:**
- Modify: `include/codeqa/application/ReportDocument.h`
- Modify: `src/application/ReportDocumentFactory.cpp`
- Test: `tests/unit/ReportDocumentTest.cpp`

**Interfaces:**
- Consumes: `codeqa::rules::FileFindingsModel::suppressedFindings` (Task 1).
- Produces: `codeqa::application::ReportFile::suppressedFindings` (`std::vector<codeqa::rules::Finding>`), populated only by `makeReportDocument`. Task 3 reads this field from `AppController`'s `ReportDocument`.

- [ ] **Step 1: Add the failing assertions**

In `tests/unit/ReportDocumentTest.cpp`, extend `TEST_CASE("makeReportDocument maps fresh analysis without a JSON round trip")`:

```cpp
    codeqa::rules::FileFindingsModel findings;
    findings.path = file.path;
    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.observedValue = 12;
    findings.findings.push_back(finding);
    codeqa::rules::Finding suppressed;
    suppressed.ruleId = "signature.parameter-count";
    suppressed.symbolName = "run";
    findings.suppressedFindings.push_back(suppressed);
    analysis.findings.files.push_back(findings);
```

(Insert the three new lines — `suppressed`'s declaration and the `push_back`/comment above — right after the existing `findings.findings.push_back(finding);` line and before `analysis.findings.files.push_back(findings);`.) Then add an assertion alongside the existing ones:

```cpp
    CHECK(document.files.front().findings.front().ruleId == "complexity.cyclomatic");
    CHECK(document.files.front().suppressedFindings.front().ruleId == "signature.parameter-count");
```

Also extend `TEST_CASE("parseReportDocument accepts a schema-version-1 CodeQA report")` with one more assertion, proving the parse path never populates the field (the report JSON built by `makeCompleteReportJson()` has no `suppressedFindings` key at all — `toReportJson` never emits one):

```cpp
    CHECK(result.document->files.front().findings.front().ruleId == "complexity.cyclomatic");
    CHECK(result.document->files.front().suppressedFindings.empty());
```

- [ ] **Step 2: Run tests to verify they fail to compile**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Expected: FAIL — `no member named 'suppressedFindings' in 'codeqa::application::ReportFile'`

- [ ] **Step 3: Add the field**

In `include/codeqa/application/ReportDocument.h`, extend `ReportFile`:

```cpp
struct ReportFile
{
    std::filesystem::path path;
    codeqa::core::Language language{codeqa::core::Language::Unknown};
    codeqa::model::AnalysisState state{codeqa::model::AnalysisState::Unavailable};
    std::vector<codeqa::core::Diagnostic> diagnostics;
    std::vector<codeqa::metrics::FunctionMetricsModel> metrics;
    std::vector<codeqa::rules::Finding> findings;
    std::vector<codeqa::rules::Finding> suppressedFindings;
};
```

- [ ] **Step 4: Populate it on the live path only**

In `src/application/ReportDocumentFactory.cpp`, `makeReportDocument` currently has (around line 374):

```cpp
        file.findings = analysis.findings.files[index].findings;
```

Add immediately after it:

```cpp
        file.findings = analysis.findings.files[index].findings;
        file.suppressedFindings = analysis.findings.files[index].suppressedFindings;
```

Do **not** touch `parseFile` (the JSON-parsing function used by `parseReportDocument`, around line 269-298) — `ReportFile::suppressedFindings` stays default-constructed (empty) there, which is the desired behavior.

- [ ] **Step 5: Build and run tests to verify they pass**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug --target codeqa_tests -j 4`
Then: `& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build -R codeqa_tests --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/codeqa/application/ReportDocument.h src/application/ReportDocumentFactory.cpp tests/unit/ReportDocumentTest.cpp
git commit -m "feat: carry suppressed findings into ReportDocument on the live analysis path"
```

---

### Task 3: Desktop `SuppressedFindingListModel` + `AppController` wiring

**Files:**
- Modify: `src/desktop/ReportModels.h`
- Modify: `src/desktop/ReportModels.cpp`
- Modify: `src/desktop/AppController.h`
- Modify: `src/desktop/AppController.cpp`
- Test: `tests/desktop/AppControllerTest.cpp`

**Interfaces:**
- Consumes: `codeqa::application::ReportFile::suppressedFindings` (Task 2), via `AppController`'s `currentDocument_`.
- Produces: `AppController::suppressedFindings` (`Q_PROPERTY(SuppressedFindingListModel*)`, `CONSTANT`), with roles `ruleId`, `symbolName`, `path`, `line`, `column`. Task 4's QML reads this property and these role names.

- [ ] **Step 1: Write the failing test**

In `tests/desktop/AppControllerTest.cpp`, add a new fixture helper next to `writeTextFile`/`oneFinding` (same anonymous namespace, around line 124-127):

```cpp
QString sourceWithSuppressedFinding()
{
    return QStringLiteral(
        "// codeqa-disable-next-line signature.parameter-count\n"
        "int classify(int a, int b, int c, int d, int e)\n"
        "{\n"
        "    return a + b + c + d + e;\n"
        "}\n");
}
```

Add a new slot declaration to the `private slots:` list (after `analysisReportsRealPhasesAndFailedQualityGate();`, around line 165):

```cpp
    void suppressedFindingsAreExposedSeparatelyFromRegularFindings();
```

Add the test definition (place it right after `analysisReportsRealPhasesAndFailedQualityGate`'s own definition, around line 720, before `analysisModelResetSeesOneConsistentCompletedState`):

```cpp
void AppControllerTest::suppressedFindingsAreExposedSeparatelyFromRegularFindings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTextFile(
        directory.filePath("sample.c"),
        sourceWithSuppressedFinding().toUtf8());
    AppController controller(settingsPath(directory));

    controller.startAnalysis(QUrl::fromLocalFile(directory.path()));

    QTRY_VERIFY_WITH_TIMEOUT(!controller.analysisRunning(), 30000);
    QCOMPARE(controller.findings()->rowCount(), 0);
    QCOMPARE(controller.suppressedFindings()->rowCount(), 1);
    QCOMPARE(
        controller.suppressedFindings()->data(
            controller.suppressedFindings()->index(0), SuppressedFindingListModel::RuleIdRole).toString(),
        QString("signature.parameter-count"));
    QCOMPARE(
        controller.suppressedFindings()->data(
            controller.suppressedFindings()->index(0), SuppressedFindingListModel::SymbolNameRole).toString(),
        QString("classify"));
}
```

- [ ] **Step 2: Run test to verify it fails to compile**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_tests -j 4`
Expected: FAIL — `no member named 'suppressedFindings' in 'AppController'` (and `SuppressedFindingListModel` undeclared).

- [ ] **Step 3: Add `SuppressedFindingListModel` to the header**

In `src/desktop/ReportModels.h`, add this class after `FindingListModel`'s closing brace (after line 162) and before `class FileListModel`:

```cpp
class SuppressedFindingListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        RuleIdRole = Qt::UserRole + 1,
        SymbolNameRole,
        PathRole,
        LineRole,
        ColumnRole,
    };
    Q_ENUM(Role)

    explicit SuppressedFindingListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);

private:
    friend class AppController;

    // Row points directly into document_'s vectors -- same safety caveat as
    // FindingListModel::Row: only safe because every reassignment of
    // document_ (applyDocument()) is always immediately followed by
    // rebuildRows() in the same call.
    struct Row
    {
        const codeqa::application::ReportFile* file{nullptr};
        const codeqa::rules::Finding* finding{nullptr};
        QString path;
    };

    void rebuildRows();
    void beginDocumentReset();
    void applyDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void endDocumentReset();

    std::shared_ptr<const codeqa::application::ReportDocument> document_;
    std::vector<Row> rows_;
};
```

- [ ] **Step 4: Implement `SuppressedFindingListModel` in the .cpp**

In `src/desktop/ReportModels.cpp`, add this implementation after `FindingListModel::rebuildRows`'s closing brace (after line 605, right before `FileListModel::FileListModel`):

```cpp
SuppressedFindingListModel::SuppressedFindingListModel(QObject* parent) : QAbstractListModel(parent) {}

int SuppressedFindingListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant SuppressedFindingListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    const codeqa::rules::Finding& finding = *row.finding;
    switch (role)
    {
    case RuleIdRole:
        return QString::fromStdString(finding.ruleId);
    case SymbolNameRole:
        return QString::fromStdString(finding.symbolName);
    case PathRole:
        return row.path;
    case LineRole:
        return static_cast<qulonglong>(finding.location.line);
    case ColumnRole:
        return static_cast<qulonglong>(finding.location.column);
    default:
        return {};
    }
}

QHash<int, QByteArray> SuppressedFindingListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {RuleIdRole, "ruleId"},
        {SymbolNameRole, "symbolName"},
        {PathRole, "path"},
        {LineRole, "line"},
        {ColumnRole, "column"}};
    return roles;
}

void SuppressedFindingListModel::setDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    beginDocumentReset();
    applyDocument(std::move(document));
    endDocumentReset();
}

void SuppressedFindingListModel::beginDocumentReset()
{
    beginResetModel();
}

void SuppressedFindingListModel::applyDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    document_ = std::move(document);
    rebuildRows();
}

void SuppressedFindingListModel::endDocumentReset()
{
    endResetModel();
}

void SuppressedFindingListModel::rebuildRows()
{
    rows_.clear();

    if (document_)
    {
        for (const codeqa::application::ReportFile& file : document_->files)
        {
            const QString path = fromPath(file.path);
            for (const codeqa::rules::Finding& finding : file.suppressedFindings)
            {
                rows_.push_back(Row{&file, &finding, path});
            }
        }

        std::stable_sort(rows_.begin(), rows_.end(), [](const Row& left, const Row& right)
        {
            const int pathOrder = QString::compare(left.path, right.path, Qt::CaseInsensitive);
            if (pathOrder != 0)
            {
                return pathOrder < 0;
            }
            if (left.finding->location.line != right.finding->location.line)
            {
                return left.finding->location.line < right.finding->location.line;
            }
            return left.finding->location.column < right.finding->location.column;
        });
    }
}
```

(`fromPath` is the existing file-local helper at the top of `ReportModels.cpp` — already used by `FindingListModel`, no new include needed.)

- [ ] **Step 5: Wire the model into `AppController`**

In `src/desktop/AppController.h`, add a `Q_PROPERTY` after the `findings` one (line 34):

```cpp
    Q_PROPERTY(FindingListModel* findings READ findings CONSTANT)
    Q_PROPERTY(SuppressedFindingListModel* suppressedFindings READ suppressedFindings CONSTANT)
```

Add the getter declaration after `findings()`'s (line 58):

```cpp
    [[nodiscard]] FindingListModel* findings() const;
    [[nodiscard]] SuppressedFindingListModel* suppressedFindings() const;
```

Add the member after `findings_` (line 148):

```cpp
    FindingListModel findings_;
    SuppressedFindingListModel suppressedFindings_;
```

In `src/desktop/AppController.cpp`, add the getter implementation after `findings()`'s (line 153):

```cpp
FindingListModel* AppController::findings() const
{
    return const_cast<FindingListModel*>(&findings_);
}

SuppressedFindingListModel* AppController::suppressedFindings() const
{
    return const_cast<SuppressedFindingListModel*>(&suppressedFindings_);
}
```

In `replaceDocument` (around lines 868-890), add `suppressedFindings_` alongside `findings_` in the begin/apply/end sequence:

```cpp
    findings_.beginDocumentReset();
    suppressedFindings_.beginDocumentReset();
    files_.beginDocumentReset();
    if (snapshots.has_value())
    {
        snapshots_.beginSnapshotsReset();
    }

    currentDocument_ = next;
    findings_.applyDocument(next);
    suppressedFindings_.applyDocument(next);
    files_.applyDocument(next);
    if (snapshots.has_value())
    {
        snapshots_.applySnapshots(std::move(*snapshots));
    }
    qualityGateStatus_ = nextQualityGateStatus;
    errorMessage_ = std::move(message);

    if (snapshots.has_value())
    {
        snapshots_.endSnapshotsReset();
    }
    files_.endDocumentReset();
    suppressedFindings_.endDocumentReset();
    findings_.endDocumentReset();
```

- [ ] **Step 6: Build and run tests to verify they pass**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop codeqa_desktop_tests -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_tests --output-on-failure`
Expected: PASS, including the new `suppressedFindingsAreExposedSeparatelyFromRegularFindings` test.

- [ ] **Step 7: Commit**

```bash
git add src/desktop/ReportModels.h src/desktop/ReportModels.cpp src/desktop/AppController.h src/desktop/AppController.cpp tests/desktop/AppControllerTest.cpp
git commit -m "feat: expose suppressed findings to the desktop GUI via SuppressedFindingListModel"
```

---

### Task 4: QML "Suprimidos" tab on `ResultsPage.qml`

**Files:**
- Modify: `src/desktop/qml/pages/ResultsPage.qml`
- Test: `tests/desktop/qml/tst_DesktopShell.qml`

**Interfaces:**
- Consumes: `AppController::suppressedFindings` and its `ruleId`/`symbolName`/`path`/`line`/`column` roles (Task 3).
- Produces: nothing consumed by a later task — this is the last task.

- [ ] **Step 1: Add the fixture and failing test**

In `tests/desktop/qml/tst_DesktopShell.qml`, add a new `ListModel` fixture after `fixtureFindings`'s closing brace (after line 41):

```qml
    ListModel {
        id: fixtureSuppressedFindings

        ListElement {
            ruleId: "signature.parameter-count"
            symbolName: "manyParams"
            path: "suppressed.cpp"
            line: 4
            column: 1
        }
    }
```

Add `property var suppressedFindings: fixtureSuppressedFindings` to `fixtureController` (after the `findings` property, line 85):

```qml
        property var findings: fixtureFindings
        property var suppressedFindings: fixtureSuppressedFindings
```

Add a new test function after `test_completed_analysis_selects_results` (after line 456, before `test_selecting_a_finding_shows_its_details`):

```qml
    function test_suppressed_findings_tab_lists_rows_and_shows_empty_state() {
        const shell = createTemporaryObject(Qt.createComponent("qrc:/qt/qml/CodeQA/Desktop/Main.qml"), this)
        shell.controller = fixtureController
        shell.selectPage("Results")

        const suppressedList = shell.findChild("suppressedFindingsList")
        verify(suppressedList !== null)
        tryVerify(function() { return suppressedList.count === 1 })

        const row = suppressedList.itemAtIndex(0)
        verify(row !== null)

        fixtureSuppressedFindings.clear()
        tryVerify(function() { return suppressedList.count === 0 })

        const emptyLabel = shell.findChild("suppressedFindingsEmptyLabel")
        tryVerify(function() { return emptyLabel.visible === true })

        fixtureSuppressedFindings.append({
            ruleId: "signature.parameter-count",
            symbolName: "manyParams",
            path: "suppressed.cpp",
            line: 4,
            column: 1
        })
    }
```

(The final `append` restores the fixture to its original state so later test functions that reuse `fixtureController` are unaffected — matching this file's existing convention, see `resetFixtureFindings()`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_qml_smoke -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_qml_smoke --output-on-failure`
Expected: FAIL — `shell.findChild("suppressedFindingsList")` returns null (property/tab doesn't exist yet), or a QML property-binding error for `appController.suppressedFindings`.

- [ ] **Step 3: Add the third tab**

In `src/desktop/qml/pages/ResultsPage.qml`, add a third `TabButton` inside `resultsTabs` (after line 140, before the `TabBar`'s closing brace on line 141):

```qml
            TabButton {
                text: "Files and metrics"
            }

            TabButton {
                text: "Suprimidos"
            }
```

Add a third `StackLayout` child after the `fileMetricsList` `ListView`'s closing brace (after line 471, before `StackLayout`'s closing brace on line 472):

```qml
            ListView {
                id: suppressedFindingsList
                objectName: "suppressedFindingsList"
                clip: true
                model: root.controller ? root.controller.suppressedFindings : null
                spacing: 4

                delegate: Rectangle {
                    required property string ruleId
                    required property string symbolName
                    required property string path
                    required property var line
                    required property var column

                    width: suppressedFindingsList.width
                    height: suppressedRow.implicitHeight + 16
                    radius: 4
                    color: "#242832"

                    RowLayout {
                        id: suppressedRow
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 12

                        Label {
                            Layout.preferredWidth: 180
                            text: ruleId
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.fillWidth: true
                            text: path + ":" + line + ":" + column
                            elide: Text.ElideMiddle
                            textFormat: Text.PlainText
                        }

                        Label {
                            Layout.preferredWidth: 150
                            text: symbolName
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                    }
                }

                Label {
                    id: suppressedFindingsEmptyLabel
                    objectName: "suppressedFindingsEmptyLabel"
                    anchors.centerIn: parent
                    visible: suppressedFindingsList.count === 0
                    text: "No hay findings suprimidos en esta ejecución."
                }
            }
```

- [ ] **Step 4: Build and run test to verify it passes**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop_qml_smoke -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R codeqa_desktop_qml_smoke --output-on-failure`
Expected: PASS, all sub-tests including the new one.

- [ ] **Step 5: Full-suite regression check**

Run: `& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build buildQt --target codeqa_desktop codeqa_desktop_tests -j 4`
Then: `PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.11.0/llvm-mingw_64/bin:$PATH" ctest --test-dir buildQt -R "codeqa_desktop_qml_smoke|codeqa_desktop_tests" --output-on-failure`
Also, from the repo root, a full build+test of `codeqa_core`/`codeqa` (not just `codeqa_tests`) to catch any CLI-integration ctest target — per the duplication track's own final-review lesson, a target-restricted build silently skips those:
`& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build --config Debug -j 4` then `& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build --output-on-failure`
Expected: 100% pass, no regressions in findings/duplication/quality-gate/history tests.

- [ ] **Step 6: Commit**

```bash
git add src/desktop/qml/pages/ResultsPage.qml tests/desktop/qml/tst_DesktopShell.qml
git commit -m "feat: add read-only Suprimidos tab to the desktop Results page"
```

---

## Self-review notes

- **Spec coverage:** §4 (model extension) → Task 1. §3/§1 persistence decision (live-only, empty on parse) → Task 2 Step 1's second assertion. §6 (desktop model/controller) → Task 3. §7 (QML) → Task 4. §9 (testing strategy) → one test per task, matching the spec's four bullet points exactly. §8 edge cases (no suppressions → empty state, not hidden tab; stale directive → unchanged behavior, no new test needed since Task 1's existing "does not suppress" test already covers the mismatched-rule-id case) → covered by Task 4's empty-state assertion and Task 1's existing coverage respectively.
- **No new report/JSON schema fields** — verified no task touches `codeqa::report::renderText`/`toReportJson`/`CONFIG0xx`.
- **Type consistency:** `SuppressedFindingListModel` name and its `Role` enum values (`RuleIdRole`, `SymbolNameRole`, `PathRole`, `LineRole`, `ColumnRole`) are used identically in Task 3 (C++ definition + `AppControllerTest.cpp`) and Task 4 (QML role names `ruleId`/`symbolName`/`path`/`line`/`column`, and `objectName: "suppressedFindingsList"`/`"suppressedFindingsEmptyLabel"`).
