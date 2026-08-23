#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

namespace
{

codeqa::config::ArchitectureConfig makeConfig(
    std::map<std::string, std::vector<std::string>> layers,
    std::vector<codeqa::config::LayerDependency> forbiddenDependencies)
{
    codeqa::config::ArchitectureConfig config;
    config.layers = std::move(layers);
    config.forbiddenDependencies = std::move(forbiddenDependencies);
    return config;
}

} // namespace

TEST_CASE("computeForbiddenDependencies flags an edge crossing a forbidden layer pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {{"core", "desktop"}});

    const std::vector<codeqa::architecture::ForbiddenDependencyViolation> violations =
        codeqa::architecture::computeForbiddenDependencies(graph, "/project", config);

    REQUIRE(violations.size() == 1);
    CHECK(violations[0].fromPath == "/project/src/core/a.h");
    CHECK(violations[0].fromLayer == "core");
    CHECK(violations[0].toPath == "/project/src/desktop/b.h");
    CHECK(violations[0].toLayer == "desktop");
}

TEST_CASE("computeForbiddenDependencies ignores an edge between layers with no forbidden pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies never flags a file that matches no declared layer")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/vendor/b.h"};
    graph.outgoingEdges = {{1}, {}};

    // "vendor" is referenced by forbiddenDependencies but never declared in `layers` -- impossible
    // via real config parsing (rejected at CONFIG014), but this low-level test exercises the
    // algorithm directly and must still behave safely: "src/vendor/b.h" matches no *declared*
    // layer pattern, so layerOf() returns nullopt for it and the edge is skipped.
    const codeqa::config::ArchitectureConfig config = makeConfig({{"core", {"src/core/**"}}}, {{"core", "vendor"}});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies does not flag the reverse direction of a declared pair")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{}, {0}}; // desktop -> core; only core -> desktop is forbidden

    const codeqa::config::ArchitectureConfig config =
        makeConfig({{"core", {"src/core/**"}}, {"desktop", {"src/desktop/**"}}}, {{"core", "desktop"}});

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", config).empty());
}

TEST_CASE("computeForbiddenDependencies breaks a multi-layer match by alphabetically-first layer name")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/shared/a.h", "/project/src/other/b.h"};
    graph.outgoingEdges = {{1}, {}};

    // "src/shared/a.h" matches both "alpha" and "zeta"'s patterns -- "alpha" wins (std::map
    // iterates layers alphabetically by name). Only {alpha -> other} is forbidden, so if "zeta"
    // won instead, this would produce zero violations instead of one.
    const codeqa::config::ArchitectureConfig config = makeConfig(
        {{"alpha", {"src/shared/**"}}, {"zeta", {"src/shared/**"}}, {"other", {"src/other/**"}}},
        {{"alpha", "other"}});

    const std::vector<codeqa::architecture::ForbiddenDependencyViolation> violations =
        codeqa::architecture::computeForbiddenDependencies(graph, "/project", config);

    REQUIRE(violations.size() == 1);
    CHECK(violations[0].fromLayer == "alpha");
}

TEST_CASE("computeForbiddenDependencies returns immediately when layers or forbiddenDependencies is empty")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"/project/src/core/a.h", "/project/src/desktop/b.h"};
    graph.outgoingEdges = {{1}, {}};

    CHECK(codeqa::architecture::computeForbiddenDependencies(graph, "/project", codeqa::config::ArchitectureConfig{})
              .empty());
}
