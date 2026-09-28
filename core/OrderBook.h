#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>

#include "core/Types.h"

namespace exchange {

class OrderBook
{
  public:
    SubmitResult submit(Order order);
    CancelResult cancel(OrderId id);
    BookSnapshot snapshot(std::size_t depth) const;

  private:
    using Queue = std::list<Order>;
    using Bids = std::map<Price, Queue, std::greater<>>;
    using Asks = std::map<Price, Queue>;

    struct Locator
    {
        Side side{};
        Price price{};
        Queue::iterator it;
    };

    template <typename Levels>
    void match(Order &incoming, Levels &opposite, SubmitResult &result);

    template <typename Levels>
    void rest(const Order &order, Levels &own);

    template <typename Levels>
    void erase(Levels &levels, const Locator &loc);

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Locator> index_;
};

}
