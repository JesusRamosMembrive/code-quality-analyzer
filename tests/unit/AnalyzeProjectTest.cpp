#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

class TempProject
{
public:
    TempProject()
        : root_(std::filesystem::temp_directory_path() / "codeqa_analyze_project_test")
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~TempProject()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TempProject(const TempProject&) = delete;
    TempProject& operator=(const TempProject&) = delete;

    void addFile(const std::filesystem::path& relativePath, const std::string& content) const
    {
        const std::filesystem::path path = root_ / relativePath;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

const codeqa::model::FileModel& fileNamed(
    const codeqa::model::ProjectModel& project,
    const std::filesystem::path& path)
{
    for (const codeqa::model::FileModel& file : project.files)
    {
        if (file.path == path)
        {
            return file;
        }
    }

    throw std::runtime_error("Expected file was not modeled.");
}

template <typename Model>
bool hasNamedModel(const std::vector<Model>& models, const std::string& name)
{
    for (const Model& model : models)
    {
        if (model.symbol.name == name)
        {
            return true;
        }
    }

    return false;
}

template <typename Model, typename RangeFor>
void checkRangeOrder(const std::vector<Model>& models, RangeFor rangeFor)
{
    for (std::size_t index = 1; index < models.size(); ++index)
    {
        const codeqa::core::SourceRange& previous = rangeFor(models[index - 1]);
        const codeqa::core::SourceRange& current = rangeFor(models[index]);
        const bool ordered = previous.begin.offset < current.begin.offset
            || (previous.begin.offset == current.begin.offset && previous.end.offset <= current.end.offset);
        CHECK(ordered);
    }
}

template <typename Model, typename SignatureFor>
void checkSameOrder(
    const std::vector<Model>& first,
    const std::vector<Model>& second,
    SignatureFor signatureFor)
{
    REQUIRE(first.size() == second.size());

    for (std::size_t index = 0; index < first.size(); ++index)
    {
        CHECK(signatureFor(first[index]) == signatureFor(second[index]));
    }
}

} // namespace

TEST_CASE("analyzes a project into the common model")
{
    const TempProject fixture;
    fixture.addFile(
        "globals.c",
        "static int cache = 1;\n"
        "extern const char* title;\n"
        "typedef unsigned long Size;\n"
        "typedef unsigned int Count;\n"
        "int fileFunction(void)\n"
        "{\n"
        "    int localValue = 0;\n"
        "    return localValue;\n"
        "}\n");
    fixture.addFile(
        "widget.cpp",
        "#include <algorithm>\n"
        "#include <vector>\n"
        "namespace outer\n"
        "{\n"
        "namespace inner\n"
        "{\n"
        "class Widget\n"
        "{\n"
        "public:\n"
        "    int value() const { return 7; }\n"
        "};\n"
        "struct Helper\n"
        "{\n"
        "    int doubleValue() const { return 14; }\n"
        "};\n"
        "}\n"
        "}\n");
    fixture.addFile(
        "broken.cpp",
        "int recoveredGlobal = 3;\n"
        "class Broken\n"
        "{\n"
        "public:\n"
        "    int value() const { return recoveredGlobal; }\n"
        "};\n"
        "int broken( {\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();

    const codeqa::application::AnalyzeProject useCase;
    const codeqa::model::ProjectModel project = useCase.execute(request);
    const codeqa::model::ProjectModel repeatedProject = useCase.execute(request);

    CHECK(project.rootPath == fixture.root());
    REQUIRE(project.files.size() == 3);

    const codeqa::model::FileModel& globals = fileNamed(project, fixture.root() / "globals.c");
    const codeqa::model::FileModel& widget = fileNamed(project, fixture.root() / "widget.cpp");
    const codeqa::model::FileModel& broken = fileNamed(project, fixture.root() / "broken.cpp");

    CHECK(globals.state == codeqa::model::AnalysisState::Complete);
    CHECK(widget.state == codeqa::model::AnalysisState::Complete);
    CHECK(broken.state == codeqa::model::AnalysisState::Partial);
    CHECK_FALSE(broken.diagnostics.empty());

    CHECK(hasNamedModel(globals.globalVariables, "cache"));
    CHECK(hasNamedModel(globals.globalVariables, "title"));
    CHECK(hasNamedModel(globals.typeAliases, "Size"));
    CHECK(hasNamedModel(globals.typeAliases, "Count"));
    REQUIRE(widget.dependencies.size() == 2);
    REQUIRE(widget.callables.size() == 2);
    REQUIRE(widget.types.size() == 2);
    REQUIRE(widget.namespaces.size() == 2);
    CHECK(hasNamedModel(widget.types, "Widget"));
    CHECK(hasNamedModel(widget.types, "Helper"));
    CHECK(hasNamedModel(broken.globalVariables, "recoveredGlobal"));
    CHECK(hasNamedModel(broken.types, "Broken"));

    for (const codeqa::model::FileModel& file : project.files)
    {
        CHECK_FALSE(hasNamedModel(file.globalVariables, "localValue"));
    }

    checkRangeOrder(globals.globalVariables, [](const codeqa::model::GlobalVariableModel& variable) -> const auto& {
        return variable.symbol.range;
    });
    checkRangeOrder(globals.typeAliases, [](const codeqa::model::TypeAliasModel& alias) -> const auto& {
        return alias.symbol.range;
    });
    checkRangeOrder(widget.dependencies, [](const codeqa::model::DependencyModel& dependency) -> const auto& {
        return dependency.range;
    });
    checkRangeOrder(widget.callables, [](const codeqa::model::CallableModel& callable) -> const auto& {
        return callable.symbol.range;
    });
    checkRangeOrder(widget.types, [](const codeqa::model::TypeModel& type) -> const auto& {
        return type.symbol.range;
    });
    checkRangeOrder(widget.namespaces, [](const codeqa::model::NamespaceModel& nameSpace) -> const auto& {
        return nameSpace.symbol.range;
    });

    REQUIRE(repeatedProject.files.size() == project.files.size());
    for (std::size_t index = 0; index < project.files.size(); ++index)
    {
        CHECK(project.files[index].path == repeatedProject.files[index].path);
    }

    const codeqa::model::FileModel& repeatedGlobals =
        fileNamed(repeatedProject, fixture.root() / "globals.c");
    const codeqa::model::FileModel& repeatedWidget =
        fileNamed(repeatedProject, fixture.root() / "widget.cpp");
    checkSameOrder(globals.globalVariables, repeatedGlobals.globalVariables, [](const auto& variable) {
        return variable.symbol.name;
    });
    checkSameOrder(globals.typeAliases, repeatedGlobals.typeAliases, [](const auto& alias) {
        return alias.symbol.name;
    });
    checkSameOrder(widget.dependencies, repeatedWidget.dependencies, [](const auto& dependency) {
        return dependency.target;
    });
    checkSameOrder(widget.callables, repeatedWidget.callables, [](const auto& callable) {
        return callable.symbol.name;
    });
    checkSameOrder(widget.types, repeatedWidget.types, [](const auto& type) {
        return type.symbol.name;
    });
    checkSameOrder(widget.namespaces, repeatedWidget.namespaces, [](const auto& nameSpace) {
        return nameSpace.symbol.name;
    });
}

TEST_CASE("AnalyzeProject applies language overrides and discovery exclusions")
{
    const TempProject fixture;
    fixture.addFile("forced.h", "typedef int HeaderType;\n");
    fixture.addFile("kept.c", "int kept;\n");
    fixture.addFile("excluded.cpp", "class Excluded {};\n");

    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = fixture.root();
    request.discovery.excludePatterns = {"excluded.cpp"};
    request.languageOverrides = {
        codeqa::application::LanguageOverride{"forced.h", codeqa::core::Language::C}};

    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    REQUIRE(project.files.size() == 2);
    CHECK(fileNamed(project, fixture.root() / "forced.h").language == codeqa::core::Language::C);
    CHECK(fileNamed(project, fixture.root() / "kept.c").state == codeqa::model::AnalysisState::Complete);

    for (const codeqa::model::FileModel& file : project.files)
    {
        CHECK(file.path != fixture.root() / "excluded.cpp");
    }
}

TEST_CASE("AnalyzeProject materializes discovery diagnostics as unavailable files")
{
    const TempProject fixture;
    const std::filesystem::path missingRoot = fixture.root() / "missing-project";
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = missingRoot;

    const codeqa::model::ProjectModel project = codeqa::application::AnalyzeProject{}.execute(request);

    REQUIRE(project.files.size() == 1);
    const codeqa::model::FileModel& unavailable = project.files.front();
    CHECK(unavailable.path == missingRoot);
    CHECK(unavailable.language == codeqa::core::Language::Unknown);
    CHECK(unavailable.state == codeqa::model::AnalysisState::Unavailable);
    REQUIRE(unavailable.diagnostics.size() == 1);
    CHECK(unavailable.diagnostics[0].code == "DISC001");
    CHECK(unavailable.diagnostics[0].severity == codeqa::core::DiagnosticSeverity::Warning);
    CHECK(unavailable.diagnostics[0].location.filePath == missingRoot);
    CHECK(unavailable.diagnostics[0].location.line == 1);
    CHECK(unavailable.diagnostics[0].location.column == 1);
    CHECK(unavailable.callables.empty());
    CHECK(unavailable.types.empty());
    CHECK(unavailable.namespaces.empty());
    CHECK(unavailable.dependencies.empty());
    CHECK(unavailable.globalVariables.empty());
    CHECK(unavailable.typeAliases.empty());
}
