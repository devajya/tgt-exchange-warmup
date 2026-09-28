#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>

#include "core/OrderBook.h"

using namespace exchange;

static Order buy(OrderId id, Price price, Qty qty)
{
    return {.id = id, .side = Side::Buy, .price = price, .qty = qty};
}

static Order sell(OrderId id, Price price, Qty qty)
{
    return {.id = id, .side = Side::Sell, .price = price, .qty = qty};
}

DROGON_TEST(PricePriority)
{
    OrderBook book;
    book.submit(sell(1, 101, 5));
    book.submit(sell(2, 100, 5));

    auto result = book.submit(buy(3, 101, 5));

    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].restingId == 2);
    CHECK(result.trades[0].price == 100);
}

DROGON_TEST(FifoWithinPriceLevel)
{
    OrderBook book;
    book.submit(sell(1, 100, 5));
    book.submit(sell(2, 100, 5));

    auto result = book.submit(buy(3, 100, 5));

    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].restingId == 1);
}

DROGON_TEST(PartialFillOfRestingOrder)
{
    OrderBook book;
    book.submit(sell(1, 100, 10));

    auto result = book.submit(buy(2, 100, 4));

    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].qty == 4);
    CHECK(result.remainingQty == 0);
    auto snap = book.snapshot(5);
    REQUIRE(snap.asks.size() == 1);
    CHECK(snap.asks[0].qty == 6);
    CHECK(snap.bids.empty());
}

DROGON_TEST(PartialFillOfIncomingOrderRestsRemainder)
{
    OrderBook book;
    book.submit(sell(1, 100, 10));

    auto result = book.submit(buy(2, 100, 15));

    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].qty == 10);
    CHECK(result.remainingQty == 5);
    auto snap = book.snapshot(5);
    CHECK(snap.asks.empty());
    REQUIRE(snap.bids.size() == 1);
    CHECK(snap.bids[0].price == 100);
    CHECK(snap.bids[0].qty == 5);
}

DROGON_TEST(CrossesMultipleRestingOrders)
{
    OrderBook book;
    book.submit(sell(1, 100, 3));
    book.submit(sell(2, 101, 3));
    book.submit(sell(3, 102, 3));

    auto result = book.submit(buy(4, 101, 5));

    REQUIRE(result.trades.size() == 2);
    CHECK(result.trades[0].restingId == 1);
    CHECK(result.trades[0].price == 100);
    CHECK(result.trades[0].qty == 3);
    CHECK(result.trades[1].restingId == 2);
    CHECK(result.trades[1].price == 101);
    CHECK(result.trades[1].qty == 2);
    CHECK(result.remainingQty == 0);
    auto snap = book.snapshot(5);
    REQUIRE(snap.asks.size() == 2);
    CHECK(snap.asks[0].price == 101);
    CHECK(snap.asks[0].qty == 1);
    CHECK(snap.asks[1].price == 102);
    CHECK(snap.asks[1].qty == 3);
}

DROGON_TEST(NonCrossingOrdersRest)
{
    OrderBook book;
    book.submit(sell(1, 100, 5));

    auto result = book.submit(buy(2, 99, 5));

    CHECK(result.trades.empty());
    CHECK(result.remainingQty == 5);
    auto snap = book.snapshot(5);
    REQUIRE(snap.bids.size() == 1);
    REQUIRE(snap.asks.size() == 1);
    CHECK(snap.bids[0].price == 99);
    CHECK(snap.asks[0].price == 100);
}

DROGON_TEST(CancelRemovesRestingOrder)
{
    OrderBook book;
    book.submit(buy(1, 100, 5));

    CHECK(book.cancel(1) == CancelResult::Cancelled);

    auto snap = book.snapshot(5);
    CHECK(snap.bids.empty());
    auto result = book.submit(sell(2, 100, 5));
    CHECK(result.trades.empty());
}

DROGON_TEST(CancelKeepsOtherOrdersAtLevel)
{
    OrderBook book;
    book.submit(buy(1, 100, 5));
    book.submit(buy(2, 100, 7));

    CHECK(book.cancel(1) == CancelResult::Cancelled);

    auto snap = book.snapshot(5);
    REQUIRE(snap.bids.size() == 1);
    CHECK(snap.bids[0].qty == 7);
}

DROGON_TEST(InvalidCancel)
{
    OrderBook book;
    CHECK(book.cancel(42) == CancelResult::NotFound);

    book.submit(sell(1, 100, 5));
    book.submit(buy(2, 100, 5));
    CHECK(book.cancel(1) == CancelResult::NotFound);

    book.submit(buy(3, 100, 5));
    CHECK(book.cancel(3) == CancelResult::Cancelled);
    CHECK(book.cancel(3) == CancelResult::NotFound);
}

DROGON_TEST(RejectsInvalidInput)
{
    OrderBook book;

    auto badPrice = book.submit(buy(1, 0, 5));
    auto badQty = book.submit(buy(2, 100, 0));
    auto negativeQty = book.submit(sell(3, 100, -1));

    CHECK(badPrice.rejected == RejectReason::NonPositivePrice);
    CHECK(badQty.rejected == RejectReason::NonPositiveQty);
    CHECK(negativeQty.rejected == RejectReason::NonPositiveQty);
    auto snap = book.snapshot(5);
    CHECK(snap.bids.empty());
    CHECK(snap.asks.empty());
}

DROGON_TEST(SnapshotAggregatesAndLimitsDepth)
{
    OrderBook book;
    OrderId id = 1;
    for (Price price = 95; price <= 100; ++price)
        book.submit(buy(id++, price, 1));
    book.submit(buy(id++, 100, 4));

    auto snap = book.snapshot(5);

    REQUIRE(snap.bids.size() == 5);
    CHECK(snap.bids[0].price == 100);
    CHECK(snap.bids[0].qty == 5);
    CHECK(snap.bids[4].price == 96);
}

int main(int argc, char **argv)
{
    return drogon::test::run(argc, argv);
}
