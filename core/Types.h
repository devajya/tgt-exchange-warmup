#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace exchange {

using Price = std::int64_t;
using Qty = std::int64_t;
using OrderId = std::uint64_t;

enum class Side { Buy, Sell };

struct Order
{
    OrderId id;
    Side side;
    Price price;
    Qty qty;
};

struct Trade
{
    OrderId restingId;
    OrderId incomingId;
    Side incomingSide;
    Price price;
    Qty qty;
};

struct Level
{
    Price price;
    Qty qty;
};

struct BookSnapshot
{
    std::vector<Level> bids;
    std::vector<Level> asks;
};

enum class RejectReason { NonPositivePrice, NonPositiveQty };

struct SubmitResult
{
    OrderId orderId = 0;
    std::optional<RejectReason> rejected;
    std::vector<Trade> trades;
    Qty remainingQty = 0;
};

enum class CancelResult { Cancelled, NotFound };

}
