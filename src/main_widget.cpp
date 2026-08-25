#include "main_widget.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSize>
#include <QStringList>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr auto ApiBaseUrl = "https://bestchange.app/v2/";

QString numberString(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        auto result = QString::number(value.toDouble(), 'f', 8);
        while (result.endsWith('0')) {
            result.chop(1);
        }
        if (result.endsWith('.')) {
            result.chop(1);
        }
        return result;
    }
    return {};
}

double numberValue(const QJsonValue& value)
{
    return value.isString() ? value.toString().toDouble() : value.toDouble();
}
} // namespace

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent)
    , network_(new QNetworkAccessManager(this))
{
    setWindowTitle(QStringLiteral("Best Exchange"));
    resize(QSize{1150, 700});

    auto* description = new QLabel(
        tr("Best offer for every Tether USDT → RUB direction from the official "
           "BestChange API."),
        this);
    description->setWordWrap(true);

    apiKeyEdit_ = new QLineEdit(this);
    apiKeyEdit_->setEchoMode(QLineEdit::Password);
    apiKeyEdit_->setPlaceholderText(tr("BestChange partner API key"));
    apiKeyEdit_->setClearButtonEnabled(true);

    refreshButton_ = new QPushButton(tr("Load best rates"), this);
    connect(refreshButton_, &QPushButton::clicked, this, [this] { refreshRates(); });
    connect(apiKeyEdit_, &QLineEdit::returnPressed, this, [this] { refreshRates(); });

    auto* keyLayout = new QHBoxLayout;
    keyLayout->addWidget(apiKeyEdit_, 1);
    keyLayout->addWidget(refreshButton_);

    statusLabel_ = new QLabel(tr("Enter an API key to load rates."), this);
    statusLabel_->setWordWrap(true);

    ratesTable_ = new QTableWidget(this);
    ratesTable_->setColumnCount(9);
    ratesTable_->setHorizontalHeaderLabels(
        {tr("Tether network"), tr("Receive RUB via"), tr("Exchanger"),
         tr("Rate, RUB/USDT"), tr("Ranking rate"), tr("Reserve, RUB"),
         tr("Minimum, USDT"), tr("Maximum, USDT"), tr("Conditions")});
    ratesTable_->setAlternatingRowColors(true);
    ratesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ratesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ratesTable_->setSortingEnabled(true);
    ratesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ratesTable_->horizontalHeader()->setStretchLastSection(true);
    ratesTable_->verticalHeader()->setVisible(false);
    connect(ratesTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
                const auto url = ratesTable_->item(row, 2)->data(Qt::UserRole).toUrl();
                if (url.isValid()) {
                    QDesktopServices::openUrl(url);
                }
            });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(description);
    layout->addLayout(keyLayout);
    layout->addWidget(statusLabel_);
    layout->addWidget(ratesTable_, 1);
}

void MainWidget::refreshRates()
{
    apiKey_ = apiKeyEdit_->text().trimmed();
    if (apiKey_.isEmpty()) {
        showError(tr("Enter your BestChange API key."));
        return;
    }

    ratesTable_->setRowCount(0);
    sourceCurrencies_.clear();
    targetCurrencies_.clear();
    changers_.clear();
    pairs_.clear();
    setLoading(true, tr("Loading currency directory…"));
    loadCurrencies();
}

void MainWidget::loadCurrencies()
{
    getJson(QStringLiteral("currencies/ru"), [this](const QJsonObject& root) {
        const auto currencies = root.value(QStringLiteral("currencies")).toArray();
        for (const auto& value : currencies) {
            const auto object = value.toObject();
            Currency currency{object.value(QStringLiteral("id")).toInt(),
                              object.value(QStringLiteral("name")).toString(),
                              object.value(QStringLiteral("code")).toString()};

            if (currency.code.compare(QStringLiteral("USDT"), Qt::CaseInsensitive) == 0
                && currency.name.startsWith(QStringLiteral("Tether"),
                                            Qt::CaseInsensitive)) {
                sourceCurrencies_.append(currency);
            }
            if (currency.code.compare(QStringLiteral("RUB"), Qt::CaseInsensitive) == 0) {
                targetCurrencies_.append(currency);
            }
        }

        if (sourceCurrencies_.isEmpty() || targetCurrencies_.isEmpty()) {
            showError(tr("The API returned no Tether USDT or RUB currencies."));
            return;
        }

        setLoading(true, tr("Loading exchanger directory…"));
        loadChangers();
    });
}

void MainWidget::loadChangers()
{
    getJson(QStringLiteral("changers/ru"), [this](const QJsonObject& root) {
        const auto changers = root.value(QStringLiteral("changers")).toArray();
        for (const auto& value : changers) {
            const auto object = value.toObject();
            const int id = object.value(QStringLiteral("id")).toInt();
            const auto urls = object.value(QStringLiteral("urls")).toObject();
            const auto url = urls.value(QStringLiteral("ru")).toString(
                urls.value(QStringLiteral("en")).toString());
            changers_.insert(id,
                             {object.value(QStringLiteral("name")).toString(), url,
                              object.value(QStringLiteral("active")).toBool()});
        }

        setLoading(true, tr("Loading all USDT → RUB directions…"));
        loadRates();
    });
}

void MainWidget::loadRates()
{
    QStringList pairIds;
    for (const auto& source : sourceCurrencies_) {
        for (const auto& target : targetCurrencies_) {
            const auto pairId = QStringLiteral("%1-%2").arg(source.id).arg(target.id);
            pairIds.append(pairId);
            pairs_.insert(pairId, {source, target});
        }
    }

    if (pairIds.size() > 500) {
        showError(tr("BestChange returned %1 directions; the API batch limit is 500.")
                      .arg(pairIds.size()));
        return;
    }

    getJson(QStringLiteral("rates/%1").arg(pairIds.join('+')),
            [this](const QJsonObject& root) {
                const auto rates = root.value(QStringLiteral("rates")).toObject();
                ratesTable_->setSortingEnabled(false);
                ratesTable_->setRowCount(0);

                for (auto iterator = rates.constBegin(); iterator != rates.constEnd();
                     ++iterator) {
                    if (!pairs_.contains(iterator.key())) {
                        continue;
                    }

                    QJsonObject bestOffer;
                    double bestRankingRate = -1.0;
                    for (const auto& value : iterator.value().toArray()) {
                        const auto offer = value.toObject();
                        const int changerId = offer.value(QStringLiteral("changer")).toInt();
                        const auto changer = changers_.value(changerId);
                        if (!changer.active) {
                            continue;
                        }

                        const double rankingRate =
                            numberValue(offer.value(QStringLiteral("rankrate")));
                        if (rankingRate > bestRankingRate) {
                            bestRankingRate = rankingRate;
                            bestOffer = offer;
                        }
                    }

                    if (bestOffer.isEmpty()) {
                        continue;
                    }

                    const auto pair = pairs_.value(iterator.key());
                    const int changerId =
                        bestOffer.value(QStringLiteral("changer")).toInt();
                    const auto changer = changers_.value(changerId);
                    const int row = ratesTable_->rowCount();
                    ratesTable_->insertRow(row);

                    ratesTable_->setItem(row, 0, new QTableWidgetItem(pair.source.name));
                    ratesTable_->setItem(row, 1, new QTableWidgetItem(pair.target.name));

                    auto* changerItem = new QTableWidgetItem(changer.name);
                    changerItem->setData(Qt::UserRole, QUrl{changer.url});
                    changerItem->setToolTip(tr("Double-click to open this exchanger"));
                    ratesTable_->setItem(row, 2, changerItem);

                    auto* rateItem = new QTableWidgetItem;
                    rateItem->setData(Qt::DisplayRole,
                                      numberValue(bestOffer.value(QStringLiteral("rate"))));
                    ratesTable_->setItem(row, 3, rateItem);

                    auto* rankingItem = new QTableWidgetItem;
                    rankingItem->setData(Qt::DisplayRole, bestRankingRate);
                    ratesTable_->setItem(row, 4, rankingItem);

                    auto addNumber = [this, row, &bestOffer](int column, const char* key) {
                        const auto value = numberString(
                            bestOffer.value(QString::fromLatin1(key)));
                        auto* item = new QTableWidgetItem;
                        item->setData(Qt::DisplayRole, value.toDouble());
                        ratesTable_->setItem(row, column, item);
                    };
                    addNumber(5, "reserve");
                    addNumber(6, "inmin");
                    addNumber(7, "inmax");

                    QStringList marks;
                    for (const auto& mark :
                         bestOffer.value(QStringLiteral("marks")).toArray()) {
                        marks.append(mark.toString());
                    }
                    ratesTable_->setItem(row, 8,
                                         new QTableWidgetItem(marks.join(QStringLiteral(", "))));
                }

                ratesTable_->setSortingEnabled(true);
                ratesTable_->sortItems(4, Qt::DescendingOrder);
                setLoading(false,
                           tr("Found %1 directions with offers. Best offer from each "
                              "direction is shown; ranking rate includes commissions for "
                              "BestChange's reference amount.")
                               .arg(ratesTable_->rowCount()));
            });
}

void MainWidget::getJson(
    const QString& endpoint, const std::function<void(const QJsonObject&)>& onSuccess)
{
    const auto encodedKey = QUrl::toPercentEncoding(apiKey_);
    const auto encodedUrl = QByteArray{ApiBaseUrl} + encodedKey + '/'
        + endpoint.toUtf8();
    QNetworkRequest request{QUrl::fromEncoded(encodedUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("BestExchange/0.1"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Accept-Encoding", "gzip");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess] {
        const auto data = reply->readAll();
        const auto status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const auto errorText = reply->errorString();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            showError(tr("BestChange request failed (HTTP %1): %2")
                          .arg(status)
                          .arg(errorText));
            return;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            showError(tr("BestChange returned invalid JSON: %1")
                          .arg(parseError.errorString()));
            return;
        }

        onSuccess(document.object());
    });
}

void MainWidget::showError(const QString& message)
{
    setLoading(false, message);
    statusLabel_->setStyleSheet(QStringLiteral("color: #b00020;"));
}

void MainWidget::setLoading(bool loading, const QString& message)
{
    apiKeyEdit_->setEnabled(!loading);
    refreshButton_->setEnabled(!loading);
    statusLabel_->setText(message);
    if (loading || !message.isEmpty()) {
        statusLabel_->setStyleSheet(QString{});
    }
}
