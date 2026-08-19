#pragma once

#include "codeqa/architecture/ArchitectureModels.h"
#include "codeqa/config/ConfigModels.h"
#include "codeqa/model/Models.h"

namespace codeqa::architecture
{

[[nodiscard]] ProjectArchitectureModel analyzeArchitecture(
    const codeqa::model::ProjectModel& project, const codeqa::config::ArchitectureConfig& config);

} // namespace codeqa::architecture
