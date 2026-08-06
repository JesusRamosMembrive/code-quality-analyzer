#pragma once

#include "codeqa/core/SourceRange.h"

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

struct FunctionModel
{
    std::string name;
    FunctionKind kind{FunctionKind::Free};
    core::SourceRange range;
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
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
