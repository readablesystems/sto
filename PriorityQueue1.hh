#pragma once

#include "Vector.hh"
#include <algorithm>

template <typename T, int INIT_CAPACITY = 10000>
class PriorityQueue1 {
public:
    PriorityQueue1() : elems(INIT_CAPACITY) {}

    
    void push(T v) {
        elems.push_back(v);
        std::push_heap(elems.begin(), elems.end());
    }
    
    void pop() {
        try {
            // XXX(nate): this doesn't compile on OS X clang with a complaint that std::swap is getting an lvalue.
            // this broke in 9bdea5a (we stopped returning a reference in operator*) but idrk how to fix it.
            std::pop_heap(elems.begin(), elems.end());
            elems.pop_back();
        } catch (const std::out_of_range& e) {}
    }
    
    T top() {
        try {
        return elems.front();
        } catch (const std::out_of_range& e) {}
    }

    uint32_t size() {
        return elems.size();
    }

    void print() {
        elems.print();
    }
    
private:
    Vector<T> elems;
};
