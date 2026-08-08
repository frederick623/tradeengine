#pragma once

#include "feed/marketdata.h"

#include <algorithm>
#include <string>
#include <vector>

#ifdef TRADEENGINE_HAVE_FIXER
#include <fixer/fixer.hpp>
#endif

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

#ifdef TRADEENGINE_HAVE_FIXER
struct SendOrderRequest {
    std::string         clientOrderID;
    std::string         symbol;
    fixer::Side         side{fixer::Side::Buy};
    std::string         quantity;
    fixer::OrderKind    kind{fixer::OrderKind::Limit};
    fixer::TimeInForce  timeInForce{fixer::TimeInForce::Day};
    std::string         price;
};

struct ModifyOrderRequest {
    std::string      requestID;
    std::string      originalClientOrderID;
    SendOrderRequest replacement;
};

struct CancelOrderRequest {
    std::string requestID;
    std::string originalClientOrderID;
    std::string symbol;
    fixer::Side side{fixer::Side::Buy};
};

class StrategyOrderGateway {
public:
    virtual ~StrategyOrderGateway() = default;
    virtual bool sendOrder(const SendOrderRequest&) = 0;
    virtual bool modifyOrder(const ModifyOrderRequest&) = 0;
    virtual bool cancelOrder(const CancelOrderRequest&) = 0;
};
#endif

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual void onMarketPrice(const MarketPriceEvent&) {}
    virtual void onOrderStateChange(const BrokerOrderStateEvent&) {}

#ifdef TRADEENGINE_HAVE_FIXER
protected:
    bool sendOrder(const SendOrderRequest& request) {
        return orderGateway_ ? orderGateway_->sendOrder(request) : false;
    }
    bool modifyOrder(const ModifyOrderRequest& request) {
        return orderGateway_ ? orderGateway_->modifyOrder(request) : false;
    }
    bool cancelOrder(const CancelOrderRequest& request) {
        return orderGateway_ ? orderGateway_->cancelOrder(request) : false;
    }

private:
    friend class StrategyDispatcher;
    void setOrderGateway(StrategyOrderGateway* gateway) noexcept { orderGateway_ = gateway; }
    StrategyOrderGateway* orderGateway_{nullptr};
#endif
};

class StrategyDispatcher : public HandlerDefaults {
public:
    void add(Strategy& strategy) {
        if (std::find(strategies_.begin(), strategies_.end(), &strategy) == strategies_.end()) {
            strategies_.push_back(&strategy);
#ifdef TRADEENGINE_HAVE_FIXER
            strategy.setOrderGateway(orderGateway_);
#endif
        }
    }

    void remove(Strategy& strategy) {
#ifdef TRADEENGINE_HAVE_FIXER
        strategy.setOrderGateway(nullptr);
#endif
        strategies_.erase(
            std::remove(strategies_.begin(), strategies_.end(), &strategy),
            strategies_.end());
    }

#ifdef TRADEENGINE_HAVE_FIXER
    void bindOrderGateway(StrategyOrderGateway* gateway) {
        orderGateway_ = gateway;
        for (Strategy* strategy : strategies_)
            strategy->setOrderGateway(orderGateway_);
    }
#endif

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
#ifdef TRADEENGINE_HAVE_FIXER
    StrategyOrderGateway*  orderGateway_{nullptr};
#endif
};

} // namespace mde
