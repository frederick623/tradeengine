#pragma once

#include "marketdata.h"

#include <algorithm>
#include <string>
#include <vector>

namespace mde {

enum class MarketPriceSource : uint8_t {
    ORDER_BOOK,
    TRADE,
};

struct MarketPriceEvent {
    InstrumentKey     instrument;
    Price             price;
    uint64_t          quantity{0};
    Side              side{Side::BID};
    MarketPriceSource source{MarketPriceSource::ORDER_BOOK};
    NsTimestamp       receivedAt{0};
};

struct BrokerOrderStateEvent {
    std::string orderID;
    std::string clientOrderID;
    std::string executionID;
    std::string symbol;
    std::string status;
    std::string executionType;
    std::string leavesQuantity;
    std::string cumulativeQuantity;
    std::string lastQuantity;
    std::string lastPrice;
};

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual void onMarketPrice(const MarketPriceEvent&) {}
    virtual void onOrderStateChange(const BrokerOrderStateEvent&) {}
};

class StrategyDispatcher : public HandlerDefaults {
public:
    void add(Strategy& strategy) {
        if (std::find(strategies_.begin(), strategies_.end(), &strategy) == strategies_.end())
            strategies_.push_back(&strategy);
    }

    void remove(Strategy& strategy) {
        strategies_.erase(
            std::remove(strategies_.begin(), strategies_.end(), &strategy),
            strategies_.end());
    }

    void onOrderEvent(const OrderEvent& event) {
        if (event.kind != OrderEventKind::ADD && event.kind != OrderEventKind::MODIFY)
            return;
        publishMarketPrice({
            event.instrument, event.price, event.quantity, event.side,
            MarketPriceSource::ORDER_BOOK, event.receivedAt
        });
    }

    void onTrade(const TradeEvent& event) {
        publishMarketPrice({
            event.instrument, event.price, event.quantity, event.aggressor,
            MarketPriceSource::TRADE, event.receivedAt
        });
    }

    void publishOrderState(const BrokerOrderStateEvent& event) {
        for (Strategy* strategy : strategies_)
            strategy->onOrderStateChange(event);
    }

private:
    void publishMarketPrice(const MarketPriceEvent& event) {
        for (Strategy* strategy : strategies_)
            strategy->onMarketPrice(event);
    }

    std::vector<Strategy*> strategies_;
};

} // namespace mde
