#include "codeqa/model/ProjectModelBuilder.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace codeqa::model
{
namespace
{

CallableKind callableKindFor(frontend::FunctionKind kind)
{
    switch (kind)
    {
    case frontend::FunctionKind::Free:
        return CallableKind::FreeFunction;
    case frontend::FunctionKind::Method:
        return CallableKind::Method;
    case frontend::FunctionKind::Constructor:
        return CallableKind::Constructor;
    case frontend::FunctionKind::Destructor:
        return CallableKind::Destructor;
    case frontend::FunctionKind::Operator:
        return CallableKind::Operator;
    case frontend::FunctionKind::Lambda:
        return CallableKind::Lambda;
    case frontend::FunctionKind::Prototype:
        return CallableKind::Prototype;
    }

    throw std::logic_error("Unknown frontend function kind.");
}

ParameterPassing parameterPassingFor(frontend::ParameterPassing passing)
{
    switch (passing)
    {
    case frontend::ParameterPassing::Value:
        return ParameterPassing::Value;
    case frontend::ParameterPassing::Pointer:
        return ParameterPassing::Pointer;
    case frontend::ParameterPassing::Reference:
        return ParameterPassing::Reference;
    case frontend::ParameterPassing::FunctionPointer:
        return ParameterPassing::FunctionPointer;
    }

    throw std::logic_error("Unknown frontend parameter passing.");
}

ControlFlowKind controlFlowKindFor(frontend::ControlFlowKind kind)
{
    switch (kind)
    {
    case frontend::ControlFlowKind::If:
        return ControlFlowKind::If;
    case frontend::ControlFlowKind::Switch:
        return ControlFlowKind::Switch;
    case frontend::ControlFlowKind::For:
        return ControlFlowKind::For;
    case frontend::ControlFlowKind::While:
        return ControlFlowKind::While;
    case frontend::ControlFlowKind::DoWhile:
        return ControlFlowKind::DoWhile;
    case frontend::ControlFlowKind::Case:
        return ControlFlowKind::Case;
    case frontend::ControlFlowKind::Default:
        return ControlFlowKind::Default;
    case frontend::ControlFlowKind::Conditional:
        return ControlFlowKind::Conditional;
    case frontend::ControlFlowKind::Return:
        return ControlFlowKind::Return;
    case frontend::ControlFlowKind::Break:
        return ControlFlowKind::Break;
    case frontend::ControlFlowKind::Continue:
        return ControlFlowKind::Continue;
    }

    throw std::logic_error("Unknown frontend control flow kind.");
}

CallKind callKindFor(frontend::CallKind kind)
{
    switch (kind)
    {
    case frontend::CallKind::Named:
        return CallKind::Named;
    case frontend::CallKind::Member:
        return CallKind::Member;
    case frontend::CallKind::Indirect:
        return CallKind::Indirect;
    }

    throw std::logic_error("Unknown frontend call kind.");
}

TypeKind typeKindFor(frontend::ClassKind kind)
{
    switch (kind)
    {
    case frontend::ClassKind::Class:
        return TypeKind::Class;
    case frontend::ClassKind::Struct:
        return TypeKind::Struct;
    case frontend::ClassKind::Union:
        return TypeKind::Union;
    case frontend::ClassKind::Enum:
        return TypeKind::Enum;
    case frontend::ClassKind::Namespace:
        throw std::logic_error("A namespace is not a type.");
    }

    throw std::logic_error("Unknown frontend class kind.");
}

IncludeKind includeKindFor(frontend::IncludeDelimiter delimiter)
{
    switch (delimiter)
    {
    case frontend::IncludeDelimiter::Local:
        return IncludeKind::Local;
    case frontend::IncludeDelimiter::System:
        return IncludeKind::System;
    }

    throw std::logic_error("Unknown frontend include delimiter.");
}

AnalysisState analysisStateFor(const frontend::ParsedFile& file)
{
    if (!file.parsed)
    {
        return AnalysisState::Unavailable;
    }

    return file.diagnostics.empty() ? AnalysisState::Complete : AnalysisState::Partial;
}

bool encloses(const core::SourceRange& enclosingRange, const core::SourceRange& nestedRange)
{
    return enclosingRange.begin.offset <= nestedRange.begin.offset
        && enclosingRange.end.offset >= nestedRange.end.offset
        && (enclosingRange.begin.offset < nestedRange.begin.offset
            || enclosingRange.end.offset > nestedRange.end.offset);
}

bool isQualifiedName(const std::string& name)
{
    return name.find("::") != std::string::npos;
}

std::string qualifiedNameFor(
    const std::string& name,
    const core::SourceRange& range,
    const std::vector<frontend::ClassModel>& classes,
    bool preserveQualifiedName)
{
    if (name.empty())
    {
        return {};
    }

    std::vector<const frontend::ClassModel*> enclosingScopes;

    for (const frontend::ClassModel& candidate : classes)
    {
        if (candidate.name.empty() || !encloses(candidate.range, range))
        {
            continue;
        }

        enclosingScopes.push_back(&candidate);
    }

    std::stable_sort(
        enclosingScopes.begin(),
        enclosingScopes.end(),
        [](const frontend::ClassModel* left, const frontend::ClassModel* right) {
            if (left->range.begin.offset != right->range.begin.offset)
            {
                return left->range.begin.offset < right->range.begin.offset;
            }

            return left->range.end.offset > right->range.end.offset;
        });

    if (preserveQualifiedName && isQualifiedName(name))
    {
        // `name` already carries whatever qualification the frontend captured verbatim
        // from source text (eg. an out-of-line "Engine::run" definition, which omits any
        // enclosing namespace the definition itself never spells out). Prepend only the
        // enclosing scopes that aren't already a leading prefix of `name`, so a namespace
        // that lexically encloses the definition but isn't part of that text still ends up
        // in the result, without double-prepending scopes `name` already spells out.
        std::size_t matchedScopeCount = 0;
        std::size_t searchPosition = 0;

        for (; matchedScopeCount < enclosingScopes.size(); ++matchedScopeCount)
        {
            const std::string prefix = enclosingScopes[matchedScopeCount]->name + "::";

            if (name.compare(searchPosition, prefix.size(), prefix) != 0)
            {
                break;
            }

            searchPosition += prefix.size();
        }

        std::string qualifiedName;

        for (std::size_t index = matchedScopeCount; index < enclosingScopes.size(); ++index)
        {
            qualifiedName += enclosingScopes[index]->name + "::";
        }

        return qualifiedName + name;
    }

    std::string qualifiedName;

    for (const frontend::ClassModel* scope : enclosingScopes)
    {
        qualifiedName += scope->name + "::";
    }

    return qualifiedName + name;
}

SymbolIdentity identityFor(
    const std::string& name,
    const core::SourceRange& range,
    const std::vector<frontend::ClassModel>& classes,
    bool preserveQualifiedName = false)
{
    return SymbolIdentity{name, qualifiedNameFor(name, range, classes, preserveQualifiedName), range};
}

template <typename Model, typename RangeAccessor>
void sortByRange(std::vector<Model>& models, RangeAccessor rangeFor)
{
    std::stable_sort(models.begin(), models.end(), [&rangeFor](const Model& left, const Model& right) {
        const core::SourceRange& leftRange = rangeFor(left);
        const core::SourceRange& rightRange = rangeFor(right);

        if (leftRange.begin.offset != rightRange.begin.offset)
        {
            return leftRange.begin.offset < rightRange.begin.offset;
        }

        return leftRange.end.offset < rightRange.end.offset;
    });
}

FileModel buildFileModel(const frontend::ParsedFile& file)
{
    FileModel model;
    model.path = file.path;
    model.language = file.language;
    model.state = analysisStateFor(file);
    model.diagnostics = file.diagnostics;

    if (!file.parsed)
    {
        return model;
    }

    for (const frontend::IncludeDependency& dependency : file.includes)
    {
        model.dependencies.push_back(DependencyModel{
            dependency.target, includeKindFor(dependency.delimiter), dependency.range});
    }

    for (const frontend::FunctionModel& function : file.functions)
    {
        CallableModel callable;
        callable.symbol = identityFor(function.name, function.range, file.classes, true);
        callable.kind = callableKindFor(function.kind);
        callable.isStatic = function.isStatic;
        callable.isVariadic = function.isVariadic;

        for (const frontend::ParameterModel& parameter : function.parameters)
        {
            callable.parameters.push_back(ParameterModel{
                parameter.name, parameterPassingFor(parameter.passing), parameter.hasDefaultValue});
        }

        for (const frontend::LocalVariableModel& local : function.localVariables)
        {
            callable.localVariables.push_back(LocalVariableModel{
                identityFor(local.name, local.range, file.classes),
                local.declaredType,
                local.hasInitializer,
                local.lexicalDepth});
        }

        for (const frontend::ControlFlowModel& flow : function.controlFlow)
        {
            callable.controlFlow.push_back(ControlFlowModel{
                controlFlowKindFor(flow.kind), flow.range, flow.nestingDepth, flow.conditionRange});
        }

        for (const frontend::CallModel& call : function.calls)
        {
            callable.calls.push_back(CallModel{call.calleeText, callKindFor(call.kind), call.argumentCount, call.range});
        }

        sortByRange(callable.localVariables, [](const LocalVariableModel& local) -> const core::SourceRange& {
            return local.symbol.range;
        });
        sortByRange(callable.controlFlow, [](const ControlFlowModel& flow) -> const core::SourceRange& {
            return flow.range;
        });
        sortByRange(callable.calls, [](const CallModel& call) -> const core::SourceRange& {
            return call.range;
        });

        model.callables.push_back(std::move(callable));
    }

    for (const frontend::ClassModel& type : file.classes)
    {
        if (type.kind == frontend::ClassKind::Namespace)
        {
            model.namespaces.push_back(NamespaceModel{
                identityFor(type.name, type.range, file.classes)});
            continue;
        }

        TypeModel typeModel;
        typeModel.symbol = identityFor(type.name, type.range, file.classes);
        typeModel.kind = typeKindFor(type.kind);

        if (file.grammar == frontend::Grammar::Cpp)
        {
            typeModel.cppDetails = CppTypeDetails{
                type.isTemplate, type.isScopedEnum, type.baseSpecifiers};
        }

        model.types.push_back(std::move(typeModel));
    }

    for (const frontend::GlobalVariableModel& variable : file.globalVariables)
    {
        model.globalVariables.push_back(GlobalVariableModel{
            identityFor(variable.name, variable.range, file.classes, true),
            variable.declaredType,
            variable.isStatic,
            variable.isExtern,
            variable.hasInitializer});
    }

    for (const frontend::TypeAliasModel& alias : file.typeAliases)
    {
        model.typeAliases.push_back(TypeAliasModel{
            identityFor(alias.name, alias.range, file.classes), alias.aliasedType});
    }

    sortByRange(model.dependencies, [](const DependencyModel& dependency) -> const core::SourceRange& {
        return dependency.range;
    });
    sortByRange(model.callables, [](const CallableModel& callable) -> const core::SourceRange& {
        return callable.symbol.range;
    });
    sortByRange(model.types, [](const TypeModel& type) -> const core::SourceRange& {
        return type.symbol.range;
    });
    sortByRange(model.namespaces, [](const NamespaceModel& nameSpace) -> const core::SourceRange& {
        return nameSpace.symbol.range;
    });
    sortByRange(model.globalVariables, [](const GlobalVariableModel& variable) -> const core::SourceRange& {
        return variable.symbol.range;
    });
    sortByRange(model.typeAliases, [](const TypeAliasModel& alias) -> const core::SourceRange& {
        return alias.symbol.range;
    });

    return model;
}

} // namespace

ProjectModel ProjectModelBuilder::build(
    const std::filesystem::path& rootPath,
    const std::vector<frontend::ParsedFile>& files) const
{
    ProjectModel project;
    project.rootPath = rootPath;

    for (const frontend::ParsedFile& file : files)
    {
        project.files.push_back(buildFileModel(file));
    }

    std::stable_sort(project.files.begin(), project.files.end(), [](const FileModel& left, const FileModel& right) {
        return left.path.generic_string() < right.path.generic_string();
    });

    return project;
}

} // namespace codeqa::model
