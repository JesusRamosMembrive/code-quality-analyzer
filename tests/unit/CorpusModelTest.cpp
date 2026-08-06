#include "doctest/doctest.h"

#include "codeqa/application/AnalyzeProject.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

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

    throw std::runtime_error("Expected corpus file was not modeled.");
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

} // namespace

TEST_CASE("corpus adapts to the common model")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;

    const codeqa::application::AnalyzeProject useCase;
    const codeqa::model::ProjectModel project = useCase.execute(request);

    CHECK(project.rootPath == corpusRoot);
    REQUIRE(project.files.size() == 13);

    std::size_t partialCount = 0;

    for (std::size_t index = 1; index < project.files.size(); ++index)
    {
        CHECK(project.files[index - 1].path.generic_string() <= project.files[index].path.generic_string());
    }

    for (const codeqa::model::FileModel& file : project.files)
    {
        if (file.state == codeqa::model::AnalysisState::Partial)
        {
            ++partialCount;
            CHECK_FALSE(file.diagnostics.empty());
        }
    }

    CHECK(partialCount == 2);

    const codeqa::model::FileModel& cTypes =
        fileNamed(project, corpusRoot / "classes" / "c-types.c");
    CHECK(cTypes.state == codeqa::model::AnalysisState::Complete);
    CHECK(hasNamedModel(cTypes.globalVariables, "globalCounter"));
    CHECK(hasNamedModel(cTypes.typeAliases, "PointAlias"));

    const codeqa::model::FileModel& cppTypes =
        fileNamed(project, corpusRoot / "classes" / "cpp-types.cpp");
    CHECK(cppTypes.state == codeqa::model::AnalysisState::Complete);
    CHECK(hasNamedModel(cppTypes.callables, "size"));
    CHECK(hasNamedModel(cppTypes.types, "Container"));
    REQUIRE(cppTypes.dependencies.size() == 1);
    CHECK(cppTypes.dependencies[0].target == "cstddef");

    checkRangeOrder(cppTypes.dependencies, [](const codeqa::model::DependencyModel& dependency) -> const auto& {
        return dependency.range;
    });
    checkRangeOrder(cppTypes.callables, [](const codeqa::model::CallableModel& callable) -> const auto& {
        return callable.symbol.range;
    });
    checkRangeOrder(cppTypes.types, [](const codeqa::model::TypeModel& type) -> const auto& {
        return type.symbol.range;
    });
}
