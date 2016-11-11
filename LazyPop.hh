#ifndef LAZYPOP_H
#define LAZYPOP_H

template <typename T, class Queue>
class LazyPop {
public:
    typedef Queue queue_t;
    typedef T value_type;
    static constexpr int popitem_key = -2;
    
    LazyPop() : q_(NULL), popped_(false), fulfilled_(false), reassigned_(false) {};
    LazyPop(queue_t* q) : q_(q), popped_(false), fulfilled_(false), reassigned_(false) {};
  
    // someone is reassigning a lazypop (used on LHS)
    LazyPop& operator=(bool b) {
        fulfilled_ = true;
        popped_ = b;
        reassigned_ = true;
        return *this;
    };
    bool is_fulfilled() const {
        return fulfilled_;
    }
    bool is_popped() const {
        return popped_;
    }
    bool is_reassigned() const {
        return reassigned_;
    }
    void set_fulfilled() {
        fulfilled_ = true;
    }
    void set_popped(bool b) {
        assert(fulfilled_);
        popped_ = b;
    }
    // someone is accessing a lazypop. must be done after a transaction or 
    // after the lazypop has been reassigned
    operator bool() {
        assert(fulfilled_ || reassigned_);
        return popped_;
    }

private:
    queue_t* q_;
    bool popped_;
    bool fulfilled_;
    bool reassigned_;
};

#endif
