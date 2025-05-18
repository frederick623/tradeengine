#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/task.hpp>

#include "order.hpp"
#include "channel.hpp"

struct Exchange
{
    template <IsOrder T> 
    void add(T&& value)
    {
        ladders_[value.exchange_ticker].add(std::forward<T>(value));
    }

    boost::cobalt::task<void> process(OrderCh& ch)
    {
        while (true) 
        {
            auto order = co_await ch.read(); // Read line from channel
            try
            {
                auto iter = ids_.find(order.order_id);
 
                if (order.request_type=='N')
                {
                    if (iter==ids_.end()) ids_.insert(order.order_id);
                    else throw std::runtime_error("Duplicated order id");
                }
                else
                {
                    if (iter==ids_.end()) throw std::runtime_error("Invalid order id");
                    else if (order.request_type=='C') ids_.erase(order.order_id);
                }

                add<Order>(std::move(order));
            }
            catch(const std::runtime_error& e)
            {
                // Can add more handling here
                std::cerr << e.what() << ": " << order << '\n';
            }
        }
        co_return;
    }
    
    void print() const
    {
        for (const auto& pair : ladders_)
        {
            std::cout << "Ticker: " << pair.first << std::endl;
            pair.second.print();
        }
    }
    
private:
    std::unordered_set<std::string> ids_;
    std::unordered_map<int, Orders> ladders_;
};
