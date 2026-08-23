# Unparenthesized Macro Parameter Rule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `preprocessor.unparenthesized-macro-parameter`, the fourth and final rule in roadmap v2 track #3, which flags a function-like macro parameter used in its body without safe bracketing (the classic `#define SQUARE(x) x*x` bug). Unlike the three prior rules in this track, a macro is not inside any function, so `Rule::evaluate(RuleContext{metrics, callable})` cannot see it at all — this plan first teaches the frontend/model about `#define`, then builds a brand-new, smaller-scoped `FileRule` mechanism (parallel to `Rule`, evaluated once per file) before implementing the rule itself on top of it.

**Architecture:** Two new tree-sitter queries extract `preproc_def`/`preproc_function_def` nodes into a new `frontend::MacroModel` (Task 1), bridged into a parallel `model::MacroModel` on `FileModel::macros` and serialized to JSON (Task 2). A new `codeqa::rules::FileRule` interface (`evaluate(FileRuleContext{file})`) is added alongside the existing `Rule`, with its own factory `allFileRules(config)` sharing the same `config.rules` map (Task 3, which also implements `UnparenthesizedMacroParameterRule` and registers it in every rule-id-list surface). `ApplyRules.cpp` gains a second, file-scoped evaluation loop that feeds the exact same `FileFindingsModel`/suppression/JSON pipeline as every callable-scoped rule (Task 4). The desktop GUI's rule list picks up the new id last (Task 5). No task changes `Rule`, `RuleContext`, or any of the 11 existing rules.

**Tech Stack:** C++17, tree-sitter (vendored `tree-sitter-c`/`tree-sitter-cpp`), doctest (`codeqa_tests`), Qt6/QML (`AnalyzePage.qml`), CMake (MSVC `build/` for `codeqa_tests`, MinGW/MSVC `buildQt/` for the desktop app and its tests).

**Spec:** `docs/superpowers/specs/2026-08-22-macros-parametros-sin-parentesis-design.md`

## Global Constraints

- New rule id, exact string: `preprocessor.unparenthesized-macro-parameter`.
- New frontend struct `frontend::MacroModel{name, isFunctionLike, parameters, isVariadic, body, range}`; new model struct `model::MacroModel{symbol, isFunctionLike, parameters, isVariadic, body}`. Both added to `include/codeqa/frontend/StructuralModels.h` / `include/codeqa/model/Models.h` respectively — never merged into one shared type.
- `model::MacroModel::symbol` is built as a **flat** `SymbolIdentity{name, name, range}` in `ProjectModelBuilder.cpp` — deliberately bypassing `identityFor()` (macros are never namespace/class-scoped by the preprocessor), unlike every other symbol kind bridged in that function.
- The macro body (`value` field of `preproc_def`/`preproc_function_def`) is an opaque raw-text string. Detection is a text/word-boundary scan over that string — never AST traversal — matching this codebase's existing `security.unsafe-string-api` text-scan style.
- Detection heuristic (exact, do not weaken): a parameter occurrence in the body is *safe* only when the nearest non-space character before it is one of `( [ ,` (or body start) **and** the nearest non-space character after it is one of `) ] ,` (or body end). A parameter is flagged if it has **at least one** occurrence that fails this test. One `Finding` per flagged parameter (not per occurrence, not per macro). Only function-like macros are checked; an object-like macro is always skipped.
- New `FileRule` interface (`include/codeqa/rules/FileRule.h`) is strictly additive: it must not change `Rule`, `RuleContext`, `Rule.h`, or any of the 11 existing rules' behavior.
- `FileRuleContext` holds `const codeqa::model::FileModel& file` by reference — never a copy.
- `config.rules` is a single flat `std::map<std::string, RuleConfig>` shared by both `allRules()` and `allFileRules()` ids — the new rule id lives in the same map as the 11 existing ones, default `thresholds(0, 0)` (enabled, no numeric limits).
- Finding shape for the new rule: `severity = Warning`; `description = "Parameter '<p>' of macro '<name>' is used without enclosing parentheses in the macro body."`; `recommendation = "Wrap the parameter as (<p>) everywhere it appears in the macro body to avoid operator-precedence bugs at the call site."`; `location = macro.symbol.range.begin`; `symbolName = macro.symbol.qualifiedName`.
- Desktop GUI: hide `warningLimitField`/`highLimitField` for `"preprocessor.unparenthesized-macro-parameter"` too — a 4th exclusion clause alongside the three existing ones. Extend, do not replace.
- Rule-id count moves 11 → 12 everywhere it is hardcoded (`DefaultConfig.cpp`, `ConfigJson.cpp::knownRuleIds()`, `Rules.h` doc comment, `ReportJsonTest.cpp`, `DefaultConfigTest.cpp`, `AppControllerTest.cpp` — both `rowCount()` call sites plus `expectedRuleIds`).
- Out of scope, do not add in this plan: whole-macro-body parenthesization; multi-statement `do {} while(0)` checks; `__VA_ARGS__`/variadic parameter checking; string/char-literal-aware scanning; an object-like "prefer constexpr" rule; any second macro rule; any change to `Rule::evaluate`'s signature.

---

### Task 1: Extract `frontend::MacroModel` from `#define`s

**Files:**
- Modify: `include/codeqa/frontend/StructuralModels.h:149-156` (new `MacroModel` struct)
- Modify: `include/codeqa/frontend/ParsedFile.h:26-27` (new `macros` field)
- Create: `src/frontend/queries/c/macros.scm`
- Create: `src/frontend/queries/cpp/macros.scm`
- Modify: `CMakeLists.txt:25-44` (query file list + embedding)
- Modify: `cmake/EmbeddedQueries.h.in:36-40` (2 new embedded blocks)
- Modify: `src/frontend/StructuralExtractor.cpp` (new `adaptMacro()`, `hasSameMacroRange()`, `Impl` query members/accessor, new extraction pass)
- Modify: `tests/unit/StructuralExtractorTest.cpp` (4 new test cases)

**Interfaces:**
- Consumes: existing frontend helpers `childForField(TSNode, const char*)`, `nodeText(TSNode, std::string_view)`, `isNodeType(TSNode, const char*)`, `trim(std::string)`, `sourceRange(TSNode, const std::filesystem::path&)` (all in `src/frontend/StructuralExtractor.cpp`'s anonymous namespace); `compileQuery(...)` and the `embedded_queries::` namespace pattern already used for `cIncludes`/`cppIncludes`.
- Produces: `codeqa::frontend::MacroModel{name, isFunctionLike, parameters, isVariadic, body, range}` and `ParsedFile::macros`, sorted/deduped by range — consumed by Task 2's `ProjectModelBuilder.cpp` bridging.

- [ ] **Step 1: Write the failing frontend tests**

Open `tests/unit/StructuralExtractorTest.cpp`. The test case `"extracts file scope globals from C"` currently ends the relevant block at line 829 with a closing `}`. Insert these 4 new test cases immediately after it:

```cpp
TEST_CASE("extracts an object-like macro with no parameters")
{
    const TempSourceFile file{
        "object-like-macro.c",
        "#define MAX_SIZE 128\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.macros.size() == 1);
    CHECK(result.macros[0].name == "MAX_SIZE");
    CHECK_FALSE(result.macros[0].isFunctionLike);
    CHECK(result.macros[0].parameters.empty());
    CHECK_FALSE(result.macros[0].isVariadic);
    CHECK(result.macros[0].body == "128");
}

TEST_CASE("extracts a function-like macro with parameters")
{
    const TempSourceFile file{
        "function-like-macro.c",
        "#define SQUARE(x) x*x\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.macros.size() == 1);
    CHECK(result.macros[0].name == "SQUARE");
    CHECK(result.macros[0].isFunctionLike);
    REQUIRE(result.macros[0].parameters.size() == 1);
    CHECK(result.macros[0].parameters[0] == "x");
    CHECK_FALSE(result.macros[0].isVariadic);
    CHECK(result.macros[0].body == "x*x");
}

TEST_CASE("extracts a variadic function-like macro, recording only the named parameter")
{
    const TempSourceFile file{
        "variadic-macro.c",
        "#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.macros.size() == 1);
    CHECK(result.macros[0].name == "LOG");
    CHECK(result.macros[0].isFunctionLike);
    REQUIRE(result.macros[0].parameters.size() == 1);
    CHECK(result.macros[0].parameters[0] == "fmt");
    CHECK(result.macros[0].isVariadic);
    CHECK(result.macros[0].body == "printf(fmt, __VA_ARGS__)");
}

TEST_CASE("extracts a macro with no value without crashing")
{
    const TempSourceFile file{
        "flag-macro.c",
        "#define FLAG\n"};
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;

    const codeqa::frontend::ParsedFile result = codeqa::frontend::parseSourceFile(
        file.path(), codeqa::frontend::Grammar::C, "detected language", parser, extractor);

    REQUIRE(result.macros.size() == 1);
    CHECK(result.macros[0].name == "FLAG");
    CHECK_FALSE(result.macros[0].isFunctionLike);
    CHECK(result.macros[0].body.empty());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*macro*"
```
Expected: FAIL to compile — `codeqa::frontend::ParsedFile` has no member `macros` yet.

- [ ] **Step 3: Add `MacroModel` to `StructuralModels.h`**

In `include/codeqa/frontend/StructuralModels.h`, immediately after the `TypeAliasModel` struct (ends at line 154 with `};`) and before the closing `} // namespace codeqa::frontend` (line 156), insert:

```cpp
struct MacroModel
{
    std::string name;
    bool isFunctionLike{false};
    std::vector<std::string> parameters;
    bool isVariadic{false};
    // Raw, unparsed replacement-list text (tree-sitter never parses a
    // macro body as C++); empty if the macro has no value (`#define FLAG`).
    std::string body;
    core::SourceRange range;
};
```

- [ ] **Step 4: Add the `macros` field to `ParsedFile`**

In `include/codeqa/frontend/ParsedFile.h`, immediately after `std::vector<TypeAliasModel> typeAliases;` (line 27), add:

```cpp
    std::vector<MacroModel> macros;
```

- [ ] **Step 5: Add the two query files**

Create `src/frontend/queries/c/macros.scm`:

```
[(preproc_def) (preproc_function_def)] @macro
```

Create `src/frontend/queries/cpp/macros.scm` with the identical content:

```
[(preproc_def) (preproc_function_def)] @macro
```

- [ ] **Step 6: Wire the two new query files into CMake**

In `CMakeLists.txt`, the `CODEQA_QUERY_FILES` list currently ends (line 33):

```cmake
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm
)
```

Change to:

```cmake
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/macros.scm
    ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/macros.scm
)
```

The `file(READ ...)` block currently ends (line 44):

```cmake
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm CODEQA_CPP_DECLARATIONS_QUERY)
```

Add two new lines right after it:

```cmake
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/declarations.scm CODEQA_CPP_DECLARATIONS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/c/macros.scm CODEQA_C_MACROS_QUERY)
file(READ ${PROJECT_SOURCE_DIR}/src/frontend/queries/cpp/macros.scm CODEQA_CPP_MACROS_QUERY)
```

- [ ] **Step 7: Add the two new embedded-query blocks**

In `cmake/EmbeddedQueries.h.in`, immediately after the `cppDeclarations` block (ends line 38 with `)CQPD";`) and before the closing `} // namespace` (line 40), insert:

```cpp
inline constexpr std::string_view cMacros = R"CQCM(
@CODEQA_C_MACROS_QUERY@
)CQCM";

inline constexpr std::string_view cppMacros = R"CQPM(
@CODEQA_CPP_MACROS_QUERY@
)CQPM";
```

- [ ] **Step 8: Add `adaptMacro()` in `StructuralExtractor.cpp`**

In `src/frontend/StructuralExtractor.cpp`, immediately after `adaptInclude()` (ends at line 782 with `}`) and before `isFileOrNamespaceScopeDeclaration()` (line 784), insert:

```cpp
MacroModel adaptMacro(TSNode candidate, std::string_view source, const std::filesystem::path& path)
{
    const TSNode nameNode = childForField(candidate, "name");

    MacroModel model;
    model.name = ts_node_is_null(nameNode) ? std::string{} : nodeText(nameNode, source);
    model.isFunctionLike = isNodeType(candidate, "preproc_function_def");

    if (model.isFunctionLike)
    {
        const TSNode parametersNode = childForField(candidate, "parameters");
        const uint32_t childCount = ts_node_child_count(parametersNode);

        for (uint32_t index = 0; index < childCount; ++index)
        {
            const TSNode child = ts_node_child(parametersNode, index);

            if (isNodeType(child, "identifier"))
            {
                model.parameters.push_back(nodeText(child, source));
            }
            else if (isNodeType(child, "..."))
            {
                model.isVariadic = true;
            }
        }
    }

    const TSNode valueNode = childForField(candidate, "value");
    model.body = ts_node_is_null(valueNode) ? std::string{} : trim(nodeText(valueNode, source));
    model.range = sourceRange(candidate, path);
    return model;
}
```

- [ ] **Step 9: Add `hasSameMacroRange()` next to the other dedup predicates**

In the same file, immediately after `hasSameTypeAliasRange()` (ends at line 1324 with `}`) and before the closing `} // namespace` (line 1326), insert:

```cpp
bool hasSameMacroRange(const MacroModel& left, const MacroModel& right)
{
    return left.range == right.range;
}
```

- [ ] **Step 10: Add the query members and accessor to `Impl`**

In the same file, `StructuralExtractor::Impl`'s constructor initializer list currently ends (lines 1340-1341):

```cpp
        , cppDeclarations_(compileQuery(
              tree_sitter_cpp(), "queries/cpp/declarations.scm", embedded_queries::cppDeclarations))
    {
    }
```

Change to:

```cpp
        , cppDeclarations_(compileQuery(
              tree_sitter_cpp(), "queries/cpp/declarations.scm", embedded_queries::cppDeclarations))
        , cMacros_(compileQuery(tree_sitter_c(), "queries/c/macros.scm", embedded_queries::cMacros))
        , cppMacros_(compileQuery(tree_sitter_cpp(), "queries/cpp/macros.scm", embedded_queries::cppMacros))
    {
    }
```

The `declarationsFor()` accessor currently reads (lines 1360-1363):

```cpp
    [[nodiscard]] TSQuery* declarationsFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cDeclarations_.get() : cppDeclarations_.get();
    }
```

Add a new accessor right after it:

```cpp
    [[nodiscard]] TSQuery* declarationsFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cDeclarations_.get() : cppDeclarations_.get();
    }

    [[nodiscard]] TSQuery* macrosFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cMacros_.get() : cppMacros_.get();
    }
```

The private member list currently ends (lines 1366-1373):

```cpp
private:
    QueryPointer cFunctions_;
    QueryPointer cTypes_;
    QueryPointer cIncludes_;
    QueryPointer cDeclarations_;
    QueryPointer cppFunctions_;
    QueryPointer cppTypes_;
    QueryPointer cppIncludes_;
    QueryPointer cppDeclarations_;
};
```

Change to:

```cpp
private:
    QueryPointer cFunctions_;
    QueryPointer cTypes_;
    QueryPointer cIncludes_;
    QueryPointer cDeclarations_;
    QueryPointer cppFunctions_;
    QueryPointer cppTypes_;
    QueryPointer cppIncludes_;
    QueryPointer cppDeclarations_;
    QueryPointer cMacros_;
    QueryPointer cppMacros_;
};
```

- [ ] **Step 11: Add the extraction pass in `extract()`**

In the same file, `extract()`'s final block (the `typeAliases` sort/dedup) currently ends at line 1532 with `result.typeAliases.end());` followed by the function's closing `}` (line 1533). Immediately before that closing `}`, insert a new pass:

```cpp
    ts_query_cursor_exec(cursor.get(), impl_->macrosFor(grammar), root);

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            result.macros.push_back(adaptMacro(match.captures[index].node, source, path));
        }
    }

    std::sort(
        result.macros.begin(), result.macros.end(),
        [](const MacroModel& left, const MacroModel& right) { return left.range < right.range; });
    result.macros.erase(
        std::unique(result.macros.begin(), result.macros.end(), hasSameMacroRange),
        result.macros.end());
```

- [ ] **Step 12: Run the tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*macro*"
```
Expected: PASS (4 test cases, 0 failures).

- [ ] **Step 13: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures (confirms `EmbeddedQueriesTest.cpp` and `FrontendPublicApiTest.cpp` did not regress from the new query wiring).

- [ ] **Step 14: Commit**

```bash
git add include/codeqa/frontend/StructuralModels.h include/codeqa/frontend/ParsedFile.h src/frontend/queries/c/macros.scm src/frontend/queries/cpp/macros.scm CMakeLists.txt cmake/EmbeddedQueries.h.in src/frontend/StructuralExtractor.cpp tests/unit/StructuralExtractorTest.cpp
git commit -m "feat: extract MacroModel from #define via new tree-sitter queries"
```

---

### Task 2: Bridge `MacroModel` into `codeqa::model` and the JSON report

**Files:**
- Modify: `include/codeqa/model/Models.h:162-188` (new `MacroModel` struct + `FileModel::macros`)
- Modify: `src/model/ProjectModelBuilder.cpp:426-449` (bridging loop + sort)
- Modify: `src/report/FileJson.cpp:190-321` (`macroJson()` + `document["macros"]`)
- Modify: `tests/unit/ProjectModelBuilderTest.cpp` (2 new test cases)
- Modify: `tests/unit/FileJsonTest.cpp` (1 new test case)

**Interfaces:**
- Consumes: `codeqa::frontend::MacroModel` and `ParsedFile::macros` (Task 1).
- Produces: `codeqa::model::MacroModel{symbol, isFunctionLike, parameters, isVariadic, body}` and `FileModel::macros` — consumed by Task 3's `FileRuleContext{file}` and by Task 4's `ApplyRules.cpp` loop.

- [ ] **Step 1: Write the failing model/report tests**

Open `tests/unit/ProjectModelBuilderTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 612):

```cpp
TEST_CASE("ProjectModelBuilder bridges a frontend macro into the model with a flat SymbolIdentity")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/sample.h"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::C;
    sourceFile.grammar = codeqa::frontend::Grammar::C;
    sourceFile.parsed = true;
    sourceFile.macros.push_back(codeqa::frontend::MacroModel{
        "SQUARE", true, {"x"}, false, "x*x", rangeFor(sourcePath, 0, 20)});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].macros.size() == 1);
    const codeqa::model::MacroModel& macro = result.files[0].macros[0];
    CHECK(macro.symbol.name == "SQUARE");
    CHECK(macro.symbol.qualifiedName == "SQUARE");
    CHECK(macro.isFunctionLike);
    REQUIRE(macro.parameters.size() == 1);
    CHECK(macro.parameters[0] == "x");
    CHECK_FALSE(macro.isVariadic);
    CHECK(macro.body == "x*x");
}

TEST_CASE("ProjectModelBuilder gives a macro a flat qualifiedName even inside an enclosing namespace")
{
    const std::filesystem::path rootPath{"project"};
    const std::filesystem::path sourcePath{"project/src/sample.h"};

    codeqa::frontend::ParsedFile sourceFile;
    sourceFile.path = sourcePath;
    sourceFile.language = codeqa::core::Language::Cpp;
    sourceFile.grammar = codeqa::frontend::Grammar::Cpp;
    sourceFile.parsed = true;
    sourceFile.classes = {
        codeqa::frontend::ClassModel{
            "app", codeqa::frontend::ClassKind::Namespace, rangeFor(sourcePath, 0, 100)}};
    sourceFile.macros.push_back(codeqa::frontend::MacroModel{
        "MAX_SIZE", false, {}, false, "128", rangeFor(sourcePath, 10, 30)});

    const codeqa::model::ProjectModel result = codeqa::model::ProjectModelBuilder{}.build(
        rootPath, std::vector<codeqa::frontend::ParsedFile>{sourceFile});

    REQUIRE(result.files.size() == 1);
    REQUIRE(result.files[0].macros.size() == 1);
    CHECK(result.files[0].macros[0].symbol.name == "MAX_SIZE");
    CHECK(result.files[0].macros[0].symbol.qualifiedName == "MAX_SIZE");
}
```

Open `tests/unit/FileJsonTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 161):

```cpp
TEST_CASE("fileJson serializes a macro with its parameters and body")
{
    codeqa::model::FileModel file;
    file.path = "src/macros.h";
    file.language = codeqa::core::Language::C;

    codeqa::model::MacroModel macro;
    macro.symbol.name = "SQUARE";
    macro.symbol.qualifiedName = "SQUARE";
    macro.isFunctionLike = true;
    macro.parameters = {"x"};
    macro.isVariadic = false;
    macro.body = "x*x";
    file.macros.push_back(macro);

    const nlohmann::json document = codeqa::report::fileJson(file);

    REQUIRE(document.at("macros").size() == 1);
    const nlohmann::json& macroJson = document.at("macros")[0];
    CHECK(macroJson.at("qualifiedName").get<std::string>() == "SQUARE");
    CHECK(macroJson.at("isFunctionLike").get<bool>());
    REQUIRE(macroJson.at("parameters").size() == 1);
    CHECK(macroJson.at("parameters")[0].get<std::string>() == "x");
    CHECK_FALSE(macroJson.at("isVariadic").get<bool>());
    CHECK(macroJson.at("body").get<std::string>() == "x*x");
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*macro*"
```
Expected: FAIL to compile — `codeqa::model::MacroModel` and `FileModel::macros` do not exist yet.

- [ ] **Step 3: Add `MacroModel` and `FileModel::macros` to `Models.h`**

In `include/codeqa/model/Models.h`, immediately after the `TypeAliasModel` struct (ends at line 166 with `};`) and before `DependencyModel` (line 168), insert:

```cpp
struct MacroModel
{
    SymbolIdentity symbol;
    bool isFunctionLike{false};
    std::vector<std::string> parameters;
    bool isVariadic{false};
    std::string body;
};

```

In `FileModel` (currently lines 175-188), immediately after `std::vector<TypeAliasModel> typeAliases;` (line 186), add:

```cpp
    std::vector<MacroModel> macros;
```

- [ ] **Step 4: Bridge macros in `ProjectModelBuilder.cpp`**

In `src/model/ProjectModelBuilder.cpp`, immediately after the `typeAliases` bridging loop (ends at line 430 with `}`) and before the `sortByRange(model.dependencies, ...)` call (line 432), insert:

```cpp
    for (const frontend::MacroModel& macro : file.macros)
    {
        model.macros.push_back(MacroModel{
            SymbolIdentity{macro.name, macro.name, macro.range},
            macro.isFunctionLike,
            macro.parameters,
            macro.isVariadic,
            macro.body});
    }
```

Immediately after `sortByRange(model.typeAliases, ...)` (ends at line 449 with `});`) and before the blank line preceding `return model;` (line 451), insert:

```cpp
    sortByRange(model.macros, [](const MacroModel& macro) -> const core::SourceRange& {
        return macro.symbol.range;
    });
```

- [ ] **Step 5: Serialize macros in `FileJson.cpp`**

In `src/report/FileJson.cpp`, immediately after `typeAliasJson()` (ends at line 195 with `}`) and before `parameterJson()` (line 197), insert:

```cpp
Json macroJson(const codeqa::model::MacroModel& macro)
{
    Json document = symbolIdentityJson(macro.symbol);
    document["isFunctionLike"] = macro.isFunctionLike;
    document["parameters"] = macro.parameters;
    document["isVariadic"] = macro.isVariadic;
    document["body"] = macro.body;
    return document;
}
```

Immediately after the `document["typeAliases"]` loop (ends at line 321 with `}`) and before the `document["comments"]` loop (line 323), insert:

```cpp
    document["macros"] = Json::array();
    for (const codeqa::model::MacroModel& macro : file.macros)
    {
        document["macros"].push_back(macroJson(macro));
    }
```

- [ ] **Step 6: Run the tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*macro*"
```
Expected: PASS (7 test cases total: 4 from Task 1, 3 new here).

- [ ] **Step 7: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures.

- [ ] **Step 8: Commit**

```bash
git add include/codeqa/model/Models.h src/model/ProjectModelBuilder.cpp src/report/FileJson.cpp tests/unit/ProjectModelBuilderTest.cpp tests/unit/FileJsonTest.cpp
git commit -m "feat: bridge MacroModel into codeqa::model and the JSON report"
```

---

### Task 3: Add the `FileRule` mechanism and implement `preprocessor.unparenthesized-macro-parameter`

**Files:**
- Create: `include/codeqa/rules/FileRule.h`
- Modify: `include/codeqa/rules/Rules.h` (include + `allFileRules()` declaration + doc comment 11 → 12)
- Modify: `src/rules/Rules.cpp` (new includes, helpers, rule class, `allFileRules()`)
- Modify: `src/config/DefaultConfig.cpp` (new default config entry)
- Modify: `src/config/ConfigJson.cpp` (`knownRuleIds()` — append the id)
- Modify: `tests/CMakeLists.txt:32` (register the new test file)
- Create: `tests/unit/FileRulesTest.cpp`
- Modify: `tests/unit/DefaultConfigTest.cpp` (count 11 → 12 + new `checkRule` line)
- Modify: `tests/unit/ReportJsonTest.cpp:52` (rule-count assertion 11 → 12)
- Modify: `tests/corpus/validation/RULE-STABILITY.md` (new row)

**Interfaces:**
- Consumes: `codeqa::model::FileModel`/`codeqa::model::MacroModel` (Task 2); `codeqa::rules::Finding` (`RuleModels.h`); the existing `thresholds()` helper in `DefaultConfig.cpp`.
- Produces: `codeqa::rules::FileRule`, `codeqa::rules::FileRuleContext{file}`, `codeqa::rules::allFileRules(config)` returning `std::vector<std::unique_ptr<FileRule>>` — consumed by Task 4's `ApplyRules.cpp`. Rule id string `"preprocessor.unparenthesized-macro-parameter"` — consumed by Task 5's desktop wiring.

- [ ] **Step 1: Write the failing rule tests**

Create `tests/unit/FileRulesTest.cpp`:

```cpp
#include "doctest/doctest.h"

#include "codeqa/config/DefaultConfig.h"
#include "codeqa/rules/Rules.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const codeqa::rules::FileRule& fileRuleNamed(
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>>& rules, const std::string& id)
{
    for (const auto& rule : rules)
    {
        if (rule->id() == id)
        {
            return *rule;
        }
    }

    throw std::runtime_error("Expected file rule was not registered: " + id);
}

codeqa::model::MacroModel makeMacro(
    std::string name, std::vector<std::string> parameters, std::string body, bool isFunctionLike = true)
{
    codeqa::model::MacroModel macro;
    macro.symbol.name = name;
    macro.symbol.qualifiedName = std::move(name);
    macro.symbol.range.begin.line = 5;
    macro.symbol.range.begin.column = 1;
    macro.isFunctionLike = isFunctionLike;
    macro.parameters = std::move(parameters);
    macro.body = std::move(body);
    return macro;
}

} // namespace

TEST_CASE("preprocessor.unparenthesized-macro-parameter flags a parameter used with an operator")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SQUARE", {"x"}, "x*x"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].ruleId == "preprocessor.unparenthesized-macro-parameter");
    CHECK(findings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(
        findings[0].description
        == "Parameter 'x' of macro 'SQUARE' is used without enclosing parentheses in the macro body.");
    CHECK(
        findings[0].recommendation
        == "Wrap the parameter as (x) everywhere it appears in the macro body to avoid "
           "operator-precedence bugs at the call site.");
    CHECK(findings[0].location.line == 5);
    CHECK(findings[0].symbolName == "SQUARE");
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter ignores a fully parenthesized parameter")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SQUARE", {"x"}, "((x)*(x))"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter treats a sole call argument as safe")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("LOG", {"x"}, "printf(x)"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter treats a sole subscript index as safe")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("AT", {"i"}, "buffer[i]"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter flags the array operand of a subscript, "
          "but not the index")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("AT", {"arr", "i"}, "arr[i]"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].description.find("'arr'") != std::string::npos);
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter produces one finding per unsafe parameter")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("ADD", {"a", "b"}, "a+b"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 2);
    CHECK(findings[0].description.find("'a'") != std::string::npos);
    CHECK(findings[1].description.find("'b'") != std::string::npos);
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter never fires on an object-like macro")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("MAX_SIZE", {}, "128", false));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter ignores a parameter never used in the body")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("UNUSED", {"x"}, "0"));

    CHECK(rule.evaluate(codeqa::rules::FileRuleContext{file}).empty());
}

TEST_CASE("preprocessor.unparenthesized-macro-parameter checks every macro in the file independently")
{
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> rules =
        codeqa::rules::allFileRules(codeqa::config::defaultConfig());
    const codeqa::rules::FileRule& rule =
        fileRuleNamed(rules, "preprocessor.unparenthesized-macro-parameter");

    codeqa::model::FileModel file;
    file.macros.push_back(makeMacro("SAFE", {"x"}, "((x))"));
    file.macros.push_back(makeMacro("UNSAFE", {"y"}, "y*y"));

    const std::vector<codeqa::rules::Finding> findings =
        rule.evaluate(codeqa::rules::FileRuleContext{file});

    REQUIRE(findings.size() == 1);
    CHECK(findings[0].description.find("'y'") != std::string::npos);
}
```

In `tests/CMakeLists.txt`, immediately after `unit/RulesTest.cpp` (line 32), add:

```cmake
    unit/FileRulesTest.cpp
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="preprocessor.unparenthesized*"
```
Expected: FAIL to compile — `codeqa::rules::FileRule`/`FileRuleContext`/`allFileRules` do not exist yet.

- [ ] **Step 3: Create `FileRule.h`**

Create `include/codeqa/rules/FileRule.h`:

```cpp
#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <string>
#include <vector>

namespace codeqa::rules
{

struct FileRuleContext
{
    const codeqa::model::FileModel& file;
};

class FileRule
{
public:
    virtual ~FileRule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const FileRuleContext& context) const = 0;
};

} // namespace codeqa::rules
```

- [ ] **Step 4: Declare `allFileRules()` in `Rules.h`**

Replace the full contents of `include/codeqa/rules/Rules.h` with:

```cpp
#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/rules/FileRule.h"
#include "codeqa/rules/Rule.h"

#include <memory>
#include <vector>

namespace codeqa::rules
{

/// Requires config.rules to contain an entry for each of the 12 known rule ids
/// (shared with allFileRules -- config.rules is one flat map for both Rule and
/// FileRule ids) -- guaranteed by codeqa::config::defaultConfig() and by
/// codeqa::config::mergeConfig() (which always starts from a full copy of the
/// defaults). A default-constructed codeqa::config::ProjectConfig{} does NOT
/// satisfy this and will throw std::out_of_range from config.rules.at() inside
/// this function.
[[nodiscard]] std::vector<std::unique_ptr<Rule>> allRules(const codeqa::config::ProjectConfig& config);

/// Same precondition as allRules above -- one shared config.rules map.
[[nodiscard]] std::vector<std::unique_ptr<FileRule>> allFileRules(const codeqa::config::ProjectConfig& config);

} // namespace codeqa::rules
```

- [ ] **Step 5: Implement the rule in `Rules.cpp`**

In `src/rules/Rules.cpp`, the includes currently read (lines 1-12):

```cpp
#include "codeqa/rules/Rules.h"

#include "codeqa/model/ParameterUtilities.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
```

Add `<cctype>` and `<cstddef>`:

```cpp
#include "codeqa/rules/Rules.h"

#include "codeqa/model/ParameterUtilities.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
```

Immediately after `ArrayParameterDecayRule` (ends at line 476 with `};`) and before the anonymous namespace's closing `} // namespace` (line 478), insert:

```cpp
bool isWordBoundaryChar(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

std::size_t findNextWholeWord(std::string_view text, std::string_view word, std::size_t from)
{
    for (std::size_t pos = text.find(word, from); pos != std::string_view::npos;
         pos = text.find(word, pos + 1))
    {
        const bool leftBoundary = pos == 0 || !isWordBoundaryChar(text[pos - 1]);
        const bool rightBoundary =
            pos + word.size() >= text.size() || !isWordBoundaryChar(text[pos + word.size()]);

        if (leftBoundary && rightBoundary)
        {
            return pos;
        }
    }

    return std::string_view::npos;
}

// Returns true as soon as any occurrence of `parameter` in `body` is not safely
// bracketed -- "safe" meaning the nearest non-space character before it is one of
// ( [ , (or body start) AND the nearest non-space character after it is one of
// ) ] , (or body end). Deliberately permissive: a sole call argument (LOG(x)) or
// sole subscript index (arr[x]) is safe, only a juxtaposition with an operator or
// another token is flagged.
bool hasUnwrappedOccurrence(std::string_view body, std::string_view parameter)
{
    for (std::size_t pos = findNextWholeWord(body, parameter, 0); pos != std::string_view::npos;
         pos = findNextWholeWord(body, parameter, pos + parameter.size()))
    {
        std::size_t before = pos;
        while (before > 0 && std::isspace(static_cast<unsigned char>(body[before - 1])) != 0)
        {
            --before;
        }

        std::size_t after = pos + parameter.size();
        while (after < body.size() && std::isspace(static_cast<unsigned char>(body[after])) != 0)
        {
            ++after;
        }

        const bool safeBefore =
            before == 0 || body[before - 1] == '(' || body[before - 1] == '[' || body[before - 1] == ',';
        const bool safeAfter =
            after >= body.size() || body[after] == ')' || body[after] == ']' || body[after] == ',';

        if (!safeBefore || !safeAfter)
        {
            return true;
        }
    }

    return false;
}

class UnparenthesizedMacroParameterRule : public FileRule
{
public:
    [[nodiscard]] std::string id() const override
    {
        return "preprocessor.unparenthesized-macro-parameter";
    }

    [[nodiscard]] bool appliesTo(codeqa::core::Language) const override { return true; }

    [[nodiscard]] std::vector<Finding> evaluate(const FileRuleContext& context) const override
    {
        std::vector<Finding> findings;

        for (const codeqa::model::MacroModel& macro : context.file.macros)
        {
            if (!macro.isFunctionLike)
            {
                continue;
            }

            for (const std::string& parameter : macro.parameters)
            {
                if (!hasUnwrappedOccurrence(macro.body, parameter))
                {
                    continue;
                }

                Finding finding;
                finding.ruleId = id();
                finding.severity = codeqa::core::DiagnosticSeverity::Warning;
                finding.description = "Parameter '" + parameter + "' of macro '" + macro.symbol.name
                    + "' is used without enclosing parentheses in the macro body.";
                finding.recommendation = "Wrap the parameter as (" + parameter
                    + ") everywhere it appears in the macro body to avoid operator-precedence "
                      "bugs at the call site.";
                finding.location = macro.symbol.range.begin;
                finding.symbolName = macro.symbol.qualifiedName;
                findings.push_back(std::move(finding));
            }
        }

        return findings;
    }
};
```

Immediately after `allRules(...)`'s closing brace (ends at line 540 with `}`) and before `} // namespace codeqa::rules` (line 542), insert:

```cpp

std::vector<std::unique_ptr<FileRule>> allFileRules(const codeqa::config::ProjectConfig& config)
{
    std::vector<std::unique_ptr<FileRule>> rules;

    if (const codeqa::config::RuleConfig& c =
            config.rules.at("preprocessor.unparenthesized-macro-parameter");
        c.enabled)
    {
        rules.push_back(std::make_unique<UnparenthesizedMacroParameterRule>());
    }

    return rules;
}
```

- [ ] **Step 6: Wire the default config and `knownRuleIds()`**

In `src/config/DefaultConfig.cpp`, add this line right before `return config;` in `defaultConfig()`, immediately after the existing `memory.array-parameter-decay` line:

```cpp
    config.rules["preprocessor.unparenthesized-macro-parameter"] = thresholds(0, 0);
```

In `src/config/ConfigJson.cpp`, `knownRuleIds()` currently ends (lines 35-36):

```cpp
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay"};
    return ids;
```

Change to:

```cpp
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay",
        "preprocessor.unparenthesized-macro-parameter"};
    return ids;
```

- [ ] **Step 7: Run the new tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="preprocessor.unparenthesized*"
```
Expected: PASS (9 test cases, 0 failures).

- [ ] **Step 8: Update `DefaultConfigTest.cpp`**

Replace the full contents of `tests/unit/DefaultConfigTest.cpp`'s `TEST_CASE` (currently lines 22-40) with:

```cpp
TEST_CASE("defaultConfig ships the eight Fase 7 thresholds plus the unsafe-string-api, "
          "uninitialized-pointer, array-parameter-decay, and unparenthesized-macro-parameter rules, "
          "all enabled, no header overrides")
{
    const codeqa::config::ProjectConfig config = codeqa::config::defaultConfig();

    CHECK(config.headerOverrides.empty());
    REQUIRE(config.rules.size() == 12);

    checkRule(config, "size.function-length", 50, 90);
    checkRule(config, "size.local-variables", 8, 12);
    checkRule(config, "signature.parameter-count", 4, 7);
    checkRule(config, "signature.consecutive-booleans", 2, 3);
    checkRule(config, "complexity.cyclomatic", 10, 15);
    checkRule(config, "complexity.returns", 4, 6);
    checkRule(config, "complexity.condition", 3, 5);
    checkRule(config, "complexity.nesting", 3, 5);
    checkRule(config, "security.unsafe-string-api", 0, 0);
    checkRule(config, "memory.uninitialized-pointer", 0, 0);
    checkRule(config, "memory.array-parameter-decay", 0, 0);
    checkRule(config, "preprocessor.unparenthesized-macro-parameter", 0, 0);
}
```

- [ ] **Step 9: Update the `ReportJsonTest.cpp` rule-count assertion**

In `tests/unit/ReportJsonTest.cpp:52`, this line:

```cpp
    CHECK(document.at("config").at("rules").size() == 11);
```

becomes:

```cpp
    CHECK(document.at("config").at("rules").size() == 12);
```

- [ ] **Step 10: Add the corpus stability row**

In `tests/corpus/validation/RULE-STABILITY.md`, add a new row at the end of the table (after the `memory.array-parameter-decay` row):

```markdown
| `preprocessor.unparenthesized-macro-parameter` | `experimental` | — | — |
```

- [ ] **Step 11: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures — confirms `RulesTest.cpp`'s existing rule-count assertions were unaffected (this is a `FileRule`, not a `Rule`, so `allRules(...).size()` stays 11) and `CorpusReportTest.cpp` needs no changes (verify with `grep -rn "#define" tests/corpus` — excluding `tests/corpus/validation/`, this returns no matches, so no corpus fixture contains a macro and none of their expected finding counts change).

- [ ] **Step 12: Commit**

```bash
git add include/codeqa/rules/FileRule.h include/codeqa/rules/Rules.h src/rules/Rules.cpp src/config/DefaultConfig.cpp src/config/ConfigJson.cpp tests/CMakeLists.txt tests/unit/FileRulesTest.cpp tests/unit/DefaultConfigTest.cpp tests/unit/ReportJsonTest.cpp tests/corpus/validation/RULE-STABILITY.md
git commit -m "feat: add FileRule mechanism and preprocessor.unparenthesized-macro-parameter rule"
```

---

### Task 4: Wire the file-level loop into `ApplyRules.cpp`

**Files:**
- Modify: `src/application/ApplyRules.cpp:1-9` (new include), `:117-190` (new loop)
- Modify: `tests/unit/ApplyRulesTest.cpp` (2 new end-to-end test cases)

**Interfaces:**
- Consumes: `codeqa::rules::allFileRules(config)`, `codeqa::rules::FileRule`, `codeqa::rules::FileRuleContext` (Task 3); `FileModel::macros` (Task 2); the existing `isSuppressed(finding, file.comments, sourceLines)` helper already defined in this file.
- Produces: findings with `ruleId == "preprocessor.unparenthesized-macro-parameter"` appearing in `FileFindingsModel::findings`/`suppressedFindings` exactly like every `Rule`-sourced finding — consumed by Task 5's desktop GUI (via the JSON report) and by nothing else in this plan.

- [ ] **Step 1: Write the failing end-to-end tests**

Open `tests/unit/ApplyRulesTest.cpp`. Append at the true end of the file (after the last `TEST_CASE`, which currently ends at line 262):

```cpp

TEST_CASE("ApplyRules generates a finding for an unsafe macro parameter")
{
    const TempProject fixture;
    fixture.addFile(
        "macros.c",
        "#define SQUARE(x) x*x\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK(hasFinding(findings.files.front().findings, "preprocessor.unparenthesized-macro-parameter"));
}

TEST_CASE("ApplyRules suppresses an unsafe macro finding with a matching inline disable comment")
{
    const TempProject fixture;
    fixture.addFile(
        "macros.c",
        "// codeqa-disable-next-line preprocessor.unparenthesized-macro-parameter\n"
        "#define SQUARE(x) x*x\n");

    const codeqa::rules::ProjectFindingsModel findings = analyzeAndApplyRules(fixture.root());

    REQUIRE(findings.files.size() == 1);
    CHECK_FALSE(
        hasFinding(findings.files.front().findings, "preprocessor.unparenthesized-macro-parameter"));
    CHECK(
        hasFinding(
            findings.files.front().suppressedFindings, "preprocessor.unparenthesized-macro-parameter"));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*unsafe macro*"
```
Expected: FAIL — no finding is produced yet, since `ApplyRules.cpp` never calls `allFileRules`.

- [ ] **Step 3: Add the file-level loop**

In `src/application/ApplyRules.cpp`, the includes currently read (lines 1-9):

```cpp
#include "codeqa/application/ApplyRules.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/core/SourceRangeText.h"
#include "codeqa/infrastructure/ReadFileContents.h"
#include "codeqa/infrastructure/StringUtilities.h"
#include "codeqa/rules/Rule.h"
#include "codeqa/rules/Rules.h"
```

Add `codeqa/rules/FileRule.h`:

```cpp
#include "codeqa/application/ApplyRules.h"

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/SourceRange.h"
#include "codeqa/core/SourceRangeText.h"
#include "codeqa/infrastructure/ReadFileContents.h"
#include "codeqa/infrastructure/StringUtilities.h"
#include "codeqa/rules/FileRule.h"
#include "codeqa/rules/Rule.h"
#include "codeqa/rules/Rules.h"
```

`ApplyRules::execute()` currently starts (lines 117-125):

```cpp
codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::config::ProjectConfig& config) const
{
    codeqa::rules::ProjectFindingsModel result;
    result.rootPath = project.rootPath;
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);

    for (const codeqa::model::FileModel& file : project.files)
```

Change to:

```cpp
codeqa::rules::ProjectFindingsModel ApplyRules::execute(
    const codeqa::model::ProjectModel& project,
    const codeqa::metrics::ProjectMetricsModel& metrics,
    const codeqa::config::ProjectConfig& config) const
{
    codeqa::rules::ProjectFindingsModel result;
    result.rootPath = project.rootPath;
    const std::vector<std::unique_ptr<codeqa::rules::Rule>> rules = codeqa::rules::allRules(config);
    const std::vector<std::unique_ptr<codeqa::rules::FileRule>> fileRules =
        codeqa::rules::allFileRules(config);

    for (const codeqa::model::FileModel& file : project.files)
```

The per-callable loop's closing brace and the subsequent `result.files.push_back(...)` currently read (lines 184-187):

```cpp
        }

        result.files.push_back(std::move(fileFindings));
    }
```

Change to:

```cpp
        }

        const codeqa::rules::FileRuleContext fileContext{file};

        for (const std::unique_ptr<codeqa::rules::FileRule>& fileRule : fileRules)
        {
            if (!fileRule->appliesTo(file.language))
            {
                continue;
            }

            for (codeqa::rules::Finding& finding : fileRule->evaluate(fileContext))
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
        }

        result.files.push_back(std::move(fileFindings));
    }
```

- [ ] **Step 4: Run the tests to verify they pass**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe --test-case="*unsafe macro*"
```
Expected: PASS (2 test cases, 0 failures).

- [ ] **Step 5: Run the full test suite**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures — confirms every existing `ApplyRulesTest.cpp` case (including the PARSE004-overlap and disabled-rule-via-config cases) is unaffected by the new loop.

- [ ] **Step 6: Commit**

```bash
git add src/application/ApplyRules.cpp tests/unit/ApplyRulesTest.cpp
git commit -m "feat: evaluate FileRules in ApplyRules, wiring macro findings into the report pipeline"
```

---

### Task 5: Wire the desktop GUI and run final verification

**Files:**
- Modify: `src/desktop/ReportModels.cpp:17-32` (`orderedRuleIds()` — append the id)
- Modify: `src/desktop/qml/pages/AnalyzePage.qml:278-316` (extend both `visible` conditions)
- Modify: `tests/desktop/AppControllerTest.cpp:219-231,264` (rowCount + expected id list, both call sites)

**Interfaces:**
- Consumes: rule id string `"preprocessor.unparenthesized-macro-parameter"` (Task 3) — must match exactly.
- Produces: nothing consumed by a later task — this is the final task in the plan.

- [ ] **Step 1: Write the failing desktop test expectations**

In `tests/desktop/AppControllerTest.cpp`, around line 219, this block:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 11);
    const QStringList expectedRuleIds = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting",
        "security.unsafe-string-api",
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay"};
```

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 12);
    const QStringList expectedRuleIds = {
        "size.function-length",
        "size.local-variables",
        "signature.parameter-count",
        "signature.consecutive-booleans",
        "complexity.cyclomatic",
        "complexity.returns",
        "complexity.condition",
        "complexity.nesting",
        "security.unsafe-string-api",
        "memory.uninitialized-pointer",
        "memory.array-parameter-decay",
        "preprocessor.unparenthesized-macro-parameter"};
```

Around line 264 (in `missingConfigurationUsesUnsavedDefaultsWithoutWriting`):

```cpp
    QCOMPARE(controller.rules()->rowCount(), 11);
```

becomes:

```cpp
    QCOMPARE(controller.rules()->rowCount(), 12);
```

- [ ] **Step 2: Run the desktop tests to verify they fail**

Run:
```
cmake --build buildQt --target codeqa_desktop_tests
buildQt\codeqa_desktop_tests.exe
```
If this executable produces no output (missing Qt platform plugin in a fresh worktree), instead run:
```
set QT_QPA_PLATFORM=offscreen
set PATH=C:\Qt\6.11.0\msvc2022_64\bin;%PATH%
buildQt\codeqa_desktop_tests.exe
```
Expected: FAIL on both updated `rowCount()` assertions (still 11 in the model, since `orderedRuleIds()` hasn't been updated yet).

- [ ] **Step 3: Update `orderedRuleIds()`**

`src/desktop/ReportModels.cpp`'s `orderedRuleIds()` currently ends (lines 28-30):

```cpp
        QStringLiteral("security.unsafe-string-api"),
        QStringLiteral("memory.uninitialized-pointer"),
        QStringLiteral("memory.array-parameter-decay")};
    return ids;
```

Change to:

```cpp
        QStringLiteral("security.unsafe-string-api"),
        QStringLiteral("memory.uninitialized-pointer"),
        QStringLiteral("memory.array-parameter-decay"),
        QStringLiteral("preprocessor.unparenthesized-macro-parameter")};
    return ids;
```

- [ ] **Step 4: Hide the unused limit fields for this rule too**

In `src/desktop/qml/pages/AnalyzePage.qml`, both `TextField`s currently carry a three-line `visible` binding (lines 278-282 and 298-302):

```qml
                                    TextField {
                                        id: warningLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
```

and

```qml
                                    TextField {
                                        id: highLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
```

Extend both with a 4th clause:

```qml
                                    TextField {
                                        id: warningLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
                                            && ruleCard.model.id !== "preprocessor.unparenthesized-macro-parameter"
```

and

```qml
                                    TextField {
                                        id: highLimitField
                                        visible: ruleCard.model.id !== "security.unsafe-string-api"
                                            && ruleCard.model.id !== "memory.uninitialized-pointer"
                                            && ruleCard.model.id !== "memory.array-parameter-decay"
                                            && ruleCard.model.id !== "preprocessor.unparenthesized-macro-parameter"
```

- [ ] **Step 5: Run the desktop tests and QML smoke test to verify they pass**

Run:
```
cmake --build buildQt --target codeqa_desktop
cmake --build buildQt --target codeqa_desktop_tests
cmake --build buildQt --target codeqa_desktop_qml_smoke
buildQt\codeqa_desktop_tests.exe
buildQt\codeqa_desktop_qml_smoke.exe
```
(Use the same `QT_QPA_PLATFORM=offscreen` + `PATH` workaround from Step 2 if the executables produce no output.)
Expected: all three builds succeed, `codeqa_desktop_tests.exe` reports 0 failures, `codeqa_desktop_qml_smoke.exe` passes (confirms `AnalyzePage.qml` still parses and loads with no binding errors).

- [ ] **Step 6: Manual visual check (recommended, not build-blocking)**

Run `buildQt\codeqa_desktop.exe`, open any project, go to the Analyze page, and confirm: the `size.function-length` row still shows both limit fields; the `preprocessor.unparenthesized-macro-parameter` row (enabled by default) shows no limit fields, just its id and the enabled checkbox — same as the three prior pattern-based rules.

- [ ] **Step 7: Run the complete CLI test suite one final time**

Run:
```
cmake --build build --config Debug --target codeqa_tests
build\tests\Debug\codeqa_tests.exe
```
Expected: PASS, all test cases, 0 failures. This is the final regression check across all 5 tasks (frontend extraction, model/report bridging, the `FileRule` mechanism and rule, `ApplyRules.cpp` wiring, and this task's desktop changes) — confirms nothing in the whole slice regressed any of the 11 pre-existing rules, `Rule`/`RuleContext`, or any other test file.

- [ ] **Step 8: Commit**

```bash
git add src/desktop/ReportModels.cpp src/desktop/qml/pages/AnalyzePage.qml tests/desktop/AppControllerTest.cpp
git commit -m "feat: wire preprocessor.unparenthesized-macro-parameter into the desktop rule list"
```

---

## Self-Review Notes

- **Spec coverage:** §2 in-scope frontend/model/report items → Task 1 + Task 2. §2's "new mechanism" (`FileRule`) and rule/heuristic → Task 3. §2's config/registration list → Task 3 Steps 6, 8-10 (rule-id-list surfaces) plus Task 1 Step 6-7 and Task 2 (new-model surfaces) plus Task 3 Steps 3-5 (new-mechanism surfaces) — all 21 numbered touch points from spec §8 have a corresponding step: #1-2 → Task 3 Step 6; #3 → Task 5 Step 3; #4 → Task 3 Step 4; #5 → Task 3 Step 9; #6-7 → Task 5 Step 4; #8 → Task 3 Step 8; #9-10 → Task 3 Step 4; #11 → Task 3 Step 5; #12 → Task 4 Step 3; #13-14 → Task 1 Steps 3-4; #15 → Task 2 Step 3; #16 → Task 2 Step 4; #17 → Task 2 Step 5; #18 → Task 1 Step 5; #19 → Task 1 Step 6; #20 → Task 1 Step 7; #21 → Task 1 Steps 8-11. §9 testing strategy → every `StructuralExtractorTest.cpp`/`ProjectModelBuilderTest.cpp`/`FileJsonTest.cpp`/`FileRulesTest.cpp`/`ApplyRulesTest.cpp`/`DefaultConfigTest.cpp`/`ReportJsonTest.cpp`/`RULE-STABILITY.md`/`AppControllerTest.cpp` item listed in the spec has a matching step, including the corrected `AT(i)`/`AT(arr,i)` asymmetric pair (Task 3 Step 1, tests 4 and 5) and the `CorpusReportTest.cpp` grep check (Task 3 Step 11). §10 acceptance criteria → covered by the combination of Task 3's `FileRulesTest.cpp` (finding shape, no-finding cases, object-like exemption), Task 4's suppression test, and Task 5's GUI/rowCount checks. No gaps found.
- **Placeholder scan:** none found — every step carries the literal code, exact file content, or exact command to run.
- **Type/name consistency:** `frontend::MacroModel{name, isFunctionLike, parameters, isVariadic, body, range}` (Task 1) and `model::MacroModel{symbol, isFunctionLike, parameters, isVariadic, body}` (Task 2) are spelled identically at every construction site across both tasks' code and tests. `FileRule`/`FileRuleContext`/`allFileRules` (Task 3) are declared in `Rules.h` and defined in `Rules.cpp` with matching signatures, and used with those exact names in Task 4's `ApplyRules.cpp` and Task 3's `FileRulesTest.cpp`. Rule id string `"preprocessor.unparenthesized-macro-parameter"` is identical across Task 3 (`id()`, `allFileRules`, `defaultConfig`, `knownRuleIds`, `FileRulesTest.cpp`, `DefaultConfigTest.cpp`, `ReportJsonTest.cpp`, `RULE-STABILITY.md`), Task 4 (`ApplyRulesTest.cpp`'s suppression comment), and Task 5 (`orderedRuleIds`, both QML `visible` bindings, `AppControllerTest.cpp`). All "11 → 12" / "12" references are consistent across every touched file. Confirmed against the actual current file contents (not assumed) by reading every touched file in full, including exact current line numbers, before writing this plan.
