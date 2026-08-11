#pragma once

#include "codeqa/model/Models.h"

#include <nlohmann/json.hpp>

namespace codeqa::report
{

[[nodiscard]] nlohmann::json fileJson(const codeqa::model::FileModel& file);

} // namespace codeqa::report
