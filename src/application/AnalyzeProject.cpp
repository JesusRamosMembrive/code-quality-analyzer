#include "codeqa/application/AnalyzeProject.h"

#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/application/DiscoverProjectSources.h"
#include "codeqa/frontend/ParseSourceFile.h"
#include "codeqa/frontend/SelectGrammar.h"
#include "codeqa/frontend/StructuralExtractor.h"
#include "codeqa/frontend/TreeSitterParser.h"
#include "codeqa/model/ProjectModelBuilder.h"

#include <utility>
#include <vector>

namespace codeqa::application
{

codeqa::model::ProjectModel AnalyzeProject::execute(const AnalyzeProjectRequest& request) const
{
    const DiscoverProjectSources discoverProjectSources;
    const codeqa::infrastructure::DiscoveryResult discovery =
        discoverProjectSources.execute(request.discovery);
    const DetectFileLanguage detectFileLanguage;
    codeqa::frontend::TreeSitterParser parser;
    codeqa::frontend::StructuralExtractor extractor;
    std::vector<codeqa::frontend::ParsedFile> parsedFiles;
    parsedFiles.reserve(discovery.files.size() + discovery.diagnostics.size());

    for (const codeqa::infrastructure::DiscoveredFile& file : discovery.files)
    {
        const codeqa::core::LanguageDetectionResult detection =
            detectFileLanguage.execute(file, request.languageOverrides);
        const codeqa::frontend::GrammarSelection grammar = codeqa::frontend::selectGrammar(detection.language);

        codeqa::frontend::ParsedFile parsedFile = codeqa::frontend::parseSourceFile(
            request.discovery.rootPath / file.path,
            grammar.grammar,
            grammar.reason,
            parser,
            extractor);
        parsedFile.language = detection.language;
        parsedFiles.push_back(std::move(parsedFile));
    }

    for (const codeqa::core::Diagnostic& diagnostic : discovery.diagnostics)
    {
        codeqa::frontend::ParsedFile unavailable;
        unavailable.path = diagnostic.location.filePath;
        unavailable.grammarReason = "source discovery failed";
        unavailable.diagnostics.push_back(diagnostic);
        parsedFiles.push_back(std::move(unavailable));
    }

    const codeqa::model::ProjectModelBuilder builder;
    return builder.build(request.discovery.rootPath, parsedFiles);
}

} // namespace codeqa::application
