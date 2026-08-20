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

struct NormalizedTokens
{
    std::string sequence;
    std::size_t count{0};
};

// ponytail: walks `candidate`'s own subtree only -- a leading storage-class
// specifier like `static` that tree-sitter parks in a sibling/parent node
// (see sourceRangeForCandidate) is not included. Acceptable for Type-1/Type-2
// clone comparison, which cares about body structure, not storage class.
void appendNormalizedTokens(TSNode node, std::string_view source, NormalizedTokens& tokens)
{
    if (isNodeType(node, "comment"))
    {
        return;
    }

    const bool isIdentifier = isNodeType(node, "identifier") || isNodeType(node, "field_identifier")
        || isNodeType(node, "type_identifier");
    const bool isLiteral = isNodeType(node, "number_literal") || isNodeType(node, "string_literal")
        || isNodeType(node, "char_literal");

    if (isIdentifier || isLiteral)
    {
        if (!tokens.sequence.empty())
        {
            tokens.sequence += ' ';
        }
        tokens.sequence += isIdentifier ? "ID" : "LIT";
        ++tokens.count;
        return;
    }

    const uint32_t childCount = ts_node_child_count(node);

    if (childCount == 0)
    {
        if (!tokens.sequence.empty())
        {
            tokens.sequence += ' ';
        }
        tokens.sequence += nodeText(node, source);
        ++tokens.count;
        return;
    }

    for (uint32_t index = 0; index < childCount; ++index)
    {
        appendNormalizedTokens(ts_node_child(node, index), source, tokens);
    }
}

NormalizedTokens normalizedTokensFor(TSNode node, std::string_view source)
{
    NormalizedTokens tokens;
    appendNormalizedTokens(node, source, tokens);
    return tokens;
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

std::string specifiersText(TSNode candidate, TSNode functionDeclarator, std::string_view source)
{
    const std::size_t rangeStart = ts_node_start_byte(candidate);
    const std::size_t rangeEnd = ts_node_start_byte(functionDeclarator);
    std::string text(source.substr(rangeStart, rangeEnd - rangeStart));
    const uint32_t childCount = ts_node_child_count(candidate);

    // Blank out comment nodes so a standalone-token scan (eg. for "static") can't be
    // fooled by a keyword that only appears inside a comment, like `int /* static */ f()`.
    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_child(candidate, index);

        if (!isNodeType(child, "comment"))
        {
            continue;
        }

        const std::size_t commentStart = ts_node_start_byte(child);
        const std::size_t commentEnd = ts_node_end_byte(child);

        if (commentStart < rangeStart || commentEnd > rangeEnd)
        {
            continue;
        }

        std::fill(text.begin() + (commentStart - rangeStart), text.begin() + (commentEnd - rangeStart), ' ');
    }

    return text;
}

TSNode findParameterList(TSNode functionDeclarator)
{
    if (ts_node_is_null(functionDeclarator))
    {
        return TSNode{};
    }

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
        || isNodeType(declarator, "operator_name") || isNodeType(declarator, "type_identifier"))
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

bool isFunctionPointerDeclaration(TSNode declarator)
{
    const TSNode nestedDeclarator = childForField(declarator, "declarator");
    return isNodeType(nestedDeclarator, "parenthesized_declarator")
        && !ts_node_is_null(directChildOfType(nestedDeclarator, "pointer_declarator"));
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

std::string parameterDeclaredType(TSNode parameter, TSNode declarator, std::string_view source)
{
    const std::size_t parameterStart = ts_node_start_byte(parameter);
    std::size_t typeEnd = 0;

    if (!ts_node_is_null(declarator))
    {
        typeEnd = ts_node_start_byte(declarator);
    }
    else
    {
        // An unnamed parameter has no declarator to bound the type text. Using the whole
        // parameter would swallow the default value of an optional_parameter_declaration
        // (`int = 5` instead of `int`), so bound it by the `type` field instead.
        const TSNode type = childForField(parameter, "type");
        typeEnd = ts_node_is_null(type)
            ? ts_node_end_byte(parameter)
            : ts_node_end_byte(type);
    }

    if (typeEnd < parameterStart || typeEnd > source.size())
    {
        return trim(nodeText(parameter, source));
    }

    return trim(std::string(source.substr(parameterStart, typeEnd - parameterStart)));
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
            : (!ts_node_is_null(findDescendantOfType(declarator, "array_declarator"))
                      || !ts_node_is_null(
                          findDescendantOfType(declarator, "abstract_array_declarator"))
                    ? ParameterPassing::Array
                    : (!ts_node_is_null(findDescendantOfType(declarator, "reference_declarator"))
                              || !ts_node_is_null(
                                  findDescendantOfType(declarator, "abstract_reference_declarator"))
                            ? ParameterPassing::Reference
                            : (!ts_node_is_null(findDescendantOfType(declarator, "pointer_declarator"))
                                      || !ts_node_is_null(findDescendantOfType(
                                          declarator, "abstract_pointer_declarator"))
                                    ? ParameterPassing::Pointer
                                    : ParameterPassing::Value)));
        model.hasDefaultValue = !ts_node_is_null(defaultValue)
            || isNodeType(parameter, "optional_parameter_declaration");
        model.declaredType = parameterDeclaredType(parameter, declarator, source);
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

bool subtreeContainsErrorOrMissing(TSNode node);

void extractBodyStructure(
    TSNode body,
    std::string_view source,
    const std::filesystem::path& path,
    FunctionModel& function);

FunctionModel adaptFunction(
    TSNode candidate,
    TSNode functionDeclarator,
    std::string_view source,
    const std::filesystem::path& path)
{
    const bool isLambda = isNodeType(candidate, "lambda_expression");
    functionDeclarator = isLambda ? candidate : functionDeclarator;
    const TSNode declarator = findDeclarator(functionDeclarator);
    const std::string name = isLambda
        ? "<lambda>"
        : declaratorName(declarator, source);
    const std::string specifiers = isLambda
        ? std::string{}
        : specifiersText(candidate, functionDeclarator, source);

    // A lambda_expression owns its body, so searching all of it for a parameter_list can
    // latch onto an unrelated nested one (eg. an inner lambda that does take parameters).
    // The lambda's own list lives in its `declarator` field; no field means no parameters.
    const TSNode parameterListOwner = isLambda
        ? childForField(candidate, "declarator")
        : functionDeclarator;

    FunctionModel model;
    model.name = name;
    model.kind = functionKind(candidate, declarator, name, source);
    model.range = sourceRangeForCandidate(candidate, path);
    model.parameters = parametersFor(parameterListOwner, source);
    const TSNode parameterList = findParameterList(parameterListOwner);
    model.parameterListRange = ts_node_is_null(parameterList)
        ? std::nullopt
        : std::optional<core::SourceRange>(sourceRange(parameterList, path));
    model.isStatic = hasStandaloneToken(specifiers, "static");
    model.isVariadic = isVariadic(parameterListOwner, source);

    const TSNode body = childForField(candidate, "body");

    if (!ts_node_is_null(body) && !subtreeContainsErrorOrMissing(body))
    {
        extractBodyStructure(body, source, path, model);
    }

    NormalizedTokens normalized = normalizedTokensFor(candidate, source);
    model.normalizedTokenSequence = std::move(normalized.sequence);
    model.normalizedTokenCount = normalized.count;

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

bool isFileOrNamespaceScopeDeclaration(TSNode candidate)
{
    for (TSNode parent = ts_node_parent(candidate); !ts_node_is_null(parent);
         parent = ts_node_parent(parent))
    {
        if (isNodeType(parent, "compound_statement") || isNodeType(parent, "class_specifier")
            || isNodeType(parent, "struct_specifier") || isNodeType(parent, "union_specifier"))
        {
            return false;
        }

        if (isNodeType(parent, "namespace_definition") || isNodeType(parent, "translation_unit"))
        {
            return true;
        }
    }

    return false;
}

bool isDirectDeclarator(TSNode node)
{
    return isNodeType(node, "identifier") || isNodeType(node, "type_identifier")
        || isNodeType(node, "qualified_identifier")
        || isNodeType(node, "init_declarator") || isNodeType(node, "pointer_declarator")
        || isNodeType(node, "reference_declarator") || isNodeType(node, "array_declarator")
        || isNodeType(node, "function_declarator") || isNodeType(node, "parenthesized_declarator")
        || isNodeType(node, "attributed_declarator");
}

TSNode declaratorNameNode(TSNode declarator)
{
    if (ts_node_is_null(declarator))
    {
        return TSNode{};
    }

    if (isNodeType(declarator, "qualified_identifier") || isNodeType(declarator, "identifier")
        || isNodeType(declarator, "field_identifier") || isNodeType(declarator, "type_identifier"))
    {
        return declarator;
    }

    const TSNode nestedDeclarator = childForField(declarator, "declarator");

    if (!ts_node_is_null(nestedDeclarator))
    {
        const TSNode nestedName = declaratorNameNode(nestedDeclarator);

        if (!ts_node_is_null(nestedName))
        {
            return nestedName;
        }
    }

    const TSNode identifier = findIdentifier(declarator);

    if (!ts_node_is_null(identifier))
    {
        return identifier;
    }

    return findDescendantOfType(declarator, "type_identifier");
}

TSNode typeDeclarator(TSNode declarator)
{
    if (!isNodeType(declarator, "init_declarator"))
    {
        return declarator;
    }

    const TSNode nested = childForField(declarator, "declarator");
    return ts_node_is_null(nested) ? declarator : nested;
}

std::vector<TSNode> declarationDeclarators(TSNode candidate)
{
    if (isNodeType(candidate, "for_range_loop"))
    {
        const TSNode declarator = childForField(candidate, "declarator");

        if (ts_node_is_null(declarator))
        {
            return {};
        }

        return {declarator};
    }

    std::vector<TSNode> declarators;
    const TSNode type = childForField(candidate, "type");
    const std::size_t typeEnd = ts_node_is_null(type) ? 0 : ts_node_end_byte(type);
    const uint32_t childCount = ts_node_named_child_count(candidate);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_named_child(candidate, index);

        if (ts_node_start_byte(child) >= typeEnd && isDirectDeclarator(child))
        {
            declarators.push_back(child);
        }
    }

    return declarators;
}

std::vector<TSNode> functionDeclarators(TSNode candidate)
{
    std::vector<TSNode> functions;

    for (const TSNode declarator : declarationDeclarators(candidate))
    {
        const TSNode function = isNodeType(declarator, "function_declarator")
            ? declarator
            : findFunctionDeclarator(declarator);

        if (!ts_node_is_null(function))
        {
            functions.push_back(function);
        }
    }

    return functions;
}

bool hasStorageClassSpecifier(TSNode candidate, std::string_view specifier, std::string_view source)
{
    const uint32_t childCount = ts_node_named_child_count(candidate);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_named_child(candidate, index);

        if (isNodeType(child, "storage_class_specifier")
            && trim(nodeText(child, source)) == specifier)
        {
            return true;
        }
    }

    return false;
}

std::string declarationType(
    TSNode candidate,
    TSNode declarator,
    std::string_view source)
{
    const TSNode type = childForField(candidate, "type");
    const TSNode typeNode = typeDeclarator(declarator);
    const TSNode name = declaratorNameNode(typeNode);
    const std::size_t typeEnd = ts_node_is_null(type) ? ts_node_start_byte(typeNode) : ts_node_end_byte(type);
    const std::size_t declaratorStart = ts_node_start_byte(typeNode);
    const std::size_t declaratorEnd = ts_node_end_byte(typeNode);
    std::size_t specifierStart = ts_node_start_byte(candidate);
    const uint32_t childCount = ts_node_named_child_count(candidate);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        const TSNode child = ts_node_named_child(candidate, index);

        if (ts_node_start_byte(child) >= typeEnd)
        {
            break;
        }

        if (!isNodeType(child, "storage_class_specifier"))
        {
            specifierStart = ts_node_start_byte(child);
            break;
        }

        specifierStart = ts_node_end_byte(child);
    }

    const std::size_t nameStart = ts_node_is_null(name)
        ? declaratorStart
        : ts_node_start_byte(name);
    const std::size_t nameEnd = ts_node_is_null(name) ? nameStart : ts_node_end_byte(name);
    const std::string base = specifierStart <= typeEnd && typeEnd <= source.size()
        ? trim(std::string(source.substr(specifierStart, typeEnd - specifierStart)))
        : std::string{};
    const std::string declaratorPrefix = declaratorStart <= nameStart && nameStart <= source.size()
        ? trim(std::string(source.substr(declaratorStart, nameStart - declaratorStart)))
        : std::string{};
    const std::string declaratorSuffix = nameEnd <= declaratorEnd && declaratorEnd <= source.size()
        ? trim(std::string(source.substr(nameEnd, declaratorEnd - nameEnd)))
        : std::string{};

    return base + declaratorPrefix + declaratorSuffix;
}

bool hasInitializer(TSNode declarator)
{
    return isNodeType(declarator, "init_declarator")
        && !ts_node_is_null(childForField(declarator, "value"));
}

bool isFunctionDeclaration(TSNode declarator)
{
    const TSNode function = isNodeType(declarator, "function_declarator")
        ? declarator
        : findFunctionDeclarator(declarator);
    return !ts_node_is_null(function) && !isFunctionPointerDeclaration(function);
}

bool subtreeContainsErrorOrMissing(TSNode node)
{
    if (ts_node_is_null(node))
    {
        return false;
    }

    if (ts_node_is_error(node) || ts_node_is_missing(node))
    {
        return true;
    }

    const uint32_t childCount = ts_node_child_count(node);

    for (uint32_t index = 0; index < childCount; ++index)
    {
        if (subtreeContainsErrorOrMissing(ts_node_child(node, index)))
        {
            return true;
        }
    }

    return false;
}

std::optional<ControlFlowKind> controlFlowKind(TSNode node)
{
    if (isNodeType(node, "if_statement"))
    {
        return ControlFlowKind::If;
    }

    if (isNodeType(node, "switch_statement"))
    {
        return ControlFlowKind::Switch;
    }

    if (isNodeType(node, "for_statement") || isNodeType(node, "for_range_loop"))
    {
        return ControlFlowKind::For;
    }

    if (isNodeType(node, "while_statement"))
    {
        return ControlFlowKind::While;
    }

    if (isNodeType(node, "do_statement"))
    {
        return ControlFlowKind::DoWhile;
    }

    if (isNodeType(node, "case_statement"))
    {
        return ts_node_is_null(childForField(node, "value"))
            ? ControlFlowKind::Default
            : ControlFlowKind::Case;
    }

    if (isNodeType(node, "default_statement"))
    {
        return ControlFlowKind::Default;
    }

    if (isNodeType(node, "conditional_expression"))
    {
        return ControlFlowKind::Conditional;
    }

    if (isNodeType(node, "return_statement"))
    {
        return ControlFlowKind::Return;
    }

    if (isNodeType(node, "break_statement"))
    {
        return ControlFlowKind::Break;
    }

    if (isNodeType(node, "continue_statement"))
    {
        return ControlFlowKind::Continue;
    }

    return std::nullopt;
}

bool isForStatement(TSNode node)
{
    return isNodeType(node, "for_statement") || isNodeType(node, "for_range_loop");
}

bool isStatementNode(TSNode node)
{
    if (isNodeType(node, "declaration") && isForStatement(ts_node_parent(node)))
    {
        // The for-loop's init clause ("int i = 0" in `for (int i = 0; ...)`)
        // is part of the for_statement itself, which already counts as one
        // statement; counting it again would break the "for counts once"
        // rule from the design.
        return false;
    }

    return isNodeType(node, "expression_statement") || isNodeType(node, "declaration")
        || isNodeType(node, "return_statement") || isNodeType(node, "if_statement")
        || isNodeType(node, "for_statement") || isNodeType(node, "for_range_loop")
        || isNodeType(node, "while_statement") || isNodeType(node, "do_statement")
        || isNodeType(node, "switch_statement") || isNodeType(node, "case_statement")
        || isNodeType(node, "default_statement") || isNodeType(node, "break_statement")
        || isNodeType(node, "continue_statement");
}

CallKind callKind(TSNode functionExpression)
{
    if (isNodeType(functionExpression, "identifier") || isNodeType(functionExpression, "qualified_identifier"))
    {
        return CallKind::Named;
    }

    if (isNodeType(functionExpression, "field_expression"))
    {
        return CallKind::Member;
    }

    return CallKind::Indirect;
}

std::size_t callArgumentCount(TSNode arguments)
{
    return ts_node_is_null(arguments) ? 0 : ts_node_named_child_count(arguments);
}

void extractBodyStructure(
    TSNode body,
    std::string_view source,
    const std::filesystem::path& path,
    FunctionModel& function)
{
    const auto visit = [&](const auto& self, TSNode node, std::size_t lexicalDepth, std::size_t flowDepth) -> void {
        if (isNodeType(node, "lambda_expression") || isNodeType(node, "function_definition"))
        {
            return;
        }

        if ((isNodeType(node, "declaration") || isNodeType(node, "for_range_loop"))
            && !subtreeContainsErrorOrMissing(node))
        {
            const TSNode parent = ts_node_parent(node);
            const bool isForRangeLoop = isNodeType(node, "for_range_loop");
            const std::size_t declarationDepth = (isForRangeLoop || isForStatement(parent))
                ? lexicalDepth + 1
                : lexicalDepth;

            for (const TSNode declarator : declarationDeclarators(node))
            {
                if (isFunctionDeclaration(declarator))
                {
                    continue;
                }

                const std::string name = declaratorName(declarator, source);

                if (name.empty())
                {
                    continue;
                }

                LocalVariableModel local;
                local.name = name;
                local.declaredType = declarationType(node, declarator, source);
                local.range = sourceRange(declarator, path);
                local.hasInitializer = hasInitializer(declarator);
                local.lexicalDepth = declarationDepth;
                function.localVariables.push_back(std::move(local));
            }
        }

        if (isNodeType(node, "call_expression") && !subtreeContainsErrorOrMissing(node))
        {
            const TSNode functionExpression = childForField(node, "function");
            const TSNode arguments = childForField(node, "arguments");

            if (!ts_node_is_null(functionExpression) && !ts_node_is_null(arguments))
            {
                CallModel call;
                call.calleeText = nodeText(functionExpression, source);
                call.kind = callKind(functionExpression);
                call.argumentCount = callArgumentCount(arguments);
                call.range = sourceRange(node, path);
                function.calls.push_back(std::move(call));
            }
        }

        const std::optional<ControlFlowKind> kind = controlFlowKind(node);
        const std::size_t childFlowDepth = kind.has_value() ? flowDepth + 1 : flowDepth;

        if (kind.has_value() && !subtreeContainsErrorOrMissing(node))
        {
            ControlFlowModel flow;
            flow.kind = *kind;
            flow.range = sourceRange(node, path);
            flow.nestingDepth = flowDepth;
            const TSNode condition = childForField(node, "condition");

            if (!ts_node_is_null(condition))
            {
                flow.conditionRange = sourceRange(condition, path);
            }

            function.controlFlow.push_back(std::move(flow));
        }

        if (isStatementNode(node) && !subtreeContainsErrorOrMissing(node))
        {
            function.statements.push_back(sourceRange(node, path));
        }

        const uint32_t childCount = ts_node_child_count(node);

        for (uint32_t index = 0; index < childCount; ++index)
        {
            const TSNode child = ts_node_child(node, index);
            const std::size_t childLexicalDepth = isNodeType(child, "compound_statement")
                ? lexicalDepth + 1
                : lexicalDepth;
            self(self, child, childLexicalDepth, childFlowDepth);
        }
    };

    visit(visit, body, 0, 0);

    std::stable_sort(
        function.localVariables.begin(), function.localVariables.end(),
        [](const LocalVariableModel& left, const LocalVariableModel& right) { return left.range < right.range; });
    std::stable_sort(
        function.controlFlow.begin(), function.controlFlow.end(),
        [](const ControlFlowModel& left, const ControlFlowModel& right) { return left.range < right.range; });
    std::stable_sort(
        function.calls.begin(), function.calls.end(),
        [](const CallModel& left, const CallModel& right) { return left.range < right.range; });
    std::stable_sort(function.statements.begin(), function.statements.end());
}

void adaptDeclaration(
    TSNode candidate,
    std::string_view source,
    const std::filesystem::path& path,
    ParsedFile& result)
{
    if ((ts_node_is_error(candidate) || ts_node_is_missing(candidate))
        || !isFileOrNamespaceScopeDeclaration(candidate))
    {
        return;
    }

    const bool isTypeDefinition = isNodeType(candidate, "type_definition");
    const bool isStatic = hasStorageClassSpecifier(candidate, "static", source);
    const bool isExtern = hasStorageClassSpecifier(candidate, "extern", source);

    for (const TSNode declarator : declarationDeclarators(candidate))
    {
        const std::string nameText = declaratorName(declarator, source);

        if (nameText.empty())
        {
            continue;
        }

        if (isTypeDefinition)
        {
            TypeAliasModel model;
            model.name = nameText;
            model.aliasedType = declarationType(candidate, declarator, source);
            model.range = sourceRange(declarator, path);
            result.typeAliases.push_back(std::move(model));
            continue;
        }

        if (isFunctionDeclaration(declarator))
        {
            continue;
        }

        GlobalVariableModel model;
        model.name = nameText;
        model.declaredType = declarationType(candidate, declarator, source);
        model.range = sourceRange(declarator, path);
        model.isStatic = isStatic;
        model.isExtern = isExtern;
        model.hasInitializer = hasInitializer(declarator);
        result.globalVariables.push_back(std::move(model));
    }
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
    return left.kind == right.kind && left.range == right.range;
}

bool hasSameClassRangeAndKind(const ClassModel& left, const ClassModel& right)
{
    return left.kind == right.kind && left.range == right.range;
}

bool hasSameIncludeRange(const IncludeDependency& left, const IncludeDependency& right)
{
    return left.range == right.range;
}

bool hasSameGlobalVariableRange(const GlobalVariableModel& left, const GlobalVariableModel& right)
{
    return left.range == right.range;
}

bool hasSameTypeAliasRange(const TypeAliasModel& left, const TypeAliasModel& right)
{
    return left.range == right.range;
}

} // namespace

class StructuralExtractor::Impl
{
public:
    Impl()
        : cFunctions_(compileQuery(tree_sitter_c(), "queries/c/functions.scm", embedded_queries::cFunctions))
        , cTypes_(compileQuery(tree_sitter_c(), "queries/c/types.scm", embedded_queries::cTypes))
        , cIncludes_(compileQuery(tree_sitter_c(), "queries/c/includes.scm", embedded_queries::cIncludes))
        , cDeclarations_(compileQuery(
              tree_sitter_c(), "queries/c/declarations.scm", embedded_queries::cDeclarations))
        , cppFunctions_(compileQuery(tree_sitter_cpp(), "queries/cpp/functions.scm", embedded_queries::cppFunctions))
        , cppTypes_(compileQuery(tree_sitter_cpp(), "queries/cpp/types.scm", embedded_queries::cppTypes))
        , cppIncludes_(compileQuery(tree_sitter_cpp(), "queries/cpp/includes.scm", embedded_queries::cppIncludes))
        , cppDeclarations_(compileQuery(
              tree_sitter_cpp(), "queries/cpp/declarations.scm", embedded_queries::cppDeclarations))
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

    [[nodiscard]] TSQuery* declarationsFor(Grammar grammar) const
    {
        return grammar == Grammar::C ? cDeclarations_.get() : cppDeclarations_.get();
    }

private:
    QueryPointer cFunctions_;
    QueryPointer cTypes_;
    QueryPointer cIncludes_;
    QueryPointer cDeclarations_;
    QueryPointer cppFunctions_;
    QueryPointer cppTypes_;
    QueryPointer cppIncludes_;
    QueryPointer cppDeclarations_;
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

    std::vector<TSNode> comments;
    collectNodesOfType(root, "comment", comments);

    for (const TSNode comment : comments)
    {
        result.comments.push_back(sourceRange(comment, path));
    }

    std::stable_sort(result.comments.begin(), result.comments.end());

    QueryCursorPointer cursor(ts_query_cursor_new());
    ts_query_cursor_exec(cursor.get(), impl_->functionsFor(grammar), root);

    TSQueryMatch match{};

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            const TSNode candidate = match.captures[index].node;

            if (isNodeType(candidate, "declaration"))
            {
                for (const TSNode functionDeclarator : functionDeclarators(candidate))
                {
                    if (!isFunctionPointerDeclaration(functionDeclarator))
                    {
                        result.functions.push_back(
                            adaptFunction(candidate, functionDeclarator, source, path));
                    }
                }

                continue;
            }

            const TSNode functionDeclarator = findFunctionDeclarator(candidate);

            // A malformed declarator (e.g. a missing parameter-list, `void f { }`) can leave
            // tree-sitter's error recovery with no function_declarator descendant at all; skip
            // it instead of handing a null TSNode to adaptFunction, which crashes.
            if (!ts_node_is_null(functionDeclarator))
            {
                result.functions.push_back(adaptFunction(candidate, functionDeclarator, source, path));
            }
        }
    }

    if (grammar == Grammar::Cpp)
    {
        std::vector<TSNode> lambdas;
        collectNodesOfType(root, "lambda_expression", lambdas);

        for (const TSNode lambda : lambdas)
        {
            result.functions.push_back(adaptFunction(lambda, lambda, source, path));
        }
    }

    std::sort(
        result.functions.begin(), result.functions.end(),
        [](const FunctionModel& left, const FunctionModel& right) { return left.range < right.range; });
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
        result.classes.begin(), result.classes.end(),
        [](const ClassModel& left, const ClassModel& right) { return left.range < right.range; });
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
        result.includes.begin(), result.includes.end(),
        [](const IncludeDependency& left, const IncludeDependency& right) { return left.range < right.range; });
    result.includes.erase(
        std::unique(result.includes.begin(), result.includes.end(), hasSameIncludeRange),
        result.includes.end());

    ts_query_cursor_exec(cursor.get(), impl_->declarationsFor(grammar), root);

    while (ts_query_cursor_next_match(cursor.get(), &match))
    {
        for (uint16_t index = 0; index < match.capture_count; ++index)
        {
            adaptDeclaration(match.captures[index].node, source, path, result);
        }
    }

    std::sort(
        result.globalVariables.begin(), result.globalVariables.end(),
        [](const GlobalVariableModel& left, const GlobalVariableModel& right) { return left.range < right.range; });
    result.globalVariables.erase(
        std::unique(
            result.globalVariables.begin(), result.globalVariables.end(), hasSameGlobalVariableRange),
        result.globalVariables.end());

    std::sort(
        result.typeAliases.begin(), result.typeAliases.end(),
        [](const TypeAliasModel& left, const TypeAliasModel& right) { return left.range < right.range; });
    result.typeAliases.erase(
        std::unique(result.typeAliases.begin(), result.typeAliases.end(), hasSameTypeAliasRange),
        result.typeAliases.end());
}

} // namespace codeqa::frontend
