#pragma once

#include "codeqa/core/DiagnosticSeverity.h"
#include "codeqa/history/SnapshotComparison.h"
#include "codeqa/rules/RuleModels.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <tuple>

namespace codeqa::history
{

/// (ruleId, symbolName, file path relative to rootPath). No line/column: moving unrelated
/// code shouldn't make a still-live finding look resolved+new.
using FindingKey = std::tuple<std::string, std::string, std::string>;

/// Maps each distinct FindingKey to how many findings share it. A plain set would collapse
/// e.g. two C++ overloads or two lambdas (both named "<lambda>") that violate the same rule
/// in the same file into one key -- the map preserves that multiplicity so compareSnapshots
/// can report the correct new/existing/resolved counts instead of undercounting.
using FindingKeySet = std::map<FindingKey, std::size_t>;

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

/// True if any finding in `current` is "new" relative to `previousKeys` (the same multiset-diff
/// identity compareSnapshots uses: a finding is new once its FindingKey has no remaining
/// previousKeys occurrence left to consume) and its severity is >= minSeverity. An empty
/// previousKeys represents "no previous snapshot" -- every finding is then new, which is exactly
/// absolute mode; no separate branch is needed for it.
[[nodiscard]] bool hasNewFindingAtOrAboveSeverity(
    const codeqa::rules::ProjectFindingsModel& current,
    const FindingKeySet& previousKeys,
    codeqa::core::DiagnosticSeverity minSeverity);

} // namespace codeqa::history
