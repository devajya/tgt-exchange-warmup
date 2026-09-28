#pragma once

#include <functional>
#include <string>

#include <drogon/HttpController.h>

#include "core/Exchange.h"

namespace api {

class OrdersController : public drogon::HttpController<OrdersController, false>
{
  public:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    explicit OrdersController(exchange::Exchange &exchange);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(OrdersController::submit, "/orders", drogon::Post);
    ADD_METHOD_TO(OrdersController::cancel, "/orders/{id}", drogon::Delete);
    ADD_METHOD_TO(OrdersController::book, "/book", drogon::Get);
    METHOD_LIST_END

    void submit(const drogon::HttpRequestPtr &request, Callback &&callback);
    void cancel(const drogon::HttpRequestPtr &request, Callback &&callback,
                const std::string &id);
    void book(const drogon::HttpRequestPtr &request, Callback &&callback);

  private:
    exchange::Exchange *exchange_;
};

}
