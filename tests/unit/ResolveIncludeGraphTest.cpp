#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace
{

codeqa::model::FileModel makeFile(std::string path)
{
    codeqa::model::FileModel file;
    file.path = std::move(path);
    return file;
}

codeqa::model::DependencyModel makeDependency(std::string target, codeqa::model::IncludeKind kind)
{
    codeqa::model::DependencyModel dependency;
    dependency.target = std::move(target);
    dependency.kind = kind;
    return dependency;
}

} // namespace

TEST_CASE("resolveIncludeGraph resolves a local include to the matching project file")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.h"));
    project.files.push_back(makeFile("src/b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.filePaths.size() == 2);
    REQUIRE(graph.outgoingEdges.size() == 2);
    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
    CHECK(graph.outgoingEdges[1].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
    CHECK(graph.ambiguousIncludes == 0);
}

TEST_CASE("resolveIncludeGraph never adds an edge for a system include")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("vector"));
    project.files[0].dependencies.push_back(makeDependency("vector", codeqa::model::IncludeKind::System));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph counts an unresolved local include without adding an edge")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files[0].dependencies.push_back(
        makeDependency("nlohmann/json.hpp", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 1);
    CHECK(graph.ambiguousIncludes == 0);
}

TEST_CASE("resolveIncludeGraph counts an ambiguous local include and resolves to the first match")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("src/one/utils.h"));
    project.files.push_back(makeFile("src/two/utils.h"));
    project.files[0].dependencies.push_back(makeDependency("utils.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
    CHECK(graph.ambiguousIncludes == 1);
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph drops a self-include")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.h"));
    project.files[0].dependencies.push_back(makeDependency("a.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    CHECK(graph.outgoingEdges[0].empty());
    CHECK(graph.externalOrUnresolvedIncludes == 0);
}

TEST_CASE("resolveIncludeGraph deduplicates two includes of the same target into one edge")
{
    codeqa::model::ProjectModel project;
    project.files.push_back(makeFile("src/a.cpp"));
    project.files.push_back(makeFile("src/b.h"));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));
    project.files[0].dependencies.push_back(makeDependency("b.h", codeqa::model::IncludeKind::Local));

    const codeqa::architecture::IncludeGraph graph = codeqa::architecture::resolveIncludeGraph(project);

    REQUIRE(graph.outgoingEdges[0].size() == 1);
    CHECK(graph.outgoingEdges[0][0] == 1);
}
