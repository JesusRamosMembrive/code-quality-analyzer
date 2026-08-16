#pragma once

#include "codeqa/core/Diagnostic.h"
#include "codeqa/core/Language.h"
#include "codeqa/core/SourceRange.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace codeqa::model
{

enum class AnalysisState
{
    Complete,
    Partial,
    Unavailable
};

struct SymbolIdentity
{
    std::string name;
    std::string qualifiedName;
    core::SourceRange range;
};

enum class CallableKind
{
    FreeFunction,
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

enum class TypeKind
{
    Class,
    Struct,
    Union,
    Enum
};

enum class IncludeKind
{
    Local,
    System
};

struct ParameterModel
{
    std::string name;
    ParameterPassing passing{ParameterPassing::Value};
    bool hasDefaultValue{false};
    std::string declaredType;
};

struct LocalVariableModel
{
    SymbolIdentity symbol;
    std::string declaredType;
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

struct CallableModel
{
    SymbolIdentity symbol;
    CallableKind kind{CallableKind::FreeFunction};
    std::vector<ParameterModel> parameters;
    bool isStatic{false};
    bool isVariadic{false};
    std::vector<LocalVariableModel> localVariables;
    std::vector<ControlFlowModel> controlFlow;
    std::vector<CallModel> calls;
    std::optional<core::SourceRange> parameterListRange;
    std::vector<core::SourceRange> statements;
    std::string normalizedTokenSequence;
    std::size_t normalizedTokenCount{0};
};

struct CppTypeDetails
{
    bool isTemplate{false};
    bool isScopedEnum{false};
    std::vector<std::string> baseSpecifiers;
};

struct TypeModel
{
    SymbolIdentity symbol;
    TypeKind kind{TypeKind::Class};
    std::optional<CppTypeDetails> cppDetails;
};

struct NamespaceModel
{
    SymbolIdentity symbol;
};

struct GlobalVariableModel
{
    SymbolIdentity symbol;
    std::string declaredType;
    bool isStatic{false};
    bool isExtern{false};
    bool hasInitializer{false};
};

struct TypeAliasModel
{
    SymbolIdentity symbol;
    std::string aliasedType;
};

struct DependencyModel
{
    std::string target;
    IncludeKind kind{IncludeKind::Local};
    core::SourceRange range;
};

struct FileModel
{
    std::filesystem::path path;
    core::Language language{core::Language::Unknown};
    AnalysisState state{AnalysisState::Unavailable};
    std::vector<core::Diagnostic> diagnostics;
    std::vector<DependencyModel> dependencies;
    std::vector<CallableModel> callables;
    std::vector<TypeModel> types;
    std::vector<NamespaceModel> namespaces;
    std::vector<GlobalVariableModel> globalVariables;
    std::vector<TypeAliasModel> typeAliases;
    std::vector<core::SourceRange> comments;
};

struct ProjectModel
{
    std::filesystem::path rootPath;
    std::vector<FileModel> files;
};

} // namespace codeqa::model
