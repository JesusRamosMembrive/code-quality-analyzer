#pragma once

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/application/ReportDocument.h"

#include <QAbstractListModel>
#include <QString>

#include <memory>
#include <vector>

class FindingListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        SeverityRole = Qt::UserRole + 1,
        RuleIdRole,
        SymbolNameRole,
        PathRole,
        LineRole,
        ColumnRole,
        ObservedValueRole,
        LimitValueRole,
        DescriptionRole,
        RecommendationRole,
        MetricSummaryRole
    };
    Q_ENUM(Role)

    explicit FindingListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void setSeverityFilter(const QString& severity);
    void setRuleFilter(const QString& ruleId);
    void setSearchText(const QString& searchText);

    [[nodiscard]] QString severityFilter() const;
    [[nodiscard]] QString ruleFilter() const;
    [[nodiscard]] QString searchText() const;

private:
    struct Row
    {
        const codeqa::application::ReportFile* file{nullptr};
        const codeqa::rules::Finding* finding{nullptr};
        const codeqa::metrics::FunctionMetricsModel* metrics{nullptr};
    };

    void rebuild();
    void rebuildRows();

    std::shared_ptr<const codeqa::application::ReportDocument> document_;
    std::vector<Row> rows_;
    QString severityFilter_;
    QString ruleFilter_;
    QString searchText_;
};

class FileListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        PathRole = Qt::UserRole + 1,
        LanguageRole,
        StateRole,
        DiagnosticCountRole,
        FindingCountRole,
        FunctionCountRole,
        FunctionMetricsRole
    };
    Q_ENUM(Role)

    explicit FileListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);

private:
    std::shared_ptr<const codeqa::application::ReportDocument> document_;
};

class SnapshotListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        TimestampRole = Qt::UserRole + 1,
        PathRole
    };
    Q_ENUM(Role)

    explicit SnapshotListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setSnapshots(std::vector<codeqa::application::SnapshotEntry> snapshots);

private:
    std::vector<codeqa::application::SnapshotEntry> snapshots_;
};
