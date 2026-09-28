#include "server/JsonCodec.h"

#include <json/writer.h>

namespace api {

const char *toString(exchange::Side side)
{
    return side == exchange::Side::Buy ? "buy" : "sell";
}

static Json::Value toJson(const std::vector<exchange::Level> &levels)
{
    Json::Value out(Json::arrayValue);
    for (const auto &level : levels)
    {
        Json::Value entry;
        entry["price"] = Json::Int64{level.price};
        entry["quantity"] = Json::Int64{level.qty};
        out.append(entry);
    }
    return out;
}

Json::Value toJson(const exchange::Trade &trade)
{
    Json::Value out;
    out["restingOrderId"] = Json::UInt64{trade.restingId};
    out["incomingOrderId"] = Json::UInt64{trade.incomingId};
    out["side"] = toString(trade.incomingSide);
    out["price"] = Json::Int64{trade.price};
    out["quantity"] = Json::Int64{trade.qty};
    return out;
}

Json::Value toJson(const exchange::BookSnapshot &book)
{
    Json::Value out;
    out["bids"] = toJson(book.bids);
    out["asks"] = toJson(book.asks);
    return out;
}

Json::Value tradeMessage(const exchange::TradeEvent &event)
{
    Json::Value out = toJson(event.trade);
    out["type"] = "trade";
    out["seq"] = Json::UInt64{event.seq};
    return out;
}

Json::Value bookMessage(const exchange::BookEvent &event)
{
    Json::Value out = toJson(event.book);
    out["type"] = "book";
    out["seq"] = Json::UInt64{event.seq};
    return out;
}

std::string toText(const Json::Value &value)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

drogon::HttpResponsePtr jsonResponse(const Json::Value &body, drogon::HttpStatusCode code)
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(code);
    return response;
}

drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode code,
                                      const std::string &message)
{
    Json::Value body;
    body["error"] = message;
    return jsonResponse(body, code);
}

}
