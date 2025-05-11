#pragma once

#include <iostream>
#include <string>
#include <vector>

struct Trade
{
    Trade(int ticker, std::string oid, int qty, double prc)
    : exchange_ticker(ticker)
    , order_id(std::move(oid))
    , quantity(qty)
    , price(prc)
    {

    }

    friend std::ostream& operator<<(std::ostream& os, Trade const& trade)
    {
        return os << trade.exchange_ticker << " " 
            << trade.order_id << " "
            << trade.quantity << " "
            << trade.price ;
    }

private:
    int exchange_ticker;
    std::string order_id;
    int quantity;
    double price;
};

struct Trades
{
    template <typename ... Ts>
    void add(Ts&& ... args)
    {
        trades_.emplace_back(std::forward<Ts>(args) ...);
    }

    const Trade& back() const
    {
        return trades_.back();
    }

private:
    std::vector<Trade> trades_;
};
