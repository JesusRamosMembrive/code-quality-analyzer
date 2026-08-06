#pragma once

#include "codeqa/application/DetectFileLanguage.h"
#include "codeqa/infrastructure/DiscoveryOptions.h"
#include "codeqa/model/Models.h"

#include <vector>

namespace codeqa::application
{

struct AnalyzeProjectRequest
{
    codeqa::infrastructure::DiscoveryOptions discovery;
    std::vector<LanguageOverride> languageOverrides;
};

class AnalyzeProject
{
public:
    [[nodiscard]] codeqa::model::ProjectModel execute(const AnalyzeProjectRequest& request) const;
};

} // namespace codeqa::application
