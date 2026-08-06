#include "codeqa/frontend/StructuralExtractor.h"

#include "TreeSitterInternals.h"

#include "codeqa/frontend/EmbeddedQueries.h"
#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/TreeSitterParser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace codeqa::frontend
{

namespace
{

struct QueryDeleter
{
    void operator()(TSQuery* query) const
    {
        ts_query_delete(query);
    }
};

using QueryPointer = std::unique_ptr<TSQuery, QueryDeleter>;

struct QueryCursorDeleter
{
    void operator()(TSQueryCursor* cursor) const
    {
        ts_query_cursor_delete(cursor);
    }
};

using QueryCursorPointer = std::unique_ptr<TSQueryCursor, QueryCursorDeleter>;

QueryPointer compileQuery(
    const TSLanguage* language,
    std::string_view assetName,
    std::string_view querySource)
{
    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    TSQuery* query = ts_query_new(
        language,
        querySource.data(),
        static_cast<uint32_t>(querySource.size()),
        &errorOffset,
        &errorType);

    if (query == nullptr)
    {
        throw std::runtime_error(
            "Invalid tree-sitter query '" + std::string(assetName)
            + "' at byte offset " + std::to_string(errorOffset));
    }

    return QueryPointer(query);
}

bool isNodeType(TSNode node, const char* type)
{
    return !ts_node_is_null(node) && std::string_view(ts_node_type(node)) == type;
}

TSNode findDescendantOfType(TSNode node, const char* type)
{
    if (ts_node_is_null(node))
    {
        return TSNode{};
    }

    if (isNodeType(node, type))
    {
        return node;
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode found = findDescendantOfType(ts_node_child(node, index), type);

        if (!ts_node_is_null(found))
        {
            return found;
        }
    }

    return TSNode{};
}

TSNode directChildOfType(TSNode node, const char* type)
{
    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_child(node, index);

        if (isNodeType(child, type))
        {
            return child;
        }
    }

    return TSNode{};
}

TSNode childForField(TSNode node, const char* field)
{
    return ts_node_child_by_field_name(node, field, static_cast<uint32_t>(std::char_traits<char>::length(field)));
}

TSNode directNameChild(TSNode node)
{
    TSNode name = childForField(node, "name");

    if (!ts_node_is_null(name))
    {
        return name;
    }

    const uint32_t childCount = ts_node_named_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_named_child(node, index);

        if (isNodeType(child, "identifier") || isNodeType(child, "type_identifier")
            || isNodeType(child, "namespace_identifier"))
        {
            return child;
        }
    }

    return TSNode{};
}

std::string nodeText(TSNode node, std::string_view source)
{
    const std::size_t start = ts_node_start_byte(node);
    const std::size_t end = ts_node_end_byte(node);

    if (start > end || end > source.size())
    {
        return {};
    }

    return std::string(source.substr(start, end - start));
}

std::string trim(std::string text)
{
    const auto isSpace = [](unsigned char value) { return std::isspace(value) != 0; };
    const auto first = std::find_if_not(text.begin(), text.end(), isSpace);
    const auto last = std::find_if_not(text.rbegin(), text.rend(), isSpace).base();

    if (first >= last)
    {
        return {};
    }

    return std::string(first, last);
}

bool hasStandaloneToken(std::string_view text, std::string_view token)
{
    const auto isIdentifierCharacter = [](char value) {
        return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
    };

    std::size_t position = text.find(token);

    while (position != std::string_view::npos)
    {
        const bool startsToken = position == 0 || !isIdentifierCharacter(text[position - 1]);
        const std::size_t after = position + token.size();
        const bool endsToken = after == text.size() || !isIdentifierCharacter(text[after]);

        if (startsToken && endsToken)
        {
            return true;
        }

        position = text.find(token, position + 1);
    }

    return false;
}

TSNode findFunctionDeclarator(TSNode node)
{
    return findDescendantOfType(node, "function_declarator");
}

TSNode findParameterList(TSNode functionDeclarator)
{
    TSNode parameters = childForField(functionDeclarator, "parameters");

    if (!ts_node_is_null(parameters))
    {
        return parameters;
    }

    const TSNode directParameters = directChildOfType(functionDeclarator, "parameter_list");

    return ts_node_is_null(directParameters)
        ? findDescendantOfType(functionDeclarator, "parameter_list")
        : directParameters;
}

TSNode findDeclarator(TSNode node)
{
    TSNode declarator = childForField(node, "declarator");

    if (!ts_node_is_null(declarator))
    {
        return declarator;
    }

    const uint32_t childCount = ts_node_named_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode found = findDeclarator(ts_node_named_child(node, index));

        if (!ts_node_is_null(found))
        {
            return found;
        }
    }

    return TSNode{};
}

TSNode findIdentifier(TSNode node)
{
    if (isNodeType(node, "identifier") || isNodeType(node, "field_identifier"))
    {
        return node;
    }

    const uint32_t childCount = ts_node_named_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode found = findIdentifier(ts_node_named_child(node, index));

        if (!ts_node_is_null(found))
        {
            return found;
        }
    }

    return TSNode{};
}

std::string declaratorName(TSNode declarator, std::string_view source)
{
    if (ts_node_is_null(declarator))
    {
        return {};
    }

    if (isNodeType(declarator, "qualified_identifier") || isNodeType(declarator, "destructor_name")
        || isNodeType(declarator, "operator_name"))
    {
        return trim(nodeText(declarator, source));
    }

    TSNode nestedDeclarator = childForField(declarator, "declarator");

    if (!ts_node_is_null(nestedDeclarator))
    {
        const std::string nestedName = declaratorName(nestedDeclarator, source);

        if (!nestedName.empty())
        {
            return nestedName;
        }
    }

    const TSNode identifier = findIdentifier(declarator);
    return ts_node_is_null(identifier) ? std::string{} : nodeText(identifier, source);
}

bool isFunctionPointerDeclarator(TSNode node)
{
    return !ts_node_is_null(findDescendantOfType(node, "function_declarator"))
        && !ts_node_is_null(findDescendantOfType(node, "pointer_declarator"));
}

bool hasClassAncestor(TSNode node)
{
    for (TSNode parent = ts_node_parent(node); !ts_node_is_null(parent); parent = ts_node_parent(parent))
    {
        if (isNodeType(parent, "class_specifier") || isNodeType(parent, "struct_specifier"))
        {
            return true;
        }
    }

    return false;
}

std::string finalNameComponent(std::string_view name)
{
    const std::size_t separator = name.rfind("::");
    return std::string(name.substr(separator == std::string_view::npos ? 0 : separator + 2));
}

bool isConstructorName(std::string_view name)
{
    const std::size_t separator = name.rfind("::");

    if (separator == std::string_view::npos)
    {
        return false;
    }

    const std::string last = finalNameComponent(name);
    const std::string prefix{name.substr(0, separator)};
    return last == finalNameComponent(prefix);
}

std::string enclosingClassName(TSNode node, std::string_view source)
{
    for (TSNode parent = ts_node_parent(node); !ts_node_is_null(parent); parent = ts_node_parent(parent))
    {
        if (!isNodeType(parent, "class_specifier") && !isNodeType(parent, "struct_specifier"))
        {
            continue;
        }

        TSNode name = childForField(parent, "name");

        if (ts_node_is_null(name))
        {
            name = findDescendantOfType(parent, "type_identifier");
        }

        return ts_node_is_null(name) ? std::string{} : nodeText(name, source);
    }

    return {};
}

FunctionKind functionKind(
    TSNode candidate,
    TSNode declarator,
    const std::string& name,
    std::string_view source)
{
    if (isNodeType(candidate, "lambda_expression"))
    {
        return FunctionKind::Lambda;
    }

    if (!isNodeType(candidate, "function_definition"))
    {
        return FunctionKind::Prototype;
    }

    if (!ts_node_is_null(findDescendantOfType(declarator, "operator_name")))
    {
        return FunctionKind::Operator;
    }

    if (finalNameComponent(name).find('~') != std::string::npos)
    {
        return FunctionKind::Destructor;
    }

    if (isConstructorName(name)
        || (!enclosingClassName(candidate, source).empty()
            && finalNameComponent(name) == enclosingClassName(candidate, source)))
    {
        return FunctionKind::Constructor;
    }

    if (name.find("::") != std::string::npos || hasClassAncestor(candidate))
    {
        return FunctionKind::Method;
    }

    return FunctionKind::Free;
}

core::SourceRange sourceRange(TSNode node, const std::filesystem::path& path)
{
    const TSPoint start = ts_node_start_point(node);
    const TSPoint end = ts_node_end_point(node);

    return core::SourceRange{
        core::SourceLocation{
            path,
            static_cast<std::size_t>(start.row) + 1,
            static_cast<std::size_t>(start.column) + 1,
            static_cast<std::size_t>(ts_node_start_byte(node))},
        core::SourceLocation{
            path,
            static_cast<std::size_t>(end.row) + 1,
            static_cast<std::size_t>(end.column) + 1,
            static_cast<std::size_t>(ts_node_end_byte(node))}};
}

core::SourceRange sourceRangeForCandidate(TSNode candidate, const std::filesystem::path& path)
{
    const TSNode parent = ts_node_parent(candidate);
    return sourceRange(isNodeType(parent, "template_declaration") ? parent : candidate, path);
}

std::vector<ParameterModel> parametersFor(TSNode functionDeclarator, std::string_view source)
{
    std::vector<ParameterModel> parameters;
    const TSNode parameterList = findParameterList(functionDeclarator);

    if (ts_node_is_null(parameterList))
    {
        return parameters;
    }

    const uint32_t childCount = ts_node_named_child_count(parameterList);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode parameter = ts_node_named_child(parameterList, index);

        if (!isNodeType(parameter, "parameter_declaration")
            && !isNodeType(parameter, "optional_parameter_declaration"))
        {
            continue;
        }

        const TSNode declarator = findDeclarator(parameter);
        const TSNode defaultValue = findDescendantOfType(parameter, "default_value");

        ParameterModel model;
        model.name = declaratorName(declarator, source);
        model.passing = isFunctionPointerDeclarator(declarator)
            ? ParameterPassing::FunctionPointer
            : (!ts_node_is_null(findDescendantOfType(declarator, "reference_declarator"))
                      || !ts_node_is_null(
                          findDescendantOfType(declarator, "abstract_reference_declarator"))
                    ? ParameterPassing::Reference
                    : (!ts_node_is_null(findDescendantOfType(declarator, "pointer_declarator"))
                              || !ts_node_is_null(
                                  findDescendantOfType(declarator, "abstract_pointer_declarator"))
                            ? ParameterPassing::Pointer
                            : ParameterPassing::Value));
        model.hasDefaultValue = !ts_node_is_null(defaultValue)
            || isNodeType(parameter, "optional_parameter_declaration");
        parameters.push_back(std::move(model));
    }

    return parameters;
}

bool isVariadic(TSNode functionDeclarator, std::string_view source)
{
    const TSNode parameterList = findParameterList(functionDeclarator);

    if (ts_node_is_null(parameterList))
    {
        return false;
    }

    return !ts_node_is_null(findDescendantOfType(parameterList, "variadic_parameter"))
        || nodeText(parameterList, source).find("...") != std::string::npos;
}

FunctionModel adaptFunction(
    TSNode candidate,
    std::string_view source,
    const std::filesystem::path& path)
{
    const bool isLambda = isNodeType(candidate, "lambda_expression");
    const TSNode functionDeclarator = isLambda
        ? candidate
        : findFunctionDeclarator(candidate);
    const TSNode declarator = findDeclarator(functionDeclarator);
    const std::string name = isLambda
        ? "<lambda>"
        : declaratorName(declarator, source);
    const std::string specifiers = isLambda
        ? std::string{}
        : std::string(source.substr(
              ts_node_start_byte(candidate),
              ts_node_start_byte(functionDeclarator) - ts_node_start_byte(candidate)));

    FunctionModel model;
    model.name = name;
    model.kind = functionKind(candidate, declarator, name, source);
    model.range = sourceRangeForCandidate(candidate, path);
    model.parameters = parametersFor(functionDeclarator, source);
    model.isStatic = hasStandaloneToken(specifiers, "static");
    model.isVariadic = isVariadic(functionDeclarator, source);
    return model;
}

ClassKind classKind(TSNode candidate)
{
    if (isNodeType(candidate, "class_specifier"))
    {
        return ClassKind::Class;
    }

    if (isNodeType(candidate, "struct_specifier"))
    {
        return ClassKind::Struct;
    }

    if (isNodeType(candidate, "union_specifier"))
    {
        return ClassKind::Union;
    }

    if (isNodeType(candidate, "enum_specifier"))
    {
        return ClassKind::Enum;
    }

    return ClassKind::Namespace;
}

std::vector<std::string> baseSpecifiersFor(TSNode candidate, std::string_view source)
{
    std::vector<std::string> bases;

    if (!isNodeType(candidate, "class_specifier") && !isNodeType(candidate, "struct_specifier"))
    {
        return bases;
    }

    const TSNode clause = directChildOfType(candidate, "base_class_clause");

    if (ts_node_is_null(clause))
    {
        return bases;
    }

    const uint32_t childCount = ts_node_named_child_count(clause);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_named_child(clause, index);

        if (!isNodeType(child, "access_specifier"))
        {
            bases.push_back(trim(nodeText(child, source)));
        }
    }

    return bases;
}

ClassModel adaptType(TSNode candidate, std::string_view source, const std::filesystem::path& path)
{
    const ClassKind kind = classKind(candidate);
    const TSNode name = directNameChild(candidate);
    const TSNode parent = ts_node_parent(candidate);

    ClassModel model;
    model.name = ts_node_is_null(name) ? std::string{} : nodeText(name, source);
    model.kind = kind;
    model.range = sourceRangeForCandidate(candidate, path);
    model.isTemplate = isNodeType(parent, "template_declaration");
    model.isScopedEnum = kind == ClassKind::Enum
        && (!ts_node_is_null(directChildOfType(candidate, "class"))
            || !ts_node_is_null(directChildOfType(candidate, "struct")));
    model.baseSpecifiers = baseSpecifiersFor(candidate, source);
    return model;
}

IncludeDependency adaptInclude(
    TSNode candidate,
    std::string_view source,
    const std::filesystem::path& path)
{
    const TSNode includePath = childForField(candidate, "path");
    const std::string text = nodeText(includePath, source);
    const bool hasAngleDelimiters = text.size() >= 2 && text.front() == '<' && text.back() == '>';
    const bool hasQuoteDelimiters = text.size() >= 2 && text.front() == '"' && text.back() == '"';

    IncludeDependency model;
    model.target = hasAngleDelimiters || hasQuoteDelimiters ? text.substr(1, text.size() - 2) : text;
    model.delimiter = hasAngleDelimiters
        ? IncludeDelimiter::System
        : IncludeDelimiter::Local;
    model.range = sourceRange(candidate, path);
    return model;
}

void collectNodesOfType(TSNode node, const char* type, std::vector<TSNode>& nodes)
{
    if (isNodeType(node, type))
    {
        nodes.push_back(node);
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        collectNodesOfType(ts_node_child(node, index), type, nodes);
    }
}

bool hasSameFunctionRangeAndKind(const FunctionModel& left, const FunctionModel& right)
{
    return left.kind == right.kind && left.range.begin.offset == right.range.begin.offset
        && left.range.end.offset == right.range.end.offset;
}

bool hasSameClassRangeAndKind(const ClassModel& left, const ClassModel& right)
{
    return left.kind == right.kind && left.range.begin.offset == right.range.begin.offset
        && left.range.end.offset == right.range.end.offset;
}

bool hasSameIncludeRange(const IncludeDependency& left, const IncludeDependency& right)
{
    return left.range.begin.offset == right.range.begin.offset
        && left.range.end.offset == right.range.end.offset;
}

} // namespace

class StructuralExtractor::Impl
{
public:
    Impl()
        : cFunctions_(compileQuery(tree_sitter_c(), "queries/c/functions.scm", embedded_queries::cFunctions))
        , cTypes_(compileQuery(tree_sitter_c(), "queries/c/types.scm", embedded_queries::cTypes))
        , cIncludes_(compileQuery(tree_sitter_c(), "queries/c/includes.scm", embedded_queries::cIncludes))
        , cppFunctions_(compileQuery(tree_sitter_cpp(), "queries/cpp/functions.scm", embedded_queries::cppFunctions))
        , cppTypes_(compileQuery(tree_sitter_cpp(), "queries/cpp/types.scm", embedded_queries::cppTypes))
        , cppIncludes_(compileQuery(tree_sitter_cpp(), "queries/cpp/includes.scm", embedded_queries::cppIncludes))
    {
    }

    [[nodiscard]] TSQuery* functionsFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cFunctions_.get() : cppFunctions_.get();
    }

    [[nodiscard]] TSQuery* typesFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cTypes_.get() : cppTypes_.get();
    }

    [[nodiscard]] TSQuery* includesFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cIncludes_.get() : cppIncludes_.get();
    }

private:
    QueryPointer cFunctions_;
    QueryPointer cTypes_;
    QueryPointer cIncludes_;
    QueryPointer cppFunctions_;
    QueryPointer cppTypes_;
    QueryPointer cppIncludes_;
};

StructuralExtractor::StructuralExtractor()
    : impl_(std::make_unique<Impl>())
{
}

StructuralExtractor::~StructuralExtractor() = default;

StructuralExtractor::StructuralExtractor(StructuralExtractor&&) noexcept = default;

StructuralExtractor& StructuralExtractor::operator=(StructuralExtractor&&) noexcept = default;

void StructuralExtractor::extract(
    const SyntaxTree& tree,
    Grammar grammar,
    std::string_view source,
    const std::filesystem::path& path,
    ParsedFile& result) const
{
    const TSNode root = tree.impl_->rootNode();
    QueryCursorPointer cursor(ts_query_cursor_new());
    ts_query_cursor_exec(cursor.get(), impl_->functionsFor(grammar), root);

    TSQueryMatch match{};

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            const TSNode candidate = match.captures[index].node;

            if (isNodeType(candidate, "declaration")
                && ts_node_is_null(findFunctionDeclarator(candidate)))
            {
                continue;
            }

            result.functions.push_back(adaptFunction(candidate, source, path));
        }
    }

    if (grammar == Grammar::Cpp)
    {
        std::vector<TSNode> lambdas;
        collectNodesOfType(root, "lambda_expression", lambdas);

        for (const TSNode lambda : lambdas)
        {
            result.functions.push_back(adaptFunction(lambda, source, path));
        }
    }

    std::sort(
        result.functions.begin(),
        result.functions.end(),
        [](const FunctionModel& left, const FunctionModel& right) {
            if (left.range.begin.offset != right.range.begin.offset)
            {
                return left.range.begin.offset < right.range.begin.offset;
            }

            return left.range.end.offset < right.range.end.offset;
        });
    result.functions.erase(
        std::unique(
            result.functions.begin(), result.functions.end(), hasSameFunctionRangeAndKind),
        result.functions.end());

    ts_query_cursor_exec(cursor.get(), impl_->typesFor(grammar), root);

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            const TSNode candidate = match.captures[index].node;

            if (ts_node_is_error(candidate) || ts_node_is_missing(candidate))
            {
                continue;
            }

            result.classes.push_back(adaptType(candidate, source, path));
        }
    }

    std::sort(
        result.classes.begin(),
        result.classes.end(),
        [](const ClassModel& left, const ClassModel& right) {
            if (left.range.begin.offset != right.range.begin.offset)
            {
                return left.range.begin.offset < right.range.begin.offset;
            }

            return left.range.end.offset < right.range.end.offset;
        });
    result.classes.erase(
        std::unique(result.classes.begin(), result.classes.end(), hasSameClassRangeAndKind),
        result.classes.end());

    ts_query_cursor_exec(cursor.get(), impl_->includesFor(grammar), root);

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            const TSNode candidate = match.captures[index].node;
            const TSNode includePath = childForField(candidate, "path");

            if (!ts_node_is_null(includePath))
            {
                result.includes.push_back(adaptInclude(candidate, source, path));
            }
        }
    }

    std::sort(
        result.includes.begin(),
        result.includes.end(),
        [](const IncludeDependency& left, const IncludeDependency& right) {
            if (left.range.begin.offset != right.range.begin.offset)
            {
                return left.range.begin.offset < right.range.begin.offset;
            }

            return left.range.end.offset < right.range.end.offset;
        });
    result.includes.erase(
        std::unique(result.includes.begin(), result.includes.end(), hasSameIncludeRange),
        result.includes.end());
}

} // namespace codeqa::frontend
