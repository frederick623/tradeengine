#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/generator.hpp>

namespace asio = boost::asio;
namespace fs = std::filesystem;

struct Reader
{
    Reader(const std::string& filename)
    : file_(filename)
    {
        
    }

    // Generator function to read lines from file
    boost::cobalt::task<void> read_lines(boost::cobalt::channel<std::string>& ch)
    {
        std::string line;
        if (!file_.is_open()) 
        {
            std::cerr << "Failed to open file: " << std::endl;
            co_return ;
        }

        while (std::getline(file_, line)) 
        {
            co_await ch.write(line); 
        }
        co_return ;
    }

private:
    std::ifstream file_;
};
