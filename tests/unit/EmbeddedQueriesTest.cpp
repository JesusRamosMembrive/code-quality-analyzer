#include "doctest/doctest.h"
#include "codeqa/frontend/StructuralExtractor.h"

TEST_CASE("all embedded structural queries compile")
{
    CHECK_NOTHROW(codeqa::frontend::StructuralExtractor{});
}
