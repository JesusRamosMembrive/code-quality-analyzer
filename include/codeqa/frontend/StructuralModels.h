#pragma once

#include "codeqa/core/SourceRange.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::frontend
{

enum class FunctionKind
{
    Free,
    Method,
    Constructor,
    Destructor,
    Operator,
    Lambda,
    Prototype
};

enum class ParameterPassing
{
    Value,
    Pointer,
    Reference,
    FunctionPointer
};

enum class ControlFlowKind
{
    If,
    Switch,
    For,
    While,
    DoWhile,
    Case,
    Default,
    Conditional,
    Return,
    Break,
    Continue
};

enum class CallKind
{
    Named,
    Member,
    Indirect
};

enum class ClassKind
{
    Class,
    Struct,
    Union,
    Enum,
    Namespace
};

enum class IncludeDelimiter
{
    Local,
    System
};

struct ParameterModel
{
    std::string name;
    ParameterPassing passing{ParameterPassing::Value};
    bool hasDefaultValue{false};
};

struct LocalVariableModel
{
    std::string name;
    std::string declaredType;
    core::SourceRange range;
    bool hasInitializer{false};
    std::size_t lexicalDepth{0};
};

struct ControlFlowModel
{
    ControlFlowKind kind{ControlFlowKind::If};
    core::SourceRange range;
    std::size_t nestingDepth{0};
    std::optional<core::SourceRange> conditionRange;
};

struct CallModel
{
    std::string calleeText;
    CallKind kind{CallKind::Named};
    std::size_t argumentCount{0};
    core::SourceRange range;
};

struct FunctionModel
{
    std::string name;
    FunctionKind kind{FunctionKind::Free};
    core::SourceRange range;
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
};

struct ClassModel
{
    std::string name;
    ClassKind kind{ClassKind::Class};
    core::SourceRange range;
    bool isTemplate{false};
    bool isScopedEnum{false};
    std::vector<std::string> baseSpecifiers;
};

struct IncludeDependency
{
    std::string target;
    IncludeDelimiter delimiter{IncludeDelimiter::Local};
    core::SourceRange range;
};

struct GlobalVariableModel
{
    std::string name;
    std::string declaredType;
    core::SourceRange range;
    bool isStatic{false};
    bool isExtern{false};
    bool hasInitializer{false};
};

struct TypeAliasModel
{
    std::string name;
    std::string aliasedType;
    core::SourceRange range;
};

} // namespace codeqa::frontend
