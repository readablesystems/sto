#pragma once

#include "Vector.hh"
#include <algorithm>

template <typename T>
class PriorityQueue1 {
public:
    PriorityQueue1() : elems() {}

    
    void push(T v) {
        try {
        elems.push_back(v);
        std::push_heap(elems.begin(), elems.end());
        } catch (Empty e) {Sto::abort();}
    }
    
    void pop() {
        try {
        std::pop_heap(elems.begin(), elems.end());
        elems.pop_back();
        } catch (Empty e) {}
    }
    
    T top() {
        try {
        return elems.front();
        } catch (Empty e) {}
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
