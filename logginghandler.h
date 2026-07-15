#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  logginghandler.h  –  compile-time Handler that logs normalised events
//
//  A ready-made MarketDataHandler that writes every event through NanoLog. Useful
//  as a demo sink for the live feed handler and for visibility inside tests.
// ─────────────────────────────────────────────────────────────────────────────
#include "marketdata.h"
#include "adapter/orderbook.h"
#include "NanoLog.hpp"
#include <string>

class LoggingHandler : public mde::HandlerDefaults {
public:
    void onControl(const mde::ControlEvent& e) {
        LOG_INFO << "[CTRL] exch=" << exchName(e.exchange)
                 << " kind=" << kindName(e.kind);
    }

    void onInstrumentDef(const mde::InstrumentDef& d) {
        LOG_INFO << "[INSTR] exch=" << exchName(d.key.exchange)
                 << " sym=" << d.key.symbol
                 << " kind=" << kindName(d.kind)
                 << " ccy=" << d.currency
                 << " exp=" << d.expirationDate
                 << " dec=" << static_cast<uint32_t>(d.priceDecimals)
                 << (d.isSuspended ? " [SUSP]" : "");
    }

    void onStrategyDef(const mde::StrategyDef& s) {
        std::string legs;
        for (const auto& l : s.legs) {
            legs += l.instrument.symbol;
            legs += (l.legSide == mde::StrategyLegSide::AS_DEFINED ? "+" : "-");
            legs += "x";
            legs += std::to_string(l.ratio);
            legs += " ";
        }
        LOG_INFO << "[STRAT] sym=" << s.key.symbol
                 << " legs=" << static_cast<uint64_t>(s.legs.size())
                 << " {" << legs << "}";
    }

    void onMarketState(const mde::MarketStateEvent& e) {
        LOG_INFO << "[STATE] exch=" << exchName(e.exchange)
                 << " sym=" << e.instrument.symbol
                 << " state=" << stateName(e.state)
                 << " detail=" << e.stateDetail
                 << (e.isEndOfDay ? " [EOD]" : "");
    }

    void onOrderEvent(const mde::OrderEvent& e) {
        LOG_INFO << "[ORDER] exch=" << exchName(e.exchange)
                 << " sym=" << e.instrument.symbol
                 << " " << kindName(e.kind)
                 << " oid=" << static_cast<uint64_t>(e.orderID)
                 << " " << (e.side == mde::Side::BID ? "BID" : "ASK")
                 << " px=" << e.price.toDouble()
                 << " qty=" << static_cast<uint64_t>(e.quantity)
                 << (e.bookPosition ? " pos=" + std::to_string(e.bookPosition) : "");
    }

    void onTrade(const mde::TradeEvent& e) {
        LOG_INFO << "[TRADE] exch=" << exchName(e.exchange)
                 << " sym=" << e.instrument.symbol
                 << " " << kindName(e.kind)
                 << " tid=" << static_cast<uint64_t>(e.tradeID)
                 << " px=" << e.price.toDouble()
                 << " qty=" << static_cast<uint64_t>(e.quantity)
                 << (e.isPrintable ? "" : " [no-print]");
    }

    void logOrderBookSnapshot(const mde::InstrumentKey& k,
                              const mde::OrderBookMap::Book& book) {
        const auto bid = book.bestBid();
        const auto ask = book.bestAsk();
        LOG_INFO << "[BOOK] exch=" << exchName(k.exchange)
                 << " sym=" << k.symbol
                 << " nid=" << static_cast<uint64_t>(k.nativeID)
                 << " bid=" << (bid ? bid->value() : 0.0)
                 << " ask=" << (ask ? ask->value() : 0.0)
                 << " bidDepth5=" << static_cast<uint64_t>(book.marketDepth(::Side::Buy, 5))
                 << " askDepth5=" << static_cast<uint64_t>(book.marketDepth(::Side::Sell, 5))
                 << " liveOrders=" << static_cast<uint64_t>(book.poolUsed())
                 << (bid ? "" : " [NO_BID]")
                 << (ask ? "" : " [NO_ASK]");
    }

private:
    static const char* exchName(mde::Exchange ex) {
        switch(ex) { case mde::Exchange::HKEX: return "HKEX";
                     case mde::Exchange::TSE:  return "TSE"; default: return "?"; }
    }
    static const char* kindName(mde::ControlKind k) {
        switch(k) { case mde::ControlKind::HEARTBEAT:      return "HEARTBEAT";
                    case mde::ControlKind::SEQUENCE_RESET: return "SEQ_RESET";
                    case mde::ControlKind::DR_IN_PROGRESS: return "DR_START";
                    case mde::ControlKind::DR_COMPLETED:   return "DR_DONE";
                    case mde::ControlKind::COMM_START:     return "COMM_START";
                    case mde::ControlKind::COMM_END:       return "COMM_END";
                    case mde::ControlKind::RESET_START:    return "RESET_START";
                    case mde::ControlKind::RESET_END:      return "RESET_END";
                    default: return "?"; }
    }
    static const char* kindName(mde::InstrumentKind k) {
        switch(k) { case mde::InstrumentKind::EQUITY:      return "EQ";
                    case mde::InstrumentKind::FUTURE:       return "FUT";
                    case mde::InstrumentKind::OPTION_CALL:  return "OPT_C";
                    case mde::InstrumentKind::OPTION_PUT:   return "OPT_P";
                    case mde::InstrumentKind::STRATEGY:     return "STRAT";
                    default: return "?"; }
    }
    static const char* kindName(mde::OrderEventKind k) {
        switch(k) { case mde::OrderEventKind::ADD:    return "ADD";
                    case mde::OrderEventKind::MODIFY:  return "MOD";
                    case mde::OrderEventKind::DELETE:  return "DEL";
                    case mde::OrderEventKind::CLEAR:   return "CLR";
                    default: return "?"; }
    }
    static const char* kindName(mde::TradeKind k) {
        switch(k) { case mde::TradeKind::NORMAL:          return "ZARABA";
                    case mde::TradeKind::AUCTION:          return "ITAYOSE";
                    case mde::TradeKind::TRADE_AMENDMENT:  return "AMEND";
                    default: return "?"; }
    }
    static const char* stateName(mde::SessionState s) {
        switch(s) { case mde::SessionState::TRADING:           return "TRADING";
                    case mde::SessionState::ORDER_ACCEPTANCE:   return "PRE_OPEN";
                    case mde::SessionState::AUCTION_OPEN:       return "AUCTION_OPEN";
                    case mde::SessionState::AUCTION_CLOSE:      return "AUCTION_CLOSE";
                    case mde::SessionState::TRADING_HALT:       return "HALT";
                    case mde::SessionState::SUSPENDED:          return "SUSPENDED";
                    case mde::SessionState::END_OF_SESSION:     return "EOS";
                    case mde::SessionState::END_OF_DAY:         return "EOD";
                    default: return "?"; }
    }
};
