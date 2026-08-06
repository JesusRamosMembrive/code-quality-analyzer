#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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

} // namespace

TEST_CASE("analyzes a project into the common model")
{
    const TempProject fixture;
    fixture.addFile(
        "globals.c",
        "static int cache = 1;\n"
        "extern const char* title;\n"
        "typedef unsigned long Size;\n"
        "int fileFunction(void)\n"
        "{\n"
        "    int localValue = 0;\n"
        "    return localValue;\n"
        "}\n");
    fixture.addFile(
        "widget.cpp",
        "class Widget\n"
        "{\n"
        "public:\n"
        "    int value() const { return 7; }\n"
        "};\n");
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
    CHECK(hasNamedModel(widget.types, "Widget"));
    CHECK(hasNamedModel(broken.globalVariables, "recoveredGlobal"));
    CHECK(hasNamedModel(broken.types, "Broken"));

    for (const codeqa::model::FileModel& file : project.files)
    {
        CHECK_FALSE(hasNamedModel(file.globalVariables, "localValue"));
    }
}
