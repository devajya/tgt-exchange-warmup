#include "core/Exchange.h"

#include <utility>

namespace exchange {

void Exchange::setListener(MarketDataListener listener)
{
    listener_ = std::move(listener);
}

SubmitResult Exchange::submit(Side side, Price price, Qty qty)
{
    SubmitResult result;
    std::vector<TradeEvent> trades;
    BookEvent book{};
    {
        const std::scoped_lock lock(mutex_);
        result = book_.submit({.id = nextId_, .side = side, .price = price, .qty = qty});
        if (result.rejected)
            return result;

        ++nextId_;
        trades.reserve(result.trades.size());
        for (const auto &trade : result.trades)
            trades.push_back({.seq = ++seq_, .trade = trade});
        book = {.seq = ++seq_, .book = book_.snapshot(kBookDepth)};
    }
    publish(trades, book);
    return result;
}

CancelResult Exchange::cancel(OrderId id)
{
    BookEvent book{};
    {
        const std::scoped_lock lock(mutex_);
        if (book_.cancel(id) == CancelResult::NotFound)
            return CancelResult::NotFound;

        book = {.seq = ++seq_, .book = book_.snapshot(kBookDepth)};
    }
    publish({}, book);
    return CancelResult::Cancelled;
}

BookEvent Exchange::snapshot() const
{
    const std::scoped_lock lock(mutex_);
    return {.seq = seq_, .book = book_.snapshot(kBookDepth)};
}

void Exchange::publish(const std::vector<TradeEvent> &trades, const BookEvent &book) const
{
    if (listener_.onTrade)
    {
        for (const auto &trade : trades)
            listener_.onTrade(trade);
    }
    if (listener_.onBook)
        listener_.onBook(book);
}

}
