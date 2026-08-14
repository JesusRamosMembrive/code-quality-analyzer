#pragma once

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/application/ReportDocument.h"
#include "codeqa/config/ConfigModels.h"

#include <QAbstractListModel>
#include <QString>

#include <memory>
#include <map>
#include <vector>

class AppController;

class RuleConfigModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        EnabledRole,
        WarningLimitRole,
        HighLimitRole
    };
    Q_ENUM(Role)

    explicit RuleConfigModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    bool setEnabled(const QString& ruleId, bool enabled);
    bool setLimit(const QString& ruleId, qlonglong warningLimit, qlonglong highLimit);
    void setRules(const std::map<std::string, codeqa::config::RuleConfig>& rules);
    [[nodiscard]] std::map<std::string, codeqa::config::RuleConfig> rules() const;

signals:
    void configurationChanged();

private:
    struct Row
    {
        QString id;
        codeqa::config::RuleConfig config;
    };

    [[nodiscard]] int findRow(const QString& ruleId) const;

    std::vector<Row> rows_;
};

class HeaderOverrideModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        PatternRole = Qt::UserRole + 1,
        LanguageRole
    };
    Q_ENUM(Role)

    explicit HeaderOverrideModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setOverrides(std::vector<codeqa::config::HeaderOverrideConfig> overrides);
    [[nodiscard]] const std::vector<codeqa::config::HeaderOverrideConfig>& overrides() const;
    void addOverride(codeqa::config::HeaderOverrideConfig overrideConfig);
    [[nodiscard]] bool updateOverride(int row, codeqa::config::HeaderOverrideConfig overrideConfig);
    [[nodiscard]] bool removeOverride(int row);

signals:
    void configurationChanged();

private:
    std::vector<codeqa::config::HeaderOverrideConfig> overrides_;
};

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
    friend class AppController;

    struct Row
    {
        const codeqa::application::ReportFile* file{nullptr};
        const codeqa::rules::Finding* finding{nullptr};
        const codeqa::metrics::FunctionMetricsModel* metrics{nullptr};
    };

    void rebuild();
    void rebuildRows();
    void beginDocumentReset();
    void applyDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void endDocumentReset();

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
    friend class AppController;

    void beginDocumentReset();
    void applyDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void endDocumentReset();

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
    friend class AppController;

    void beginSnapshotsReset();
    void applySnapshots(std::vector<codeqa::application::SnapshotEntry> snapshots);
    void endSnapshotsReset();

    std::vector<codeqa::application::SnapshotEntry> snapshots_;
};
