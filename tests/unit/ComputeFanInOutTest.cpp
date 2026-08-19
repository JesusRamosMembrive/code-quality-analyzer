#include "doctest/doctest.h"

#include "codeqa/architecture/IncludeGraph.h"

TEST_CASE("computeFanInOut counts in-degree and out-degree per file")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h", "c.h"};
    graph.outgoingEdges = {{1, 2}, {2}, {}};

    const std::vector<codeqa::architecture::FileDependencyMetrics> metrics =
        codeqa::architecture::computeFanInOut(graph);

    REQUIRE(metrics.size() == 3);
    CHECK(metrics[0].path == "a.h");
    CHECK(metrics[0].fanOut == 2);
    CHECK(metrics[0].fanIn == 0);
    CHECK(metrics[1].fanOut == 1);
    CHECK(metrics[1].fanIn == 1);
    CHECK(metrics[2].fanOut == 0);
    CHECK(metrics[2].fanIn == 2);
}

TEST_CASE("computeFanInOut returns zero for every file in a graph with no edges")
{
    codeqa::architecture::IncludeGraph graph;
    graph.filePaths = {"a.h", "b.h"};
    graph.outgoingEdges = {{}, {}};

    const std::vector<codeqa::architecture::FileDependencyMetrics> metrics =
        codeqa::architecture::computeFanInOut(graph);

    REQUIRE(metrics.size() == 2);
    CHECK(metrics[0].fanIn == 0);
    CHECK(metrics[0].fanOut == 0);
    CHECK(metrics[1].fanIn == 0);
    CHECK(metrics[1].fanOut == 0);
}
