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

void checkCollectionOrder(
    const codeqa::model::FileModel& file,
    const codeqa::model::FileModel& repeatedFile)
{
    checkRangeOrder(file.dependencies, [](const codeqa::model::DependencyModel& dependency) -> const auto& {
        return dependency.range;
    });
    checkRangeOrder(file.callables, [](const codeqa::model::CallableModel& callable) -> const auto& {
        return callable.symbol.range;
    });
    checkRangeOrder(file.types, [](const codeqa::model::TypeModel& type) -> const auto& {
        return type.symbol.range;
    });
    checkRangeOrder(file.namespaces, [](const codeqa::model::NamespaceModel& nameSpace) -> const auto& {
        return nameSpace.symbol.range;
    });
    checkRangeOrder(file.globalVariables, [](const codeqa::model::GlobalVariableModel& variable) -> const auto& {
        return variable.symbol.range;
    });
    checkRangeOrder(file.typeAliases, [](const codeqa::model::TypeAliasModel& alias) -> const auto& {
        return alias.symbol.range;
    });

    checkSameOrder(file.dependencies, repeatedFile.dependencies, [](const auto& dependency) {
        return dependency.target;
    });
    checkSameOrder(file.callables, repeatedFile.callables, [](const auto& callable) {
        return callable.symbol.name;
    });
    checkSameOrder(file.types, repeatedFile.types, [](const auto& type) {
        return type.symbol.name;
    });
    checkSameOrder(file.namespaces, repeatedFile.namespaces, [](const auto& nameSpace) {
        return nameSpace.symbol.name;
    });
    checkSameOrder(file.globalVariables, repeatedFile.globalVariables, [](const auto& variable) {
        return variable.symbol.name;
    });
    checkSameOrder(file.typeAliases, repeatedFile.typeAliases, [](const auto& alias) {
        return alias.symbol.name;
    });
}

} // namespace

TEST_CASE("corpus adapts to the common model")
{
    const std::filesystem::path corpusRoot{CODEQA_CORPUS_DIR};
    codeqa::application::AnalyzeProjectRequest request;
    request.discovery.rootPath = corpusRoot;

    const codeqa::application::AnalyzeProject useCase;
    const codeqa::model::ProjectModel project = useCase.execute(request);
    const codeqa::model::ProjectModel repeatedProject = useCase.execute(request);

    CHECK(project.rootPath == corpusRoot);
    REQUIRE(project.files.size() == 13);
    REQUIRE(repeatedProject.files.size() == project.files.size());

    for (std::size_t index = 1; index < project.files.size(); ++index)
    {
        CHECK(project.files[index - 1].path.generic_string() <= project.files[index].path.generic_string());
    }

    for (std::size_t index = 0; index < project.files.size(); ++index)
    {
        const codeqa::model::FileModel& file = project.files[index];
        const codeqa::model::FileModel& repeatedFile = repeatedProject.files[index];
        CHECK(file.path == repeatedFile.path);
        checkCollectionOrder(file, repeatedFile);
    }

    const codeqa::model::FileModel& truncated =
        fileNamed(project, corpusRoot / "malformed" / "truncated.c");
    const codeqa::model::FileModel& unclosedBrace =
        fileNamed(project, corpusRoot / "malformed" / "unclosed-brace.cpp");

    CHECK(truncated.state == codeqa::model::AnalysisState::Partial);
    REQUIRE(truncated.diagnostics.size() == 1);
    CHECK(truncated.diagnostics[0].code == "PARSE001");
    CHECK(truncated.diagnostics[0].severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(truncated.diagnostics[0].location.filePath == corpusRoot / "malformed" / "truncated.c");
    CHECK(truncated.diagnostics[0].location.line == 8);

    CHECK(unclosedBrace.state == codeqa::model::AnalysisState::Partial);
    REQUIRE(unclosedBrace.diagnostics.size() == 1);
    CHECK(unclosedBrace.diagnostics[0].code == "PARSE002");
    CHECK(unclosedBrace.diagnostics[0].severity == codeqa::core::DiagnosticSeverity::Error);
    CHECK(unclosedBrace.diagnostics[0].location.filePath == corpusRoot / "malformed" / "unclosed-brace.cpp");
    CHECK(unclosedBrace.diagnostics[0].location.line == 13);

    for (const codeqa::model::FileModel& file : project.files)
    {
        if (file.path != truncated.path && file.path != unclosedBrace.path)
        {
            CHECK(file.state == codeqa::model::AnalysisState::Complete);
        }
    }

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

}
