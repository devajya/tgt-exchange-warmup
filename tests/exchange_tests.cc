#include <drogon/drogon_test.h>

#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/Exchange.h"

using namespace exchange;

struct Recorded
{
    std::string kind;
    Seq seq;
};

DROGON_TEST(AcceptedOrdersGetDenseIds)
{
    Exchange ex;

    auto first = ex.submit(Side::Buy, 100, 5);
    auto rejected = ex.submit(Side::Buy, 0, 5);
    auto second = ex.submit(Side::Sell, 105, 5);

    CHECK(first.orderId == 1);
    CHECK(rejected.rejected.has_value());
    CHECK(rejected.orderId == 0);
    CHECK(second.orderId == 2);
}

DROGON_TEST(TradeEventsPrecedeBookEventWithIncreasingSeq)
{
    Exchange ex;
    std::vector<Recorded> events;
    ex.setListener({
        .onTrade = [&](const TradeEvent &e) { events.push_back({"trade", e.seq}); },
        .onBook = [&](const BookEvent &e) { events.push_back({"book", e.seq}); },
    });
    ex.submit(Side::Sell, 100, 3);
    ex.submit(Side::Sell, 101, 3);
    events.clear();

    ex.submit(Side::Buy, 101, 5);

    REQUIRE(events.size() == 3);
    CHECK(events[0].kind == "trade");
    CHECK(events[1].kind == "trade");
    CHECK(events[2].kind == "book");
    CHECK(events[0].seq < events[1].seq);
    CHECK(events[1].seq < events[2].seq);
}

DROGON_TEST(BookEventCarriesTopOfBook)
{
    Exchange ex;
    BookEvent last{};
    ex.setListener({.onTrade = {}, .onBook = [&](const BookEvent &e) { last = e; }});

    ex.submit(Side::Buy, 99, 4);

    REQUIRE(last.book.bids.size() == 1);
    CHECK(last.book.bids[0].price == 99);
    CHECK(last.book.bids[0].qty == 4);
}

DROGON_TEST(CancelPublishesBookEventOnlyOnSuccess)
{
    Exchange ex;
    int bookEvents = 0;
    ex.setListener({.onTrade = {}, .onBook = [&](const BookEvent &) { ++bookEvents; }});
    auto placed = ex.submit(Side::Buy, 100, 5);
    bookEvents = 0;

    CHECK(ex.cancel(placed.orderId) == CancelResult::Cancelled);
    CHECK(bookEvents == 1);
    CHECK(ex.cancel(placed.orderId) == CancelResult::NotFound);
    CHECK(ex.cancel(999) == CancelResult::NotFound);
    CHECK(bookEvents == 1);
}

DROGON_TEST(RejectedSubmitPublishesNothing)
{
    Exchange ex;
    int calls = 0;
    ex.setListener({
        .onTrade = [&](const TradeEvent &) { ++calls; },
        .onBook = [&](const BookEvent &) { ++calls; },
    });

    ex.submit(Side::Buy, 100, 0);

    CHECK(calls == 0);
}

DROGON_TEST(SnapshotReportsLatestSeqWithoutAdvancing)
{
    Exchange ex;
    Seq lastPublished = 0;
    ex.setListener(
        {.onTrade = {}, .onBook = [&](const BookEvent &e) { lastPublished = e.seq; }});
    ex.submit(Side::Sell, 100, 5);

    auto first = ex.snapshot();
    auto second = ex.snapshot();

    CHECK(first.seq == lastPublished);
    CHECK(second.seq == first.seq);
    REQUIRE(first.book.asks.size() == 1);
    CHECK(first.book.asks[0].price == 100);
}

DROGON_TEST(ConcurrentSubmitsGetUniqueIdsAndSeqs)
{
    Exchange ex;
    std::mutex recordMutex;
    std::set<Seq> seqs;
    ex.setListener({.onTrade = {}, .onBook = [&](const BookEvent &e) {
                        const std::scoped_lock lock(recordMutex);
                        seqs.insert(e.seq);
                    }});

    constexpr int threads = 8;
    constexpr int perThread = 500;
    std::vector<std::vector<OrderId>> ids(threads);
    {
        std::vector<std::jthread> workers;
        workers.reserve(threads);
        for (int t = 0; t < threads; ++t)
        {
            workers.emplace_back([&ex, &ids, t] {
                for (int i = 0; i < perThread; ++i)
                    ids[t].push_back(ex.submit(Side::Buy, 100 + t, 1).orderId);
            });
        }
    }

    std::set<OrderId> unique;
    for (const auto &batch : ids)
        unique.insert(batch.begin(), batch.end());
    CHECK(unique.size() == threads * perThread);
    CHECK(*unique.begin() == 1);
    CHECK(*unique.rbegin() == threads * perThread);
    CHECK(seqs.size() == threads * perThread);
    auto snap = ex.snapshot();
    CHECK(snap.book.bids.size() == Exchange::kBookDepth);
    CHECK(snap.book.bids[0].price == 100 + threads - 1);
    CHECK(snap.book.bids[0].qty == perThread);
}
