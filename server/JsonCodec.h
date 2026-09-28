#pragma once

#include <string>

#include <drogon/HttpResponse.h>
#include <json/value.h>

#include "core/Exchange.h"

namespace api {

const char *toString(exchange::Side side);

Json::Value toJson(const exchange::Trade &trade);
Json::Value toJson(const exchange::BookSnapshot &book);

Json::Value tradeMessage(const exchange::TradeEvent &event);
Json::Value bookMessage(const exchange::BookEvent &event);
std::string toText(const Json::Value &value);

drogon::HttpResponsePtr jsonResponse(const Json::Value &body,
                                     drogon::HttpStatusCode code);
drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode code,
                                      const std::string &message);

}
