#pragma once

#include "codeqa/core/Language.h"
#include "codeqa/metrics/MetricsModels.h"
#include "codeqa/model/Models.h"
#include "codeqa/rules/RuleModels.h"

#include <string>
#include <vector>

namespace codeqa::rules
{

struct RuleContext
{
    const codeqa::metrics::FunctionMetricsModel& metrics;
    const codeqa::model::CallableModel& callable;
};

class Rule
{
public:
    virtual ~Rule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual bool appliesTo(codeqa::core::Language language) const = 0;
    [[nodiscard]] virtual std::vector<Finding> evaluate(const RuleContext& context) const = 0;
};

} // namespace codeqa::rules
