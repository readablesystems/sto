#pragma once

#include <any>
#include <vector>

namespace storia {

class Upquery {
public:
    Upquery(std::initializer_list<std::any> keys) : keys_(keys) {}

    std::vector<std::any> keys() const {
        return keys_;
    }

private:
    std::vector<std::any> keys_;
};

};  // namespace storia
