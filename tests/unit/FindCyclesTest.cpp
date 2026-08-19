#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

TEST_CASE("findCycles returns nothing for an acyclic graph")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h", "d.h"};
    graph.outgoingEdges = {{1, 2}, {3}, {3}, {}};

    CHECK(codeqa::architecture::findCycles(graph).empty());
}

TEST_CASE("findCycles reports a 2-file mutual cycle")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h"};
    graph.outgoingEdges = {{1}, {0}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 1);
    CHECK(cycles[0].files.size() == 2);
}

TEST_CASE("findCycles reports a 3-file cycle")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h"};
    graph.outgoingEdges = {{1}, {2}, {0}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 1);
    CHECK(cycles[0].files.size() == 3);
}

TEST_CASE("findCycles reports two independent cycles as two separate entries")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h", "d.h"};
    graph.outgoingEdges = {{1}, {0}, {3}, {2}};

    const std::vector<codeqa::architecture::DependencyCycle> cycles = codeqa::architecture::findCycles(graph);

    REQUIRE(cycles.size() == 2);
    CHECK(cycles[0].files.size() == 2);
    CHECK(cycles[1].files.size() == 2);
}
