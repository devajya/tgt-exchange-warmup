#include "server/MarketDataController.h"

#include <memory>

#include "server/JsonCodec.h"

namespace api {

static const std::string kTopic = "marketdata";

MarketDataController::MarketDataController(exchange::Exchange &exchange)
    : exchange_(&exchange)
{}

exchange::MarketDataListener MarketDataController::listener()
{
    return {
        .onTrade =
            [this](const exchange::TradeEvent &event) {
                feed_.publish(kTopic, toText(tradeMessage(event)));
            },
        .onBook =
            [this](const exchange::BookEvent &event) {
                feed_.publish(kTopic, toText(bookMessage(event)));
            },
    };
}

void MarketDataController::handleNewConnection(
    [[maybe_unused]] const drogon::HttpRequestPtr &request,
    const drogon::WebSocketConnectionPtr &connection)
{
    const auto id = feed_.subscribe(
        kTopic, [connection]([[maybe_unused]] const std::string &topic,
                             const std::string &message) { connection->send(message); });
    connection->setContext(std::make_shared<drogon::SubscriberID>(id));
    connection->send(toText(bookMessage(exchange_->snapshot())));
}

void MarketDataController::handleNewMessage(
    [[maybe_unused]] const drogon::WebSocketConnectionPtr &connection,
    [[maybe_unused]] std::string &&message,
    [[maybe_unused]] const drogon::WebSocketMessageType &type)
{}

void MarketDataController::handleConnectionClosed(
    const drogon::WebSocketConnectionPtr &connection)
{
    if (const auto id = connection->getContext<drogon::SubscriberID>())
        feed_.unsubscribe(kTopic, *id);
}

}
