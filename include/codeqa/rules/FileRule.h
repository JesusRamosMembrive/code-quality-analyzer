#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <string>
#include <vector>

namespace codeqa::rules
{

struct FileRuleContext
{
    const codeqa::model::FileModel& file;
};

class FileRule
{
public:
    virtual ~FileRule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const FileRuleContext& context) const = 0;
};

} // namespace codeqa::rules
