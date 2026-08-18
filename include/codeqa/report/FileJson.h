#pragma once

#include "codeqa/core/SourceLocation.h"
#include "codeqa/model/Models.h"

#include <nlohmann/json.hpp>

namespace codeqa::report
{

[[nodiscard]] nlohmann::json locationJson(const codeqa::core::SourceLocation& location);

[[nodiscard]] nlohmann::json fileJson(const codeqa::model::FileModel& file);

} // namespace codeqa::report
