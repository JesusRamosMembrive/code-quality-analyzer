#include "doctest/doctest.h"

#include "codeqa/application/GetApplicationInfo.h"

TEST_CASE("GetApplicationInfo returns the expected name and version")
{
    const codeqa::application::GetApplicationInfo useCase;

    const codeqa::application::ApplicationInfo info = useCase.execute();

    CHECK(info.name == "Code Quality Analyzer");
    CHECK(info.version == "0.1.0");
}
