#include "codeqa/frontend/StructuralExtractor.h"

#include "codeqa/frontend/ParsedFile.h"
#include "codeqa/frontend/TreeSitterParser.h"
#include "codeqa/frontend/EmbeddedQueries.h"

#include <tree_sitter/api.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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
    static_cast<void>(tree);
    static_cast<void>(grammar);
    static_cast<void>(source);
    static_cast<void>(path);
    static_cast<void>(result);
}

} // namespace codeqa::frontend
