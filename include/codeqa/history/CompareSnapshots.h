#pragma once

#include "codeqa/history/SnapshotComparison.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <tuple>

namespace codeqa::history
{

/// (ruleId, symbolName, file path relative to rootPath). No line/column: moving unrelated
/// code shouldn't make a still-live finding look resolved+new.
using FindingKey = std::tuple<std::string, std::string, std::string>;
using FindingKeySet = std::set<FindingKey>;

/// For the just-computed current run (findings still in memory). Relativizes each
/// FileFindingsModel::path against findings.rootPath (ProjectFindingsModel already carries
/// it -- ApplyRules::execute sets it from ProjectModel::rootPath -- no separate rootPath
/// parameter needed).
[[nodiscard]] FindingKeySet extractFindingKeys(const codeqa::rules::ProjectFindingsModel& findings);

/// For a previous run loaded back from a snapshot file. Paths inside snapshotReport are
/// already relative (toReportJson relativizes them at save time). Throws
/// nlohmann::json::exception if the document doesn't have the expected files[].findings[]
/// shape -- the caller (runAnalyze) treats that the same as an unreadable snapshot.
[[nodiscard]] FindingKeySet extractFindingKeys(const nlohmann::json& snapshotReport);

[[nodiscard]] SnapshotComparison compareSnapshots(const FindingKeySet& current, const FindingKeySet& previous);

} // namespace codeqa::history
