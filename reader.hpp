#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/generator.hpp>

#include "order.hpp"
#include "channel.hpp"

namespace asio = boost::asio;
namespace fs = std::filesystem;

struct Reader
{
    Reader(const std::string& filename)
    : file_(filename)
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

    // Generator function to read lines from file
    boost::cobalt::task<void> read_lines(OrderCh& ch)
    {
        std::string line;
        if (!file_.is_open()) 
        {
            std::cerr << "Failed to open file: " << std::endl;
            co_return ;
        }

        while (std::getline(file_, line)) 
        {
            try
            {
                co_await ch.write(from_line(line)); 
            }
            catch(const std::runtime_error& e)
            {
                std::cerr << e.what() << '\n';
            }
            
        }
        co_return ;
    }

private:
    std::ifstream file_;
};
