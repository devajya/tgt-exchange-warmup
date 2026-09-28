#include <memory>

#include <drogon/drogon.h>

#include "core/Exchange.h"
#include "server/OrdersController.h"

int main()
{
    exchange::Exchange exchange;

    auto orders = std::make_shared<api::OrdersController>(exchange);

    drogon::app().registerController(orders);
    drogon::app().addListener("0.0.0.0", 8080).setThreadNum(4).run();
}
