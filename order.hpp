#pragma once

#include <cmath>
#include <set>
#include <string>
#include <utility>

#include "policy.hpp"
#include "trade.hpp"

constexpr static double TICK_THRESHOLD = 1E-6;

struct Order
{
    int exchange_ticker;
    char request_type;
    std::string order_id;
    char side;
    mutable int quantity;
    double price;

    // Operator overload for comparison based on price
    bool operator<(const Order& other) const {
        return price<other.price-TICK_THRESHOLD;
    }

    // Operator overload for comparison based on price
    bool operator>(const Order& other) const {
        return price>other.price+TICK_THRESHOLD;
    }

    // Operator overload for comparison based on price
    bool operator==(const Order& other) const {
        return std::fabs(price-other.price)<=TICK_THRESHOLD;
    }

    friend std::ostream& operator<<(std::ostream& os, Order const& order)
    {
        return os 
            << "OrderId: " << order.order_id 
            << " Side: " << (order.side=='B'?"Buy":"Sell")
            << " Quantity: " << order.quantity 
            << " Price: " << order.price ;
    }

    Order(int ticker, char req, std::string oid, char sid, int qty, double prc)
    : exchange_ticker(ticker)
    , request_type(req)
    , order_id(std::move(oid))
    , side(sid)
    , quantity(qty)
    , price(prc)
    {

    }

    static Order from_line(const std::string& line)
    {
        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, ',');
        auto exchange_ticker = std::stoi(token);
        std::getline(ss, token, ',');
        auto request_type = token[0];
        if (request_type!='N' and request_type!='A' and request_type!='C') throw std::runtime_error("Invalid request type");
        std::getline(ss, token, ',');
        auto order_id = token;
        std::getline(ss, token, ',');
        auto side = token[0];
        std::getline(ss, token, ',');
        auto quantity = std::stoi(token);
        if (quantity<=0) throw std::runtime_error("Invalid quantity");
        std::getline(ss, token, ',');
        auto price = std::stod(token);
        if (price<=0) throw std::runtime_error("Invalid price");

        return Order(exchange_ticker, request_type, order_id, side, quantity, price);
    }

};

template <typename T>
concept IsOrder = std::same_as<std::remove_reference_t<T>, Order>;

template <typename Derived, IsOrder Order, ValidOrderPolicy OrderPolicy>
struct OrderBase : CrtpBase<Derived, Order, OrderPolicy> 
{
    void match(Order& order, Trades& trades)
    {
        while (true)
        {
            auto& data = this->getData();
            if (!data.size()) break;
            auto& cur = *data.begin();

            if (order.quantity and !this->compare(order, cur))
            {
                auto qty = std::min(cur.quantity, order.quantity);
                order.quantity -= qty;
                cur.quantity -= qty;

                trades.add(cur.exchange_ticker, cur.order_id, qty, cur.price);
                std::cout << "Trade: " << trades.back() << std::endl;
                trades.add(order.exchange_ticker, order.order_id, qty, cur.price);
                std::cout << "Trade: " << trades.back() << std::endl;

                if (!cur.quantity) data.erase(cur);
            }
            else
            {
                break;
            }
        }
    }

    void create(const Order& order)
    {
        auto& data = this->getData();
        if (order.quantity) data.insert(order);
    }
   
    void remove(const Order& order)
    {
        auto& data = this->getData();
        std::erase_if(data, [&order](const auto& ele){
            return ele.order_id==order.order_id;
        });
    }

};

// Concrete class for ascending order
struct AskOrders : OrderBase<AskOrders, Order, AskPolicy> 
{
    std::multiset<Order, AskPolicy::Comparator<Order>> data_;
    char side{'A'};
};
    
// Concrete class for descending order
struct BidOrders : OrderBase<BidOrders, Order, BidPolicy> 
{
    std::multiset<Order, BidPolicy::Comparator<Order>> data_;
    char side{'B'};
};

struct Orders
{
    template <IsOrder T> 
    void add(T&& value)
    {
        if (value.request_type=='N')
        {
            create(std::forward<T>(value));
        }
        else if (value.request_type=='A')
        {
            remove(std::forward<T>(value));
            create(std::forward<T>(value));
        }
        else if (value.request_type=='C')
        {
            remove(std::forward<T>(value));
        }
        
    }

    template <IsOrder T> 
    void create(T&& value)
    {
        if (value.side=='S')
        {
            std::get<BidOrders>(ladders_).match(value, trades_);
            std::get<AskOrders>(ladders_).create(value);
        }
        else
        {
            std::get<AskOrders>(ladders_).match(value, trades_);
            std::get<BidOrders>(ladders_).create(value);
        }
    }

    template <IsOrder T> 
    void remove(T&& value)
    {
        if (value.side=='S')
        {
            std::get<AskOrders>(ladders_).remove(value);
        }
        else
        {
            std::get<BidOrders>(ladders_).remove(value);
        }
    }

    void print() const
    {
        for (const auto& order : std::get<BidOrders>(ladders_).data_)
        {
            std::cout << order << std::endl;
        }
        for (const auto& order : std::get<AskOrders>(ladders_).data_)
        {
            std::cout << order << std::endl;
        }
    }

private:
    std::pair<BidOrders, AskOrders> ladders_;
    Trades trades_;
};
