#include "doctest/doctest.h"

#include "codeqa/architecture/AnalyzeArchitecture.h"
#include "codeqa/config/DefaultConfig.h"

namespace
{

codeqa::model::FileModel makeFile(std::string path)
{
    codeqa::model::FileModel file;
    file.path = std::move(path);
    return file;
}

codeqa::model::DependencyModel makeDependency(std::string target)
{
    codeqa::model::DependencyModel dependency;
    dependency.target = std::move(target);
    dependency.kind = codeqa::model::IncludeKind::Local;
    return dependency;
}

} // namespace

TEST_CASE("analyzeArchitecture reports a cycle end-to-end from raw includes")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("a.h"));
    project.files.push_back(makeFile("b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h"));
    project.files[1].dependencies.push_back(makeDependency("a.h"));

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.cycles.size() == 1);
    CHECK(result.cycles[0].files.size() == 2);
    CHECK(result.files.size() == 2);
}

TEST_CASE("analyzeArchitecture flags a file whose fan-out is at or above the warning threshold")
{
    codeqa::model::ProjectModel project;
    for (int i = 0; i < 10; ++i)
    {
        project.files.push_back(makeFile("dep" + std::to_string(i) + ".h"));
    }
    project.files.push_back(makeFile("hub.h"));
    for (int i = 0; i < 10; ++i)
    {
        project.files.back().dependencies.push_back(makeDependency("dep" + std::to_string(i) + ".h"));
    }

    codeqa::config::ArchitectureConfig config;
    config.fanOutWarning = 10;
    config.fanOutHigh = 20;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.fanOutFindings.size() == 1);
    CHECK(result.fanOutFindings[0].path == "hub.h");
    CHECK(result.fanOutFindings[0].fanOut == 10);
    CHECK(result.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Warning);
}

TEST_CASE("analyzeArchitecture uses Error severity at or above the high threshold")
{
    codeqa::model::ProjectModel project;
    for (int i = 0; i < 20; ++i)
    {
        project.files.push_back(makeFile("dep" + std::to_string(i) + ".h"));
    }
    project.files.push_back(makeFile("hub.h"));
    for (int i = 0; i < 20; ++i)
    {
        project.files.back().dependencies.push_back(makeDependency("dep" + std::to_string(i) + ".h"));
    }

    codeqa::config::ArchitectureConfig config;
    config.fanOutWarning = 10;
    config.fanOutHigh = 20;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.fanOutFindings.size() == 1);
    CHECK(result.fanOutFindings[0].severity == codeqa::core::DiagnosticSeverity::Error);
}

TEST_CASE("analyzeArchitecture never flags fan-in, regardless of value")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("core.h"));
    for (int i = 0; i < 30; ++i)
    {
        project.files.push_back(makeFile("user" + std::to_string(i) + ".h"));
        project.files.back().dependencies.push_back(makeDependency("core.h"));
    }

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.fanOutFindings.empty());

    bool foundCore = false;
    for (const codeqa::architecture::FileDependencyMetrics& file : result.files)
    {
        if (file.path == "core.h")
        {
            foundCore = true;
            CHECK(file.fanIn == 30);
        }
    }
    CHECK(foundCore);
}

TEST_CASE("analyzeArchitecture returns an empty model when disabled")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("a.h"));
    project.files.push_back(makeFile("b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h"));
    project.files[1].dependencies.push_back(makeDependency("a.h"));

    codeqa::config::ArchitectureConfig config;
    config.enabled = false;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.files.empty());
    CHECK(result.cycles.empty());
    CHECK(result.fanOutFindings.empty());
    CHECK(result.forbiddenDependencyViolations.empty());
}

TEST_CASE("analyzeArchitecture reports a forbidden-dependency violation end-to-end")
{
    codeqa::model::ProjectModel project;
    project.rootPath = "project";
    project.files.push_back(makeFile((project.rootPath / "src" / "core" / "a.h").generic_string()));
    project.files.push_back(makeFile((project.rootPath / "src" / "desktop" / "b.h").generic_string()));
    project.files[0].dependencies.push_back(makeDependency("b.h"));

    codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;
    config.layers = {{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}};
    config.forbiddenDependencies = {{"core", "desktop"}};

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    REQUIRE(result.forbiddenDependencyViolations.size() == 1);
    CHECK(result.forbiddenDependencyViolations[0].fromLayer == "core");
    CHECK(result.forbiddenDependencyViolations[0].toLayer == "desktop");
}

TEST_CASE("analyzeArchitecture reports no violation when the project has no layer configuration")
{
    codeqa::model::ProjectModel project;
    project.rootPath = "project";
    project.files.push_back(makeFile((project.rootPath / "src" / "core" / "a.h").generic_string()));
    project.files.push_back(makeFile((project.rootPath / "src" / "desktop" / "b.h").generic_string()));
    project.files[0].dependencies.push_back(makeDependency("b.h"));

    const codeqa::config::ArchitectureConfig config = codeqa::config::defaultConfig().architecture;

    const codeqa::architecture::ProjectArchitectureModel result =
        codeqa::architecture::analyzeArchitecture(project, config);

    CHECK(result.forbiddenDependencyViolations.empty());
}
