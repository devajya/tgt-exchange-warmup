# TGT Exchange warm-up

A single-symbol, in-memory limit order book service written in C++20 with Drogon. Orders are submitted, cancelled and inspected over HTTP. Every trade and book change is streamed over a WebSocket. Matching uses price-time priority: the best price trades first, and orders at the same price trade in arrival order.

## Build and test

Tested on Ubuntu with g++ 15 and CMake 4.2. Debian's Drogon package needs several development libraries that apt does not pull in automatically, so install them all:

```
sudo apt install libdrogon-dev libjsoncpp-dev uuid-dev libsqlite3-dev \
    libmariadb-dev libhiredis-dev libyaml-cpp-dev clang-tidy
```

`clang-tidy` is optional. When it is installed, it runs on every build.

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/exchange_server
```

The server listens on port 8080.

## HTTP API

Prices are integers in ticks, and quantities are integers.

| Method | Path | Success | Errors |
|---|---|---|---|
| `POST` | `/orders` | `201` with the order outcome | `400` for a malformed body or invalid values |
| `DELETE` | `/orders/{id}` | `200` | `400` for a malformed id, `404` for an unknown or inactive order |
| `GET` | `/book` | `200` with the top 5 price levels per side | none |

Submitting an order:

```
POST /orders
{"side": "buy", "price": 101, "quantity": 6}
```

```json
{
  "orderId": 4,
  "status": "filled",
  "filledQuantity": 6,
  "remainingQuantity": 0,
  "trades": [
    {"restingOrderId": 2, "incomingOrderId": 4, "side": "buy", "price": 100, "quantity": 3},
    {"restingOrderId": 1, "incomingOrderId": 4, "side": "buy", "price": 101, "quantity": 3}
  ]
}
```

`side` must be exactly `"buy"` or `"sell"`. `price` and `quantity` must be JSON integers, and both must be positive. `status` is `filled` (nothing left), `partially_filled` (some trades, with the remainder resting on the book) or `resting` (no trades). A trade always executes at the resting order's price.

`GET /book` returns quantity aggregated per price level, best price first, along with the current sequence number:

```json
{"bids": [{"price": 99, "quantity": 4}], "asks": [{"price": 101, "quantity": 2}], "seq": 6}
```

Every error has the form `{"error": "<message>"}`.

## Market data (WebSocket)

Connect to `ws://localhost:8080/marketdata`. The server first sends a `book` snapshot of the current top 5 levels, then streams two kinds of message:

```json
{"type": "trade", "seq": 3, "restingOrderId": 1, "incomingOrderId": 3, "side": "buy", "price": 100, "quantity": 3}
{"type": "book", "seq": 5, "bids": [], "asks": [{"price": 101, "quantity": 4}]}
```

For each accepted order, the server sends one `trade` message per fill, then a `book` message showing the book after the change. A successful cancel produces a `book` message. Every message carries a `seq` that increases strictly across the whole exchange. Messages from concurrent requests can arrive slightly out of order, so a client should ignore any message whose `seq` is not greater than the last one it applied. Messages sent by the client are ignored.

## Postman demo

Import `postman/tgt-exchange.postman_collection.json`. The "Valid flow" folder is meant to be run top to bottom against a freshly started server. It rests some orders, crosses two price levels, and cancels an order. The "Rejected requests" folder shows the 400 and 404 responses. Postman's collection format cannot store WebSocket requests, so open one by hand with **New → WebSocket** and the URL `ws://localhost:8080/marketdata`, and keep it connected while running the HTTP requests.

## Design

The code has two layers. `core/` is the exchange itself: plain C++ that never includes or links Drogon, and CMake enforces this because the core library has no Drogon dependency. `server/` is a thin Drogon adapter. It turns JSON into domain values, calls the core, and turns the results back into HTTP or WebSocket messages. The controller checks the shape and types of a request, and the core checks the domain rules. The core defines its own market-data listener interface, which the WebSocket controller implements. The dependency therefore points from Drogon into the core, never the other way.

**Ownership.** `main` creates the single `Exchange` and passes it by reference to both controllers. There are no globals or singletons, and the controllers do not own the exchange. Inside the book, orders are stored by value: a `std::map` from price to a `std::list` of orders per side, with bids sorted highest first. A hash map from order id to list iterator gives constant-time cancel. List iterators stay valid when other orders are inserted or removed, so the index never goes stale. No code uses `new` or `delete` directly.

**Concurrency.** Drogon calls each controller from several IO threads at once, so the book is shared mutable state. One `std::mutex` in `Exchange` guards every submit, cancel and snapshot for the whole operation. While it holds the lock, the exchange assigns order ids and stamps each event with its `seq`. It publishes the events only after releasing the lock, so a slow subscriber can never stall matching. Publishing after unlocking allows two concurrent requests to deliver their events out of order, which is why every message carries a `seq`. Matching is in-memory and short, so no event loop is blocked. The tests include an 8-thread concurrent submission test, and a ThreadSanitizer build of the test suite reports no data races.

## Layout

```
core/Types.h                 value types: orders, trades, levels, results
core/OrderBook.{h,cpp}       price-time priority matching, cancel, snapshot
core/Exchange.{h,cpp}        order ids, locking, sequence-numbered events
server/OrdersController.*    POST /orders, DELETE /orders/{id}, GET /book
server/MarketDataController.* WS /marketdata
server/JsonCodec.*           JSON conversion shared by both controllers
server/main.cc               wiring
tests/                       core and exchange unit tests (DrogonTest)
postman/                     demo collection
```
