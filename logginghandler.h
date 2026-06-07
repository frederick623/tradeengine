#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  logginghandler.h  –  compile-time Handler that prints normalised events
//
//  A ready-made MarketDataHandler that streams every event to stdout.  Useful
//  as a demo sink for the live feed handler and for visibility inside tests.
// ─────────────────────────────────────────────────────────────────────────────
#include "marketdata.h"
#include <iostream>

class LoggingHandler : public mde::HandlerDefaults {
public:
    void onControl(const mde::ControlEvent& e) {
        std::cout << "[CTRL] exch=" << exchName(e.exchange)
                  << " kind=" << kindName(e.kind) << "\n";
    }

    void onInstrumentDef(const mde::InstrumentDef& d) {
        std::cout << "[INSTR] exch=" << exchName(d.key.exchange)
                  << " sym=" << d.key.symbol
                  << " kind=" << kindName(d.kind)
                  << " ccy=" << d.currency
                  << " exp=" << d.expirationDate
                  << " dec=" << (int)d.priceDecimals
                  << (d.isSuspended ? " [SUSP]" : "")
                  << "\n";
    }

    void onStrategyDef(const mde::StrategyDef& s) {
        std::cout << "[STRAT] sym=" << s.key.symbol
                  << " legs=" << s.legs.size() << " {";
        for (auto& l : s.legs)
            std::cout << l.instrument.symbol
                      << (l.legSide == mde::StrategyLegSide::AS_DEFINED ? "+" : "-")
                      << "x" << l.ratio << " ";
        std::cout << "}\n";
    }

    void onMarketState(const mde::MarketStateEvent& e) {
        std::cout << "[STATE] exch=" << exchName(e.exchange)
                  << " sym=" << e.instrument.symbol
                  << " state=" << stateName(e.state)
                  << " detail=" << e.stateDetail
                  << (e.isEndOfDay ? " [EOD]" : "") << "\n";
    }

    void onOrderEvent(const mde::OrderEvent& e) {
        std::cout << "[ORDER] exch=" << exchName(e.exchange)
                  << " sym=" << e.instrument.symbol
                  << " " << kindName(e.kind)
                  << " oid=" << e.orderID
                  << " " << (e.side == mde::Side::BID ? "BID" : "ASK")
                  << " px=" << e.price.toDouble()
                  << " qty=" << e.quantity;
        if (e.bookPosition) std::cout << " pos=" << e.bookPosition;
        std::cout << "\n";
    }

    void onTrade(const mde::TradeEvent& e) {
        std::cout << "[TRADE] exch=" << exchName(e.exchange)
                  << " sym=" << e.instrument.symbol
                  << " " << kindName(e.kind)
                  << " tid=" << e.tradeID
                  << " px=" << e.price.toDouble()
                  << " qty=" << e.quantity
                  << (e.isPrintable ? "" : " [no-print]") << "\n";
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
