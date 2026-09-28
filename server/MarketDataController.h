#pragma once

#include <string>

#include <drogon/PubSubService.h>
#include <drogon/WebSocketController.h>

#include "core/Exchange.h"

namespace api {

class MarketDataController
    : public drogon::WebSocketController<MarketDataController, false>
{
  public:
    explicit MarketDataController(exchange::Exchange &exchange);

    exchange::MarketDataListener listener();

    void handleNewConnection(const drogon::HttpRequestPtr &request,
                             const drogon::WebSocketConnectionPtr &connection) override;
    void handleNewMessage(const drogon::WebSocketConnectionPtr &connection,
                          std::string &&message,
                          const drogon::WebSocketMessageType &type) override;
    void
    handleConnectionClosed(const drogon::WebSocketConnectionPtr &connection) override;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/marketdata", drogon::Get);
    WS_PATH_LIST_END

  private:
    exchange::Exchange *exchange_;
    drogon::PubSubService<std::string> feed_;
};

}
