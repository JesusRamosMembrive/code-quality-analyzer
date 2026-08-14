#include "ReportModels.h"

#include "codeqa/core/EnumJson.h"

#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace
{

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

FindingListModel::FindingListModel(QObject* parent) : QAbstractListModel(parent) {}

int FindingListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
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
    return {
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
}

void FindingListModel::setDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    beginDocumentReset();
    applyDocument(std::move(document));
    endDocumentReset();
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
}

void FindingListModel::rebuildRows()
{
    rows_.clear();

    if (document_)
    {
        for (const codeqa::application::ReportFile& file : document_->files)
        {
            const QString path = fromPath(file.path);
            for (const codeqa::rules::Finding& finding : file.findings)
            {
                const QString severity = severityText(finding.severity);
                const QString ruleId = QString::fromStdString(finding.ruleId);
                const QString symbolName = QString::fromStdString(finding.symbolName);
                if (!severityFilter_.isEmpty() && severity != severityFilter_)
                {
                    continue;
                }
                if (!ruleFilter_.isEmpty() && ruleId.compare(ruleFilter_, Qt::CaseInsensitive) != 0)
                {
                    continue;
                }
                if (!searchText_.isEmpty()
                    && !ruleId.contains(searchText_, Qt::CaseInsensitive)
                    && !symbolName.contains(searchText_, Qt::CaseInsensitive)
                    && !path.contains(searchText_, Qt::CaseInsensitive))
                {
                    continue;
                }
                rows_.push_back(Row{&file, &finding, findMetrics(file, finding)});
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

            const std::string leftPath = left.file->path.generic_string();
            const std::string rightPath = right.file->path.generic_string();
            if (leftPath != rightPath)
            {
                return leftPath < rightPath;
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
    {
        QVariantList functions;
        functions.reserve(static_cast<qsizetype>(file.metrics.size()));
        for (const codeqa::metrics::FunctionMetricsModel& metrics : file.metrics)
        {
            functions.push_back(metricsMap(metrics));
        }
        return functions;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {LanguageRole, "language"},
        {StateRole, "state"},
        {DiagnosticCountRole, "diagnosticCount"},
        {FindingCountRole, "findingCount"},
        {FunctionCountRole, "functionCount"},
        {FunctionMetricsRole, "functionMetrics"}};
}

void FileListModel::setDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    beginDocumentReset();
    applyDocument(std::move(document));
    endDocumentReset();
}

void FileListModel::beginDocumentReset()
{
    beginResetModel();
}

void FileListModel::applyDocument(
    std::shared_ptr<const codeqa::application::ReportDocument> document)
{
    document_ = std::move(document);
}

void FileListModel::endDocumentReset()
{
    endResetModel();
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
    return {{TimestampRole, "timestamp"}, {PathRole, "path"}};
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
