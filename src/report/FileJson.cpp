#include "codeqa/report/FileJson.h"

#include "codeqa/core/EnumJson.h"

namespace codeqa::report
{

using Json = nlohmann::json;

nlohmann::json locationJson(const codeqa::core::SourceLocation& location)
{
    return Json{{"line", location.line}, {"column", location.column}};
}

namespace
{

Json rangeJson(const codeqa::core::SourceRange& range)
{
    return Json{{"begin", locationJson(range.begin)}, {"end", locationJson(range.end)}};
}

std::string includeKindToJsonString(codeqa::model::IncludeKind kind)
{
    return kind == codeqa::model::IncludeKind::Local ? "local" : "system";
}

std::string typeKindToJsonString(codeqa::model::TypeKind kind)
{
    switch (kind)
    {
    case codeqa::model::TypeKind::Class:
        return "class";
    case codeqa::model::TypeKind::Struct:
        return "struct";
    case codeqa::model::TypeKind::Union:
        return "union";
    case codeqa::model::TypeKind::Enum:
        return "enum";
    }
    return "class";
}

std::string callableKindToJsonString(codeqa::model::CallableKind kind)
{
    switch (kind)
    {
    case codeqa::model::CallableKind::FreeFunction:
        return "freeFunction";
    case codeqa::model::CallableKind::Method:
        return "method";
    case codeqa::model::CallableKind::Constructor:
        return "constructor";
    case codeqa::model::CallableKind::Destructor:
        return "destructor";
    case codeqa::model::CallableKind::Operator:
        return "operator";
    case codeqa::model::CallableKind::Lambda:
        return "lambda";
    case codeqa::model::CallableKind::Prototype:
        return "prototype";
    }
    return "freeFunction";
}

std::string parameterPassingToJsonString(codeqa::model::ParameterPassing passing)
{
    switch (passing)
    {
    case codeqa::model::ParameterPassing::Value:
        return "value";
    case codeqa::model::ParameterPassing::Pointer:
        return "pointer";
    case codeqa::model::ParameterPassing::Reference:
        return "reference";
    case codeqa::model::ParameterPassing::FunctionPointer:
        return "functionPointer";
    case codeqa::model::ParameterPassing::Array:
        return "array";
    }
    return "value";
}

std::string controlFlowKindToJsonString(codeqa::model::ControlFlowKind kind)
{
    switch (kind)
    {
    case codeqa::model::ControlFlowKind::If:
        return "if";
    case codeqa::model::ControlFlowKind::Switch:
        return "switch";
    case codeqa::model::ControlFlowKind::For:
        return "for";
    case codeqa::model::ControlFlowKind::While:
        return "while";
    case codeqa::model::ControlFlowKind::DoWhile:
        return "doWhile";
    case codeqa::model::ControlFlowKind::Case:
        return "case";
    case codeqa::model::ControlFlowKind::Default:
        return "default";
    case codeqa::model::ControlFlowKind::Conditional:
        return "conditional";
    case codeqa::model::ControlFlowKind::Return:
        return "return";
    case codeqa::model::ControlFlowKind::Break:
        return "break";
    case codeqa::model::ControlFlowKind::Continue:
        return "continue";
    }
    return "if";
}

std::string callKindToJsonString(codeqa::model::CallKind kind)
{
    switch (kind)
    {
    case codeqa::model::CallKind::Named:
        return "named";
    case codeqa::model::CallKind::Member:
        return "member";
    case codeqa::model::CallKind::Indirect:
        return "indirect";
    }
    return "named";
}

std::string stateToJsonString(codeqa::model::AnalysisState state)
{
    switch (state)
    {
    case codeqa::model::AnalysisState::Complete:
        return "complete";
    case codeqa::model::AnalysisState::Partial:
        return "partial";
    case codeqa::model::AnalysisState::Unavailable:
        return "unavailable";
    }
    return "unavailable";
}

Json symbolIdentityJson(const codeqa::model::SymbolIdentity& symbol)
{
    return Json{{"name", symbol.name}, {"qualifiedName", symbol.qualifiedName}, {"range", rangeJson(symbol.range)}};
}

Json diagnosticJson(const codeqa::core::Diagnostic& diagnostic)
{
    return Json{
        {"code", diagnostic.code},
        {"severity", codeqa::core::diagnosticSeverityToJsonString(diagnostic.severity)},
        {"message", diagnostic.message},
        {"location", locationJson(diagnostic.location)}};
}

Json dependencyJson(const codeqa::model::DependencyModel& dependency)
{
    return Json{
        {"target", dependency.target},
        {"kind", includeKindToJsonString(dependency.kind)},
        {"range", rangeJson(dependency.range)}};
}

Json typeJson(const codeqa::model::TypeModel& type)
{
    Json document = symbolIdentityJson(type.symbol);
    document["kind"] = typeKindToJsonString(type.kind);

    if (type.cppDetails.has_value())
    {
        document["cppDetails"] = Json{
            {"isTemplate", type.cppDetails->isTemplate},
            {"isScopedEnum", type.cppDetails->isScopedEnum},
            {"baseSpecifiers", type.cppDetails->baseSpecifiers}};
    }

    return document;
}

Json globalVariableJson(const codeqa::model::GlobalVariableModel& global)
{
    Json document = symbolIdentityJson(global.symbol);
    document["declaredType"] = global.declaredType;
    document["isStatic"] = global.isStatic;
    document["isExtern"] = global.isExtern;
    document["hasInitializer"] = global.hasInitializer;
    return document;
}

Json typeAliasJson(const codeqa::model::TypeAliasModel& alias)
{
    Json document = symbolIdentityJson(alias.symbol);
    document["aliasedType"] = alias.aliasedType;
    return document;
}

Json parameterJson(const codeqa::model::ParameterModel& parameter)
{
    return Json{
        {"name", parameter.name},
        {"passing", parameterPassingToJsonString(parameter.passing)},
        {"hasDefaultValue", parameter.hasDefaultValue},
        {"declaredType", parameter.declaredType}};
}

Json localVariableJson(const codeqa::model::LocalVariableModel& local)
{
    Json document = symbolIdentityJson(local.symbol);
    document["declaredType"] = local.declaredType;
    document["hasInitializer"] = local.hasInitializer;
    document["lexicalDepth"] = local.lexicalDepth;
    return document;
}

Json controlFlowJson(const codeqa::model::ControlFlowModel& controlFlow)
{
    Json document{
        {"kind", controlFlowKindToJsonString(controlFlow.kind)},
        {"range", rangeJson(controlFlow.range)},
        {"nestingDepth", controlFlow.nestingDepth}};

    if (controlFlow.conditionRange.has_value())
    {
        document["conditionRange"] = rangeJson(*controlFlow.conditionRange);
    }

    return document;
}

Json callJson(const codeqa::model::CallModel& call)
{
    return Json{
        {"calleeText", call.calleeText},
        {"kind", callKindToJsonString(call.kind)},
        {"argumentCount", call.argumentCount},
        {"range", rangeJson(call.range)}};
}

Json callableJson(const codeqa::model::CallableModel& callable)
{
    Json document = symbolIdentityJson(callable.symbol);
    document["kind"] = callableKindToJsonString(callable.kind);
    document["isStatic"] = callable.isStatic;
    document["isVariadic"] = callable.isVariadic;

    document["parameters"] = Json::array();
    for (const codeqa::model::ParameterModel& parameter : callable.parameters)
    {
        document["parameters"].push_back(parameterJson(parameter));
    }

    if (callable.parameterListRange.has_value())
    {
        document["parameterListRange"] = rangeJson(*callable.parameterListRange);
    }

    document["localVariables"] = Json::array();
    for (const codeqa::model::LocalVariableModel& local : callable.localVariables)
    {
        document["localVariables"].push_back(localVariableJson(local));
    }

    document["controlFlow"] = Json::array();
    for (const codeqa::model::ControlFlowModel& controlFlow : callable.controlFlow)
    {
        document["controlFlow"].push_back(controlFlowJson(controlFlow));
    }

    document["calls"] = Json::array();
    for (const codeqa::model::CallModel& call : callable.calls)
    {
        document["calls"].push_back(callJson(call));
    }

    return document;
}

} // namespace

nlohmann::json fileJson(const codeqa::model::FileModel& file)
{
    Json document;
    document["path"] = file.path.generic_string();
    document["language"] = codeqa::core::languageToJsonString(file.language);
    document["state"] = stateToJsonString(file.state);

    document["diagnostics"] = Json::array();
    for (const codeqa::core::Diagnostic& diagnostic : file.diagnostics)
    {
        document["diagnostics"].push_back(diagnosticJson(diagnostic));
    }

    document["dependencies"] = Json::array();
    for (const codeqa::model::DependencyModel& dependency : file.dependencies)
    {
        document["dependencies"].push_back(dependencyJson(dependency));
    }

    document["namespaces"] = Json::array();
    for (const codeqa::model::NamespaceModel& namespaceModel : file.namespaces)
    {
        document["namespaces"].push_back(symbolIdentityJson(namespaceModel.symbol));
    }

    document["types"] = Json::array();
    for (const codeqa::model::TypeModel& type : file.types)
    {
        document["types"].push_back(typeJson(type));
    }

    document["globalVariables"] = Json::array();
    for (const codeqa::model::GlobalVariableModel& global : file.globalVariables)
    {
        document["globalVariables"].push_back(globalVariableJson(global));
    }

    document["typeAliases"] = Json::array();
    for (const codeqa::model::TypeAliasModel& alias : file.typeAliases)
    {
        document["typeAliases"].push_back(typeAliasJson(alias));
    }

    document["comments"] = Json::array();
    for (const codeqa::core::SourceRange& comment : file.comments)
    {
        document["comments"].push_back(Json{{"range", rangeJson(comment)}});
    }

    document["callables"] = Json::array();
    for (const codeqa::model::CallableModel& callable : file.callables)
    {
        document["callables"].push_back(callableJson(callable));
    }

    return document;
}

} // namespace codeqa::report
