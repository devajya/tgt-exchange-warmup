#include "core/OrderBook.h"

#include <algorithm>

namespace exchange {

static bool crosses(Side incomingSide, Price incomingPrice, Price restingPrice)
{
    return incomingSide == Side::Buy ? incomingPrice >= restingPrice
                                     : incomingPrice <= restingPrice;
}

template <typename Levels>
static std::vector<Level> topLevels(const Levels &levels, std::size_t depth)
{
    std::vector<Level> out;
    for (auto it = levels.begin(); it != levels.end() && out.size() < depth; ++it)
    {
        Qty total = 0;
        for (const auto &order : it->second)
            total += order.qty;
        out.push_back({it->first, total});
    }
    return out;
}

template <typename Levels>
void OrderBook::match(Order &incoming, Levels &opposite, SubmitResult &result)
{
    while (incoming.qty > 0 && !opposite.empty())
    {
        auto level = opposite.begin();
        if (!crosses(incoming.side, incoming.price, level->first))
            break;

        auto &queue = level->second;
        while (incoming.qty > 0 && !queue.empty())
        {
            Order &resting = queue.front();
            const Qty qty = std::min(incoming.qty, resting.qty);
            result.trades.push_back(
                {resting.id, incoming.id, incoming.side, resting.price, qty});
            incoming.qty -= qty;
            resting.qty -= qty;
            if (resting.qty == 0)
            {
                index_.erase(resting.id);
                queue.pop_front();
            }
        }

        if (queue.empty())
            opposite.erase(level);
    }
}

template <typename Levels>
void OrderBook::rest(const Order &order, Levels &own)
{
    auto &queue = own[order.price];
    auto it = queue.insert(queue.end(), order);
    index_.emplace(order.id, Locator{order.side, order.price, it});
}

template <typename Levels>
void OrderBook::erase(Levels &levels, const Locator &loc)
{
    auto level = levels.find(loc.price);
    level->second.erase(loc.it);
    if (level->second.empty())
        levels.erase(level);
}

SubmitResult OrderBook::submit(Order order)
{
    SubmitResult result;
    if (order.price <= 0)
    {
        result.rejected = RejectReason::NonPositivePrice;
        return result;
    }
    if (order.qty <= 0)
    {
        result.rejected = RejectReason::NonPositiveQty;
        return result;
    }
    result.orderId = order.id;

    if (order.side == Side::Buy)
    {
        match(order, asks_, result);
        if (order.qty > 0)
            rest(order, bids_);
    }
    else
    {
        match(order, bids_, result);
        if (order.qty > 0)
            rest(order, asks_);
    }

    result.remainingQty = order.qty;
    return result;
}

CancelResult OrderBook::cancel(OrderId id)
{
    auto found = index_.find(id);
    if (found == index_.end())
        return CancelResult::NotFound;

    const Locator loc = found->second;
    index_.erase(found);
    if (loc.side == Side::Buy)
        erase(bids_, loc);
    else
        erase(asks_, loc);
    return CancelResult::Cancelled;
}

BookSnapshot OrderBook::snapshot(std::size_t depth) const
{
    return {.bids = topLevels(bids_, depth), .asks = topLevels(asks_, depth)};
}

}
