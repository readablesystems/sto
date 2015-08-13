#pragma once

#include "Vector.hh"
#include <algorithm>

template <typename T>
class PriorityQueue1 {
public:
    PriorityQueue1() : elems() {}

    
    void push(T v) {
        elems.push_back(v);
        std::push_heap(elems.begin(), elems.end());
    }
    
    void pop() {
        std::pop_heap(elems.begin(), elems.end());
        elems.pop_back();
    }
    
    T top() {
        return elems.front();
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
