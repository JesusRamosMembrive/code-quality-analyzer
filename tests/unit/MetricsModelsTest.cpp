#include "doctest/doctest.h"

#include "codeqa/metrics/MetricsModels.h"

TEST_CASE("metrics models default to zero-valued, empty state")
{
    codeqa::metrics::FunctionMetricsModel function;
    CHECK(function.symbol.name.empty());
    CHECK(function.size.physicalLines == 0);
    CHECK(function.size.codeLines == 0);
    CHECK(function.size.blankLines == 0);
    CHECK(function.size.commentLines == 0);
    CHECK(function.size.statementCount == 0);
    CHECK(function.signature.parameterCount == 0);
    CHECK(function.signature.signatureLength == 0);
    CHECK(function.signature.booleanParameterCount == 0);
    CHECK(function.complexity.cyclomaticComplexity == 0);
    CHECK(function.complexity.maxConditionComplexity == 0);
    CHECK(function.complexity.maxNestingDepth == 0);
    CHECK(function.complexity.branchCount == 0);
    CHECK(function.complexity.returnCount == 0);

    codeqa::metrics::FileMetricsModel file;
    CHECK(file.path.empty());
    CHECK(file.functions.empty());
    CHECK(file.diagnostics.empty());

    codeqa::metrics::ProjectMetricsModel project;
    CHECK(project.rootPath.empty());
    CHECK(project.files.empty());
}
