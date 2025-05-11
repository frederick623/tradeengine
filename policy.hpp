#pragma once

#include <utility>
  
// Policy classes for ordering
struct OrderPolicy
{

};

struct BidPolicy : OrderPolicy {
    template <typename T>
    struct Comparator {
        bool operator()(const T& a, const T& b) const {
            return a > b;
        }
    };
};

struct AskPolicy : OrderPolicy {
    template <typename T>
    struct Comparator {
        bool operator()(const T& a, const T& b) const {
            return a < b;
        }
    };
};

template <typename T>
concept ValidOrderPolicy = std::derived_from<T, OrderPolicy>;

// CRTP base class for sorted set
template <typename Derived, typename ValueType, ValidOrderPolicy OrderPolicy>
struct CrtpBase 
{
    using Comparator = typename OrderPolicy::template Comparator<ValueType>;
    using SetType = std::multiset<ValueType, Comparator>;

    // Perfect forwarding for lvalue and rvalue
    template <typename T>
    void add(T&& value) 
    {
        static_cast<Derived*>(this)->data_.insert(std::forward<T>(value));
    }

    SetType& getData() 
    {
        return static_cast<Derived*>(this)->data_;
    }

    Comparator compare;
};
