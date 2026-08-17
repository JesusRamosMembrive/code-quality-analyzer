#pragma once

#include "codeqa/application/ApplicationFacade.h"
#include "codeqa/application/ReportDocument.h"
#include "codeqa/config/ConfigModels.h"

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>

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
        HighLimitRole,
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
        LanguageRole,
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
    Q_PROPERTY(int findingCount READ findingCount NOTIFY summaryChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY summaryChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY summaryChanged)

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
        MetricSummaryRole,
    };
    Q_ENUM(Role)

    explicit FindingListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int findingCount() const;
    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int warningCount() const;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void setSeverityFilter(const QString& severity);
    void setRuleFilter(const QString& ruleId);
    void setSearchText(const QString& searchText);

    [[nodiscard]] QString severityFilter() const;
    [[nodiscard]] QString ruleFilter() const;
    [[nodiscard]] QString searchText() const;

signals:
    void summaryChanged();

private:
    friend class AppController;

    // file/finding/metrics point directly into document_'s vectors. Safe only
    // because every reassignment of document_ (applyDocument()) is always
    // immediately followed by rebuildRows() in the same call, so rows_ never
    // outlives the document it was built from. If a future change ever
    // separates those two steps, these pointers would dangle silently.
    struct Row
    {
        const codeqa::application::ReportFile* file{nullptr};
        const codeqa::rules::Finding* finding{nullptr};
        const codeqa::metrics::FunctionMetricsModel* metrics{nullptr};
        QString path;
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
    int errorCount_{0};
    int warningCount_{0};
};

class SuppressedFindingListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        RuleIdRole = Qt::UserRole + 1,
        SymbolNameRole,
        PathRole,
        LineRole,
        ColumnRole,
    };
    Q_ENUM(Role)

    explicit SuppressedFindingListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);

private:
    friend class AppController;

    // Row points directly into document_'s vectors -- same safety caveat as
    // FindingListModel::Row: only safe because every reassignment of
    // document_ (applyDocument()) is always immediately followed by
    // rebuildRows() in the same call.
    struct Row
    {
        const codeqa::application::ReportFile* file{nullptr};
        const codeqa::rules::Finding* finding{nullptr};
        QString path;
    };

    void rebuildRows();
    void beginDocumentReset();
    void applyDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void endDocumentReset();

    std::shared_ptr<const codeqa::application::ReportDocument> document_;
    std::vector<Row> rows_;
};

class FileListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int fileCount READ fileCount NOTIFY summaryChanged)
    Q_PROPERTY(int diagnosticCount READ diagnosticCount NOTIFY summaryChanged)

public:
    enum Role
    {
        PathRole = Qt::UserRole + 1,
        LanguageRole,
        StateRole,
        DiagnosticCountRole,
        FindingCountRole,
        FunctionCountRole,
        FunctionMetricsRole,
    };
    Q_ENUM(Role)

    explicit FileListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int fileCount() const;
    [[nodiscard]] int diagnosticCount() const;

    void setDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);

signals:
    void summaryChanged();

private:
    friend class AppController;

    void beginDocumentReset();
    void applyDocument(std::shared_ptr<const codeqa::application::ReportDocument> document);
    void endDocumentReset();

    std::shared_ptr<const codeqa::application::ReportDocument> document_;
    std::vector<QVariantList> functionMetricsCache_;
    int diagnosticCount_{0};
};

class SnapshotListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        TimestampRole = Qt::UserRole + 1,
        PathRole,
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
