#include "node.hh"

class ART {
public:
    std::atomic<Element*> root;

    ART() {
        root.store(new Node4());
    }

    std::pair<void*, bool> get(std::vector<uint8_t> key) {
        auto n = (Node*) root.load();
        auto ret = n->searchOpt(key, 0, nullptr, 0);
        auto value = std::get<0>(ret);
        auto ex = std::get<1>(ret);
        auto ok = std::get<2>(ret);
        if (ok) {
            return {value, ex};
        }
    }

    void put(std::vector<uint8_t> key, Value value) {
        while (true) {
            auto n = (Node*) root.load();
            printf("try\n");
            if (n->insertOpt(key, value, 0, nullptr, 0, root)) {
                return;
            }
        }
    }
};
