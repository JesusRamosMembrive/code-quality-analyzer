#pragma once

#include "codeqa/application/ApplicationInfo.h"

namespace codeqa::application
{

class GetApplicationInfo
{
public:
    [[nodiscard]] ApplicationInfo execute() const;
};

} // namespace codeqa::application
