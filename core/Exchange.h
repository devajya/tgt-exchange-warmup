#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "core/OrderBook.h"

namespace exchange {

using Seq = std::uint64_t;

struct TradeEvent
{
    Seq seq{};
    Trade trade{};
};

struct BookEvent
{
    Seq seq{};
    BookSnapshot book;
};

struct MarketDataListener
{
    std::function<void(const TradeEvent &)> onTrade;
    std::function<void(const BookEvent &)> onBook;
};

class Exchange
{
  public:
    static constexpr std::size_t kBookDepth = 5;

    void setListener(MarketDataListener listener);
    SubmitResult submit(Side side, Price price, Qty qty);
    CancelResult cancel(OrderId id);
    BookEvent snapshot() const;

  private:
    void publish(const std::vector<TradeEvent> &trades, const BookEvent &book) const;

    mutable std::mutex mutex_;
    OrderBook book_;
    OrderId nextId_ = 1;
    Seq seq_ = 0;
    MarketDataListener listener_;
};

}
