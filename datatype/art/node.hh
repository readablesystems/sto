#include <utility>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>
#include <algorithm>

#define maxPrefixLen 8

#define node16MinSize  4
#define node48MinSize  13
#define node256MinSize 38


#define NSPIN 30

enum NodeType {
    tNode4,
    tNode16,
    tNode48,
    tNode256,
    tNodeLeaf,
};

static inline std::vector<uint8_t> slice(const std::vector<uint8_t>& v, int start=0, int end=-1) {
    int oldlen = v.size();
    int newlen;

    if (end == -1 or end >= oldlen){
        newlen = oldlen-start;
    } else {
        newlen = end-start;
    }

    std::vector<uint8_t> nv(newlen);

    for (int i=0; i<newlen; i++) {
        nv[i] = v[start+i];
    }
    return nv;
}

class Leaf {
public:
    NodeType type;
    void* value; // maybe should be a T
    std::vector<uint8_t> key;

    Leaf(std::vector<uint8_t> k, void* v) {
        type = tNodeLeaf;
        key = k;
        value = v;
    }

    void updateOrExpand(std::vector<uint8_t> key, void* value, int depth, std::atomic<void*>& nodeLoc);

    bool match(std::vector<uint8_t> k) {
        if (key.size() != k.size()) {
            return false;
        }

        for (unsigned i = 0; i < k.size(); i++) {
            if (k[i] != key[i]) {
                return false;
            }
        }

        return true;
    }
};

class Node {
public:
    NodeType type;
    uint8_t numChildren;

    std::atomic<uint64_t> v;

    std::atomic<Leaf*> prefixLeaf;

    int32_t prefixLen;
    char prefix[maxPrefixLen];

    std::tuple<int, std::vector<uint8_t>, bool> prefixMismatch(std::vector<uint8_t> key, int depth, Node* parent, uint64_t version, uint64_t parentVersion);
    bool insertOpt(std::vector<uint8_t> key, void* value, int depth, Node* parent, uint64_t parentVersion, std::atomic<void*>& nodeLoc);
    std::pair<std::vector<uint8_t>, bool> fullKey(uint64_t version);
    void insertSplitPrefix(std::vector<uint8_t> key, std::vector<uint8_t> fullKey, void* value, int depth, int prefixLen, std::atomic<void*>& nodeLoc);
    std::tuple<void*, bool, bool> searchOpt(std::vector<uint8_t> key, int depth, Node* parent, uint64_t parentVersion);
    std::tuple<Node*, void*, int> findChild(char key);
    int checkPrefix(std::vector<uint8_t> key, int depth);
    bool shouldCompress(Node* parent);
    bool shouldShrink(Node* parent);
    void updatePrefixLeaf(std::vector<uint8_t> key, void* value);
    Node* firstChild();

    bool isFull() {
        switch (type) {
            case tNode4:
                return numChildren == 4;
            case tNode16:
                return numChildren == 16;
            case tNode48:
                return numChildren == 48;
            case tNode256:
                return false;
            case tNodeLeaf:
                return true;
        }
    }

    std::pair<uint64_t, bool> rLock() {
        auto vers = waitUnlock();
        if ((vers&1) == 1) {
            return {0, false};
        }
        return {vers, true};
    }

    bool lockCheck(uint64_t version) {
        return rUnlock(version);
    }

    bool rUnlock(uint64_t version) {
        return version == v.load();
    }

    bool rUnlockWithNode(uint64_t version, Node* lockedNode) {
        if (version != v.load()) {
            lockedNode->unlock();
            return false;
        }
        return true;
    }

    bool upgradeToLock(uint64_t version) {
        return v.compare_exchange_weak(version, version+2);
    }

    bool upgradeToLockWithNode(uint64_t version, Node* lockedNode) {
        if (!v.compare_exchange_weak(version, version+2)) {
            lockedNode->unlock();
            return false;
        }
        return true;
    }

    bool lock() {
        while (true) {
            auto l = rLock(); // version, ok
            if (!l.second) {
                return false;
            }
            if (upgradeToLock(l.first)) {
                break;
            }
        }

        return true;
    }

    void unlock() {
        v.fetch_add(2);
    }

    void unlockObsolete() {
        v.fetch_add(3);
    }

    uint64_t waitUnlock() {
        auto vers = v.load();
        auto count = NSPIN;
        while ((vers&2) == 2) {
            if (count <= 0) {
                std::this_thread::yield();
                count = NSPIN;
            }
            count--;
            vers = v.load();
        }

        return vers;
    }

    virtual void insertChild(char key, void* child);
    virtual void expandInsert(char key, void* child, std::atomic<void*>& nodeLoc);
};

class Node4 : public Node {
public:
    char keys[4];
    std::atomic<void*> children[5];

    Node4() {
        type = tNode4;
    }

    bool removeChildAndShrink(char key, std::atomic<void*>& nodeLoc);
    bool compressChild(int idx, std::atomic<void*>& nodeLoc);
    void expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) override;
    void insertChild(char key, void* child) override {
        int i;
        for (i = 0; i < numChildren; i++) {
            if (key < keys[i]) {
                break;
            }
        }
        memcpy(keys+i+1, keys+i, (3-i)*sizeof(keys[0]));
        memcpy(children+i+1, children+i, (4-i)*sizeof(children[0]));
        keys[i] = key;
        children[i].store(child);
        numChildren++;
    }

};

class Node16 : public Node {
public:
    char keys[16];
    std::atomic<void*> children[16];

    Node16() {
        type = tNode16;
    }

    bool removeChildAndShrink(char key, std::atomic<void*>& nodeLoc);
    void expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) override;

    void insertChild(char key, void* child) override {
        int i;
        for (i = 0; i < numChildren; i++) {
            if (key < keys[i]) {
                break;
            }
        }
        memcpy(keys+i+1, keys+i, 15-i);
        memcpy(children+i+1, children+i, 15-i);
        keys[i] = key;
        children[i].store(child);
        numChildren++;
    }
};

#define node48EmptySlots 0xffff000000000000
#define node48GrowSlots  0xffffffff00000000

static inline int bits_n(int64_t x) {
    return (x != 0) ? std::ceil(std::log(x) / std::log(2)) : 1;
}

class Node48 : public Node {
public:
    int8_t index[256];
    std::atomic<void*> children[48];
    uint64_t slots;

    Node48() {
        type = tNode48;
        slots = node48EmptySlots;
    }

    bool removeChildAndShrink(char key, std::atomic<void*>& nodeLoc);
    void expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) override;

    int allocSlot() {
        uint64_t idx = 48 - bits_n(~slots);
        slots |= 1 << (48 - idx - 1);
        return idx;
    }

    void freeSlot(int idx) {
        slots &= ~(1 << (48 - idx - 1));
    }

    void insertChild(char key, void* child) override {
        auto pos = allocSlot();
        children[pos] = child;
        index[(uint8_t) key] = pos + 1;
        numChildren++;
    }
};

class Node256 : public Node {
public:
    std::atomic<void*> children[256];

    Node256() {
        type = tNode256;
    }

    bool removeChildAndShrink(char key, std::atomic<void*>& nodeLoc);
    void expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) override;

    void insertChild(char key, void* child) override {
        children[(uint8_t) key] = child;
        numChildren++;
    }
};

