#include "doctest/doctest.h"

#include "codeqa/model/Models.h"

TEST_CASE("common model preserves typed file aggregates")
{
    codeqa::model::FileModel file;
    CHECK(file.state == codeqa::model::AnalysisState::Unavailable);
    CHECK(file.callables.empty());
    CHECK(file.globalVariables.empty());

    codeqa::model::CallableModel callable;
    callable.symbol.name = "sum";
    callable.kind = codeqa::model::CallableKind::FreeFunction;
    file.callables.push_back(callable);
    CHECK(file.callables[0].symbol.name == "sum");
}
