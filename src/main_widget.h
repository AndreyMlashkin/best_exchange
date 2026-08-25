#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QTableWidget;

class MainWidget final : public QWidget
{
public:
    explicit MainWidget(QWidget* parent = nullptr);

private:
    struct Currency
    {
        int id{};
        QString name;
        QString code;
    };

    struct Changer
    {
        QString name;
        QString url;
        bool active{};
    };

    struct Pair
    {
        Currency source;
        Currency target;
    };

    void refreshRates();
    void loadCurrencies();
    void loadChangers();
    void loadRates();
    void getJson(const QString& endpoint,
                 const std::function<void(const QJsonObject&)>& onSuccess);
    void showError(const QString& message);
    void setLoading(bool loading, const QString& message = {});

    QNetworkAccessManager* network_{};
    QLineEdit* apiKeyEdit_{};
    QPushButton* refreshButton_{};
    QLabel* statusLabel_{};
    QTableWidget* ratesTable_{};

    QString apiKey_;
    QList<Currency> sourceCurrencies_;
    QList<Currency> targetCurrencies_;
    QHash<int, Changer> changers_;
    QHash<QString, Pair> pairs_;
};
