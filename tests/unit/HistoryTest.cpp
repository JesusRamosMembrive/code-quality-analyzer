#include "doctest/doctest.h"

#include "codeqa/history/CompareSnapshots.h"

#include <filesystem>

TEST_CASE("extractFindingKeys from a ProjectFindingsModel relativizes each file's path against rootPath")
{
    codeqa::rules::ProjectFindingsModel findings;
    findings.rootPath = std::filesystem::path{"project"};

    codeqa::rules::FileFindingsModel fileFindings;
    fileFindings.path = findings.rootPath / "src" / "foo.cpp";
    codeqa::rules::Finding finding;
    finding.ruleId = "complexity.cyclomatic";
    finding.symbolName = "Foo::bar";
    fileFindings.findings.push_back(finding);
    findings.files.push_back(fileFindings);

    const codeqa::history::FindingKeySet keys = codeqa::history::extractFindingKeys(findings);

    REQUIRE(keys.size() == 1);
    CHECK(*keys.begin() == codeqa::history::FindingKey{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"});
}

TEST_CASE("extractFindingKeys from JSON reads ruleId/symbolName/path from a toReportJson-shaped document")
{
    const nlohmann::json snapshot = nlohmann::json::parse(R"({
        "files": [
            {
                "path": "src/foo.cpp",
                "findings": [
                    { "ruleId": "complexity.cyclomatic", "symbolName": "Foo::bar" }
                ]
            }
        ]
    })");

    const codeqa::history::FindingKeySet keys = codeqa::history::extractFindingKeys(snapshot);

    REQUIRE(keys.size() == 1);
    CHECK(*keys.begin() == codeqa::history::FindingKey{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"});
}

TEST_CASE("extractFindingKeys from JSON throws on a snapshot missing the files array")
{
    const nlohmann::json malformed = nlohmann::json::object();

    auto callExtractFindingKeys = [&]() {
        (void)codeqa::history::extractFindingKeys(malformed);
    };
    CHECK_THROWS_AS(callExtractFindingKeys(), nlohmann::json::exception);
}

TEST_CASE(
    "compareSnapshots classifies keys only in current as new, only in previous as resolved, "
    "and shared keys as existing")
{
    const codeqa::history::FindingKey shared{"complexity.cyclomatic", "Foo::bar", "src/foo.cpp"};
    const codeqa::history::FindingKey added{"complexity.returns", "Foo::baz", "src/foo.cpp"};
    const codeqa::history::FindingKey removed{"signature.parameter-count", "Foo::qux", "src/foo.cpp"};

    const codeqa::history::FindingKeySet current{shared, added};
    const codeqa::history::FindingKeySet previous{shared, removed};

    const codeqa::history::SnapshotComparison comparison = codeqa::history::compareSnapshots(current, previous);

    CHECK(comparison.newCount == 1);
    CHECK(comparison.existingCount == 1);
    CHECK(comparison.resolvedCount == 1);
}

TEST_CASE("compareSnapshots on two empty sets reports all zero counts")
{
    const codeqa::history::SnapshotComparison comparison =
        codeqa::history::compareSnapshots(codeqa::history::FindingKeySet{}, codeqa::history::FindingKeySet{});

    CHECK(comparison.newCount == 0);
    CHECK(comparison.existingCount == 0);
    CHECK(comparison.resolvedCount == 0);
}
