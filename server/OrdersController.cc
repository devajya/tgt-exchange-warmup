#include "server/OrdersController.h"

#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "server/JsonCodec.h"

namespace api {

struct ParsedOrder
{
    exchange::Side side{};
    exchange::Price price{};
    exchange::Qty qty{};
};

static std::optional<exchange::Side> parseSide(const Json::Value &value)
{
    if (value.isString())
    {
        if (value.asString() == "buy")
            return exchange::Side::Buy;
        if (value.asString() == "sell")
            return exchange::Side::Sell;
    }
    return std::nullopt;
}

static std::optional<std::int64_t> parseInteger(const Json::Value &value)
{
    const bool integerType =
        value.type() == Json::intValue || value.type() == Json::uintValue;
    if (!integerType || !value.isInt64())
        return std::nullopt;
    return value.asInt64();
}

static std::variant<ParsedOrder, std::string> parseOrder(const Json::Value *json)
{
    if (json == nullptr || !json->isObject())
        return "body must be a JSON object";

    const auto side = parseSide((*json)["side"]);
    if (!side)
        return R"(side must be "buy" or "sell")";

    const auto price = parseInteger((*json)["price"]);
    if (!price)
        return "price must be an integer";

    const auto quantity = parseInteger((*json)["quantity"]);
    if (!quantity)
        return "quantity must be an integer";

    return ParsedOrder{.side = *side, .price = *price, .qty = *quantity};
}

static std::optional<exchange::OrderId> parseOrderId(const std::string &text)
{
    exchange::OrderId id = 0;
    const auto *begin = std::to_address(text.begin());
    const auto *end = std::to_address(text.end());
    const auto [ptr, ec] = std::from_chars(begin, end, id);
    if (ec != std::errc{} || ptr != end || id == 0)
        return std::nullopt;
    return id;
}

static const char *rejectMessage(exchange::RejectReason reason)
{
    return reason == exchange::RejectReason::NonPositivePrice
               ? "price must be positive"
               : "quantity must be positive";
}

static const char *fillStatus(const exchange::SubmitResult &result)
{
    if (result.remainingQty == 0)
        return "filled";
    return result.trades.empty() ? "resting" : "partially_filled";
}

OrdersController::OrdersController(exchange::Exchange &exchange) : exchange_(&exchange) {}

void OrdersController::submit(const drogon::HttpRequestPtr &request, Callback &&callback)
{
    const auto json = request->getJsonObject();
    const auto parsed = parseOrder(json.get());
    if (const auto *error = std::get_if<std::string>(&parsed))
    {
        callback(errorResponse(drogon::k400BadRequest, *error));
        return;
    }

    const auto &order = std::get<ParsedOrder>(parsed);
    const auto result = exchange_->submit(order.side, order.price, order.qty);
    if (result.rejected)
    {
        callback(errorResponse(drogon::k400BadRequest, rejectMessage(*result.rejected)));
        return;
    }

    exchange::Qty filled = 0;
    Json::Value trades(Json::arrayValue);
    for (const auto &trade : result.trades)
    {
        filled += trade.qty;
        trades.append(toJson(trade));
    }

    Json::Value body;
    body["orderId"] = Json::UInt64{result.orderId};
    body["status"] = fillStatus(result);
    body["filledQuantity"] = Json::Int64{filled};
    body["remainingQuantity"] = Json::Int64{result.remainingQty};
    body["trades"] = trades;
    callback(jsonResponse(body, drogon::k201Created));
}

void OrdersController::cancel([[maybe_unused]] const drogon::HttpRequestPtr &request,
                              Callback &&callback, const std::string &id)
{
    const auto orderId = parseOrderId(id);
    if (!orderId)
    {
        callback(
            errorResponse(drogon::k400BadRequest, "order id must be a positive integer"));
        return;
    }

    if (exchange_->cancel(*orderId) == exchange::CancelResult::NotFound)
    {
        callback(errorResponse(drogon::k404NotFound, "order not found"));
        return;
    }

    Json::Value body;
    body["orderId"] = Json::UInt64{*orderId};
    body["status"] = "cancelled";
    callback(jsonResponse(body, drogon::k200OK));
}

void OrdersController::book([[maybe_unused]] const drogon::HttpRequestPtr &request,
                            Callback &&callback)
{
    const auto snapshot = exchange_->snapshot();
    Json::Value body = toJson(snapshot.book);
    body["seq"] = Json::UInt64{snapshot.seq};
    callback(jsonResponse(body, drogon::k200OK));
}

}
