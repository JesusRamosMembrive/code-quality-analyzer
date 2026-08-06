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
    callable.localVariables.push_back(codeqa::model::LocalVariableModel{
        {"index", "index", {}}, "int", true, 1});
    callable.controlFlow.push_back(codeqa::model::ControlFlowModel{
        codeqa::model::ControlFlowKind::For, {}, 0, {}});
    callable.calls.push_back(codeqa::model::CallModel{"callback", codeqa::model::CallKind::Named, 1, {}});
    file.callables.push_back(callable);
    CHECK(file.callables[0].symbol.name == "sum");

    CHECK(callable.localVariables[0].symbol.name == "index");
    CHECK_FALSE(callable.controlFlow[0].conditionRange.has_value());
    CHECK(callable.calls[0].kind == codeqa::model::CallKind::Named);
    CHECK(file.callables[0].localVariables[0].symbol.qualifiedName == "index");
    CHECK(file.callables[0].controlFlow[0].kind == codeqa::model::ControlFlowKind::For);
    CHECK(file.callables[0].calls[0].argumentCount == 1);

    codeqa::model::CallModel defaultCall;
    CHECK(defaultCall.kind == codeqa::model::CallKind::Named);
}
