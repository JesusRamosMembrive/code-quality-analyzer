#include "ReportModels.h"

#include "codeqa/config/DefaultConfig.h"
#include "codeqa/core/EnumJson.h"

#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace
{

const QStringList& orderedRuleIds()
{
    static const QStringList ids = {
        QStringLiteral("size.function-length"),
        QStringLiteral("size.local-variables"),
        QStringLiteral("signature.parameter-count"),
        QStringLiteral("signature.consecutive-booleans"),
        QStringLiteral("complexity.cyclomatic"),
        QStringLiteral("complexity.returns"),
        QStringLiteral("complexity.condition"),
        QStringLiteral("complexity.nesting")};
    return ids;
}

QString fromPath(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

QString severityText(const codeqa::core::DiagnosticSeverity severity)
{
    return QString::fromStdString(codeqa::core::diagnosticSeverityToJsonString(severity));
}

int severityRank(const codeqa::core::DiagnosticSeverity severity)
{
    switch (severity)
    {
    case codeqa::core::DiagnosticSeverity::Error:
        return 0;
    case codeqa::core::DiagnosticSeverity::Warning:
        return 1;
    case codeqa::core::DiagnosticSeverity::Information:
        return 2;
    }
    return 3;
}

QString stateText(const codeqa::model::AnalysisState state)
{
    switch (state)
    {
    case codeqa::model::AnalysisState::Complete:
        return QStringLiteral("complete");
    case codeqa::model::AnalysisState::Partial:
        return QStringLiteral("partial");
    case codeqa::model::AnalysisState::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QVariant optionalInteger(const std::optional<long long>& value)
{
    return value.has_value() ? QVariant::fromValue(*value) : QVariant{};
}

const codeqa::metrics::FunctionMetricsModel* findMetrics(
    const codeqa::application::ReportFile& file,
    const codeqa::rules::Finding& finding)
{
    const auto hasMatchingSymbol = [&finding](const auto& candidate)
    {
        return candidate.symbol.name == finding.symbolName
            || candidate.symbol.qualifiedName == finding.symbolName;
    };
    const auto containsFinding = [&finding](const auto& candidate)
    {
        const codeqa::core::SourceRange& range = candidate.symbol.range;
        const codeqa::core::SourceLocation& location = finding.location;
        if (location.line < range.begin.line || location.line > range.end.line)
        {
            return false;
        }
        if (location.line == range.begin.line && location.column < range.begin.column)
        {
            return false;
        }
        return location.line != range.end.line || location.column < range.end.column;
    };

    const auto exact = std::find_if(file.metrics.begin(), file.metrics.end(), [&](const auto& candidate)
    {
        return hasMatchingSymbol(candidate) && containsFinding(candidate);
    });
    if (exact != file.metrics.end())
    {
        return &*exact;
    }

    const auto byName = std::find_if(file.metrics.begin(), file.metrics.end(), hasMatchingSymbol);
    return byName == file.metrics.end() ? nullptr : &*byName;
}

QString metricsSummary(const codeqa::metrics::FunctionMetricsModel* metrics)
{
    if (metrics == nullptr)
    {
        return {};
    }

    return QStringLiteral("Cyclomatic: %1 | Nesting: %2 | Parameters: %3 | Code lines: %4")
        .arg(static_cast<qulonglong>(metrics->complexity.cyclomaticComplexity))
        .arg(static_cast<qulonglong>(metrics->complexity.maxNestingDepth))
        .arg(static_cast<qulonglong>(metrics->signature.parameterCount))
        .arg(static_cast<qulonglong>(metrics->size.codeLines));
}

QVariantMap locationMap(const codeqa::core::SourceLocation& location)
{
    return {
        {QStringLiteral("line"), static_cast<qulonglong>(location.line)},
        {QStringLiteral("column"), static_cast<qulonglong>(location.column)},
        {QStringLiteral("offset"), static_cast<qulonglong>(location.offset)}};
}

QVariantMap rangeMap(const codeqa::core::SourceRange& range)
{
    return {
        {QStringLiteral("begin"), locationMap(range.begin)},
        {QStringLiteral("end"), locationMap(range.end)}};
}

QVariantMap metricsMap(const codeqa::metrics::FunctionMetricsModel& metrics)
{
    return {
        {QStringLiteral("symbolName"), QString::fromStdString(metrics.symbol.name)},
        {QStringLiteral("qualifiedName"), QString::fromStdString(metrics.symbol.qualifiedName)},
        {QStringLiteral("line"), static_cast<qulonglong>(metrics.symbol.range.begin.line)},
        {QStringLiteral("sourceRange"), rangeMap(metrics.symbol.range)},
        {QStringLiteral("physicalLines"), static_cast<qulonglong>(metrics.size.physicalLines)},
        {QStringLiteral("codeLines"), static_cast<qulonglong>(metrics.size.codeLines)},
        {QStringLiteral("blankLines"), static_cast<qulonglong>(metrics.size.blankLines)},
        {QStringLiteral("commentLines"), static_cast<qulonglong>(metrics.size.commentLines)},
        {QStringLiteral("statementCount"), static_cast<qulonglong>(metrics.size.statementCount)},
        {QStringLiteral("parameterCount"), static_cast<qulonglong>(metrics.signature.parameterCount)},
        {QStringLiteral("booleanParameterCount"), static_cast<qulonglong>(metrics.signature.booleanParameterCount)},
        {QStringLiteral("signatureLength"), static_cast<qulonglong>(metrics.signature.signatureLength)},
        {QStringLiteral("cyclomaticComplexity"), static_cast<qulonglong>(metrics.complexity.cyclomaticComplexity)},
        {QStringLiteral("maxConditionComplexity"), static_cast<qulonglong>(metrics.complexity.maxConditionComplexity)},
        {QStringLiteral("maxConditionRange"), metrics.complexity.maxConditionRange.has_value()
             ? QVariant(rangeMap(*metrics.complexity.maxConditionRange))
             : QVariant{}},
        {QStringLiteral("maxNestingDepth"), static_cast<qulonglong>(metrics.complexity.maxNestingDepth)},
        {QStringLiteral("branchCount"), static_cast<qulonglong>(metrics.complexity.branchCount)},
        {QStringLiteral("returnCount"), static_cast<qulonglong>(metrics.complexity.returnCount)}};
}

} // namespace

RuleConfigModel::RuleConfigModel(QObject* parent) : QAbstractListModel(parent)
{
    setRules(codeqa::config::defaultConfig().rules);
}

int RuleConfigModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant RuleConfigModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case IdRole:
        return row.id;
    case EnabledRole:
        return row.config.enabled;
    case WarningLimitRole:
        return QVariant::fromValue(static_cast<qlonglong>(row.config.warningLimit));
    case HighLimitRole:
        return QVariant::fromValue(static_cast<qlonglong>(row.config.highLimit));
    default:
        return {};
    }
}

QHash<int, QByteArray> RuleConfigModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {IdRole, "id"},
        {EnabledRole, "enabled"},
        {WarningLimitRole, "warningLimit"},
        {HighLimitRole, "highLimit"}};
    return roles;
}

bool RuleConfigModel::setEnabled(const QString& ruleId, const bool enabled)
{
    const int rowIndex = findRow(ruleId);
    if (rowIndex < 0)
    {
        return false;
    }

    Row& row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.config.enabled == enabled)
    {
        return true;
    }

    row.config.enabled = enabled;
    const QModelIndex changed = index(rowIndex);
    emit dataChanged(changed, changed, {EnabledRole});
    emit configurationChanged();
    return true;
}

bool RuleConfigModel::setLimit(
    const QString& ruleId, const qlonglong warningLimit, const qlonglong highLimit)
{
    const int rowIndex = findRow(ruleId);
    if (rowIndex < 0)
    {
        return false;
    }

    Row& row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.config.warningLimit == warningLimit && row.config.highLimit == highLimit)
    {
        return true;
    }

    row.config.warningLimit = warningLimit;
    row.config.highLimit = highLimit;
    const QModelIndex changed = index(rowIndex);
    emit dataChanged(changed, changed, {WarningLimitRole, HighLimitRole});
    emit configurationChanged();
    return true;
}

void RuleConfigModel::setRules(const std::map<std::string, codeqa::config::RuleConfig>& rules)
{
    const std::map<std::string, codeqa::config::RuleConfig> defaults =
        codeqa::config::defaultConfig().rules;
    beginResetModel();
    rows_.clear();
    rows_.reserve(static_cast<std::size_t>(orderedRuleIds().size()));
    for (const QString& id : orderedRuleIds())
    {
        const std::string key = id.toStdString();
        const auto configured = rules.find(key);
        const auto fallback = defaults.find(key);
        rows_.push_back(Row{
            id,
            configured != rules.end() ? configured->second : fallback->second});
    }
    endResetModel();
}

std::map<std::string, codeqa::config::RuleConfig> RuleConfigModel::rules() const
{
    std::map<std::string, codeqa::config::RuleConfig> result;
    for (const Row& row : rows_)
    {
        result.emplace(row.id.toStdString(), row.config);
    }
    return result;
}

int RuleConfigModel::findRow(const QString& ruleId) const
{
    const QString normalized = ruleId.trimmed();
    const auto match = std::find_if(rows_.begin(), rows_.end(), [&normalized](const Row& row)
    {
        return row.id == normalized;
    });
    return match == rows_.end() ? -1 : static_cast<int>(std::distance(rows_.begin(), match));
}

HeaderOverrideModel::HeaderOverrideModel(QObject* parent) : QAbstractListModel(parent) {}

int HeaderOverrideModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(overrides_.size());
}

QVariant HeaderOverrideModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const codeqa::config::HeaderOverrideConfig& overrideConfig =
        overrides_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case PatternRole:
        return QString::fromStdString(overrideConfig.pattern);
    case LanguageRole:
        return QString::fromStdString(codeqa::core::languageToJsonString(overrideConfig.language));
    default:
        return {};
    }
}

QHash<int, QByteArray> HeaderOverrideModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {{PatternRole, "pattern"}, {LanguageRole, "language"}};
    return roles;
}

void HeaderOverrideModel::setOverrides(
    std::vector<codeqa::config::HeaderOverrideConfig> overrides)
{
    beginResetModel();
    overrides_ = std::move(overrides);
    endResetModel();
}

const std::vector<codeqa::config::HeaderOverrideConfig>& HeaderOverrideModel::overrides() const
{
    return overrides_;
}

void HeaderOverrideModel::addOverride(codeqa::config::HeaderOverrideConfig overrideConfig)
{
    const int row = rowCount();
    beginInsertRows({}, row, row);
    overrides_.push_back(std::move(overrideConfig));
    endInsertRows();
    emit configurationChanged();
}

bool HeaderOverrideModel::updateOverride(
    const int row, codeqa::config::HeaderOverrideConfig overrideConfig)
{
    if (row < 0 || row >= rowCount())
    {
        return false;
    }

    codeqa::config::HeaderOverrideConfig& current = overrides_[static_cast<std::size_t>(row)];
    if (current.pattern == overrideConfig.pattern && current.language == overrideConfig.language)
    {
        return true;
    }

    current = std::move(overrideConfig);
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {PatternRole, LanguageRole});
    emit configurationChanged();
    return true;
}

bool HeaderOverrideModel::removeOverride(const int row)
{
    if (row < 0 || row >= rowCount())
    {
        return false;
    }

    beginRemoveRows({}, row, row);
    overrides_.erase(overrides_.begin() + row);
    endRemoveRows();
    emit configurationChanged();
    return true;
}

FindingListModel::FindingListModel(QObject* parent) : QAbstractListModel(parent) {}

int FindingListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int FindingListModel::findingCount() const
{
    return static_cast<int>(rows_.size());
}

int FindingListModel::errorCount() const
{
    return errorCount_;
}

int FindingListModel::warningCount() const
{
    return warningCount_;
}

QVariant FindingListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    const codeqa::rules::Finding& finding = *row.finding;
    switch (role)
    {
    case SeverityRole:
        return severityText(finding.severity);
    case RuleIdRole:
        return QString::fromStdString(finding.ruleId);
    case SymbolNameRole:
        return QString::fromStdString(finding.symbolName);
    case PathRole:
        return fromPath(row.file->path);
    case LineRole:
        return static_cast<qulonglong>(finding.location.line);
    case ColumnRole:
        return static_cast<qulonglong>(finding.location.column);
    case ObservedValueRole:
        return optionalInteger(finding.observedValue);
    case LimitValueRole:
        return optionalInteger(finding.limitValue);
    case DescriptionRole:
        return QString::fromStdString(finding.description);
    case RecommendationRole:
        return QString::fromStdString(finding.recommendation);
    case MetricSummaryRole:
        return metricsSummary(row.metrics);
    default:
        return {};
    }
}

QHash<int, QByteArray> FindingListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {SeverityRole, "severity"},
        {RuleIdRole, "ruleId"},
        {SymbolNameRole, "symbolName"},
        {PathRole, "path"},
        {LineRole, "line"},
        {ColumnRole, "column"},
        {ObservedValueRole, "observedValue"},
        {LimitValueRole, "limitValue"},
        {DescriptionRole, "description"},
        {RecommendationRole, "recommendation"},
        {MetricSummaryRole, "metricSummary"}};
    return roles;
}

void FindingListModel::beginDocumentReset()
{
    beginResetModel();
}

void FindingListModel::applyDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    document_ = std::move(document);
    rebuildRows();
}

void FindingListModel::endDocumentReset()
{
    endResetModel();
    emit summaryChanged();
}

void FindingListModel::setSeverityFilter(const QString& severity)
{
    const QString normalized = severity.trimmed().toLower();
    if (severityFilter_ == normalized)
    {
        return;
    }
    severityFilter_ = normalized;
    rebuild();
}

void FindingListModel::setRuleFilter(const QString& ruleId)
{
    const QString normalized = ruleId.trimmed();
    if (ruleFilter_ == normalized)
    {
        return;
    }
    ruleFilter_ = normalized;
    rebuild();
}

void FindingListModel::setSearchText(const QString& searchText)
{
    const QString normalized = searchText.trimmed();
    if (searchText_ == normalized)
    {
        return;
    }
    searchText_ = normalized;
    rebuild();
}

QString FindingListModel::severityFilter() const
{
    return severityFilter_;
}

QString FindingListModel::ruleFilter() const
{
    return ruleFilter_;
}

QString FindingListModel::searchText() const
{
    return searchText_;
}

void FindingListModel::rebuild()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
    emit summaryChanged();
}

void FindingListModel::rebuildRows()
{
    rows_.clear();
    errorCount_ = 0;
    warningCount_ = 0;

    if (document_)
    {
        for (const codeqa::application::ReportFile& file : document_->files)
        {
            const QString path = fromPath(file.path);
            for (const codeqa::rules::Finding& finding : file.findings)
            {
                if (!severityFilter_.isEmpty() && severityText(finding.severity) != severityFilter_)
                {
                    continue;
                }
                if (!ruleFilter_.isEmpty()
                    && QString::fromStdString(finding.ruleId).compare(ruleFilter_, Qt::CaseInsensitive) != 0)
                {
                    continue;
                }
                if (!searchText_.isEmpty()
                    && !QString::fromStdString(finding.ruleId).contains(searchText_, Qt::CaseInsensitive)
                    && !QString::fromStdString(finding.symbolName).contains(searchText_, Qt::CaseInsensitive)
                    && !path.contains(searchText_, Qt::CaseInsensitive))
                {
                    continue;
                }
                rows_.push_back(Row{&file, &finding, findMetrics(file, finding), path});
                if (finding.severity == codeqa::core::DiagnosticSeverity::Error)
                {
                    ++errorCount_;
                }
                else if (finding.severity == codeqa::core::DiagnosticSeverity::Warning)
                {
                    ++warningCount_;
                }
            }
        }

        std::stable_sort(rows_.begin(), rows_.end(), [](const Row& left, const Row& right)
        {
            const int leftSeverity = severityRank(left.finding->severity);
            const int rightSeverity = severityRank(right.finding->severity);
            if (leftSeverity != rightSeverity)
            {
                return leftSeverity < rightSeverity;
            }

            const int pathOrder = QString::compare(left.path, right.path, Qt::CaseInsensitive);
            if (pathOrder != 0)
            {
                return pathOrder < 0;
            }
            if (left.finding->location.line != right.finding->location.line)
            {
                return left.finding->location.line < right.finding->location.line;
            }
            return left.finding->location.column < right.finding->location.column;
        });
    }

}

SuppressedFindingListModel::SuppressedFindingListModel(QObject* parent) : QAbstractListModel(parent) {}

int SuppressedFindingListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant SuppressedFindingListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    const codeqa::rules::Finding& finding = *row.finding;
    switch (role)
    {
    case RuleIdRole:
        return QString::fromStdString(finding.ruleId);
    case SymbolNameRole:
        return QString::fromStdString(finding.symbolName);
    case PathRole:
        return row.path;
    case LineRole:
        return static_cast<qulonglong>(finding.location.line);
    case ColumnRole:
        return static_cast<qulonglong>(finding.location.column);
    default:
        return {};
    }
}

QHash<int, QByteArray> SuppressedFindingListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {RuleIdRole, "ruleId"},
        {SymbolNameRole, "symbolName"},
        {PathRole, "path"},
        {LineRole, "line"},
        {ColumnRole, "column"}};
    return roles;
}

void SuppressedFindingListModel::beginDocumentReset()
{
    beginResetModel();
}

void SuppressedFindingListModel::applyDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    document_ = std::move(document);
    rebuildRows();
}

void SuppressedFindingListModel::endDocumentReset()
{
    endResetModel();
}

void SuppressedFindingListModel::rebuildRows()
{
    rows_.clear();

    if (document_)
    {
        for (const codeqa::application::ReportFile& file : document_->files)
        {
            const QString path = fromPath(file.path);
            for (const codeqa::rules::Finding& finding : file.suppressedFindings)
            {
                rows_.push_back(Row{&file, &finding, path});
            }
        }

        std::stable_sort(rows_.begin(), rows_.end(), [](const Row& left, const Row& right)
        {
            const int pathOrder = QString::compare(left.path, right.path, Qt::CaseInsensitive);
            if (pathOrder != 0)
            {
                return pathOrder < 0;
            }
            if (left.finding->location.line != right.finding->location.line)
            {
                return left.finding->location.line < right.finding->location.line;
            }
            return left.finding->location.column < right.finding->location.column;
        });
    }
}

FileListModel::FileListModel(QObject* parent) : QAbstractListModel(parent) {}

int FileListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() || !document_ ? 0 : static_cast<int>(document_->files.size());
}

int FileListModel::fileCount() const
{
    return document_ ? static_cast<int>(document_->files.size()) : 0;
}

int FileListModel::diagnosticCount() const
{
    return diagnosticCount_;
}

QVariant FileListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const codeqa::application::ReportFile& file = document_->files[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case PathRole:
        return fromPath(file.path);
    case LanguageRole:
        return QString::fromStdString(codeqa::core::languageToJsonString(file.language));
    case StateRole:
        return stateText(file.state);
    case DiagnosticCountRole:
        return static_cast<qulonglong>(file.diagnostics.size());
    case FindingCountRole:
        return static_cast<qulonglong>(file.findings.size());
    case FunctionCountRole:
        return static_cast<qulonglong>(file.metrics.size());
    case FunctionMetricsRole:
        return functionMetricsCache_[static_cast<std::size_t>(index.row())];
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {PathRole, "path"},
        {LanguageRole, "language"},
        {StateRole, "fileState"},
        {DiagnosticCountRole, "diagnosticCount"},
        {FindingCountRole, "findingCount"},
        {FunctionCountRole, "functionCount"},
        {FunctionMetricsRole, "functionMetrics"}};
    return roles;
}

void FileListModel::beginDocumentReset()
{
    beginResetModel();
}

void FileListModel::applyDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    document_ = std::move(document);
    diagnosticCount_ = 0;
    functionMetricsCache_.clear();
    if (document_)
    {
        functionMetricsCache_.reserve(document_->files.size());
        for (const codeqa::application::ReportFile& file : document_->files)
        {
            diagnosticCount_ += static_cast<int>(file.diagnostics.size());

            QVariantList functions;
            functions.reserve(static_cast<qsizetype>(file.metrics.size()));
            for (const codeqa::metrics::FunctionMetricsModel& metrics : file.metrics)
            {
                functions.push_back(metricsMap(metrics));
            }
            functionMetricsCache_.push_back(std::move(functions));
        }
    }
}

void FileListModel::endDocumentReset()
{
    endResetModel();
    emit summaryChanged();
}

SnapshotListModel::SnapshotListModel(QObject* parent) : QAbstractListModel(parent) {}

int SnapshotListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(snapshots_.size());
}

QVariant SnapshotListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const codeqa::application::SnapshotEntry& entry = snapshots_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case TimestampRole:
        return QString::fromStdString(entry.timestamp);
    case PathRole:
        return fromPath(entry.path);
    default:
        return {};
    }
}

QHash<int, QByteArray> SnapshotListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {{TimestampRole, "timestamp"}, {PathRole, "path"}};
    return roles;
}

void SnapshotListModel::setSnapshots(std::vector<codeqa::application::SnapshotEntry> snapshots)
{
    beginSnapshotsReset();
    applySnapshots(std::move(snapshots));
    endSnapshotsReset();
}

void SnapshotListModel::beginSnapshotsReset()
{
    beginResetModel();
}

void SnapshotListModel::applySnapshots(std::vector<codeqa::application::SnapshotEntry> snapshots)
{
    std::stable_sort(snapshots.begin(), snapshots.end(), [](const auto& left, const auto& right)
    {
        return left.timestamp > right.timestamp;
    });
    snapshots_ = std::move(snapshots);
}

void SnapshotListModel::endSnapshotsReset()
{
    endResetModel();
}
