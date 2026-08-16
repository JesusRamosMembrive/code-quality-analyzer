#pragma once

#include "codeqa/config/ConfigModels.h"
#include "codeqa/duplication/DuplicationModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::duplication
{

[[nodiscard]] ProjectDuplicationModel detectDuplicates(
    const codeqa::model::ProjectModel& project, const codeqa::config::DuplicationConfig& config);

} // namespace codeqa::duplication
