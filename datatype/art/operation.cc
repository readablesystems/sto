#include <cmath>

#include "node.hh"

int Node::checkPrefix(std::vector<uint8_t> key, int depth) {
    if (prefixLen == 0) {
        return 0;
    }

    auto l = std::min((int) key.size()-depth, std::min(prefixLen, maxPrefixLen));

    int idx;
    for (idx = 0; idx < l; idx++) {
        if (prefix[idx] != key[depth+idx]) {
            return idx;
        }
    }
    return idx;
}

std::tuple<Node*, void*, int> Node::findChild(char key) {
    switch (type) {
        case tNode4: {
            Node4* n4 = (Node4*) this;
            for (int i = 0; i < n4->numChildren; i++) {
                if (n4->keys[i] == key) {
                    return {(Node*) n4->children[i].load(), &n4->children[i], i};
                }
            }
            break;
        }
        case tNode16: {
            Node16* n16 = (Node16*) this;
            for (int i = 0; i < n16->numChildren; i++) {
                if (n16->keys[i] == key) {
                    return {(Node*) n16->children[i].load(), &n16->children[i], i};
                }
            }
            break;
        }
        case tNode48: {
            Node48* n48 = (Node48*) this;
            auto idx = n48->index[(uint8_t) key];
            if (idx > 0) {
                return {(Node*) n48->children[idx-1].load(), &n48->children[idx-1], key};
            }
            break;
        }
        case tNode256: {
            Node256* n256 = (Node256*) this;
            return {(Node*) n256->children[(uint8_t) key].load(), &n256->children[(uint8_t) key], key};
            break;
        }
    }

    return {0, 0, 0};
}

std::tuple<void*, bool, bool> Node::searchOpt(std::vector<uint8_t> key, int depth, Node* parent, uint64_t parentVersion) {
    uint64_t version;
    bool ok;

    Node* n = this;

RECUR:
    auto l = n->rLock();
    version = l.first;
    ok = l.second;
    if (!ok) {
        return {nullptr, false, false};
    }
    if (!parent->rUnlock(parentVersion)) {
        return {nullptr, false, false};
    }

    if (n->checkPrefix(key, depth) != std::min(n->prefixLen, maxPrefixLen)) {
        if (!n->rUnlock(version)) {
            return {nullptr, false, false};
        }
        return {nullptr, false, false};
    }

    depth += n->prefixLen;

    if (depth == key.size()) {
        Leaf* l = n->prefixLeaf.load();
        void* value;
        bool ex;
        if (l && l->match(key)) {
            value = l->value;
            ex = true;
        }
        if (!n->rUnlock(version)) {
            return {nullptr, false, false};
        }
        return {value, ex, true};
    }

    if (depth > key.size()) {
        return {nullptr, false, n->rUnlock(version)};
    }

    auto ret = n->findChild(key[depth]);
    auto nextNode = std::get<0>(ret);
    if (!n->lockCheck(version)) {
        return {nullptr, false, false};
    }

    if (!nextNode) {
        if (!n->rUnlock(version)) {
            return {nullptr, false, false};
        }
        return {nullptr, false, true};
    }

    if (nextNode->type == tNodeLeaf) {
        Leaf* l = (Leaf*) nextNode;
        void* value;
        bool ex;
        if (l->match(key)) {
            value = l->value;
            ex = true;
        }
        if (!n->rUnlock(version)) {
            return {nullptr, false, false};
        }
        return {value, ex, true};
    }

    depth += 1;
    parent = n;
    parentVersion = version;
    n = nextNode;
    goto RECUR;
}

void Node::insertSplitPrefix(std::vector<uint8_t> key, std::vector<uint8_t> fullKey, void* value, int depth, int prefixLen, std::atomic<void*>& nodeLoc) {
    Node4* newNode = new Node4();
    depth += prefixLen;
    if (key.size() == depth) {
        newNode->prefixLeaf.store(new Leaf(key, value));
    } else {
        newNode->insertChild(key[depth], new Leaf(key, value));
    }

    newNode->prefixLen = prefixLen;
    memcpy(newNode->prefix, this->prefix, std::min(maxPrefixLen, prefixLen));
    if (this->prefixLen <= maxPrefixLen) {
        newNode->insertChild(prefixLen, this);
        prefixLen -= prefixLen + 1;
        int len1 = std::min(maxPrefixLen, this->prefixLen);
        int len2 = maxPrefixLen - (prefixLen + 1);
        memcpy(this->prefix, this->prefix + prefixLen + 1, std::min(len1, len2));
    } else {
        newNode->insertChild(fullKey[depth+prefixLen], this);
        prefixLen -= prefixLen + 1;
        auto s = slice(fullKey, depth+prefixLen+1, fullKey.size());
        int len1 = std::min(maxPrefixLen, this->prefixLen);
        int len2 = fullKey.size() - (depth + prefixLen + 1);
        memcpy(this->prefix, s.data(), std::min(len1, len2));
    }
    nodeLoc.store(newNode);
}

std::pair<std::vector<uint8_t>, bool> Node::fullKey(uint64_t version) {
    Leaf* l = this->prefixLeaf.load();
    if (l) {
        if (!rUnlock(version)) {
            return {std::vector<uint8_t>(), false};
        }
        return {l->key, true};
    }

    auto next = this->firstChild();
    if (!this->lockCheck(version)) {
        return {std::vector<uint8_t>(), false};
    }

    if (next->type == tNodeLeaf) {
        Leaf* l = (Leaf*) next;
        if (!rUnlock(version)) {
            return {std::vector<uint8_t>(), false};
        }
        return {l->key, true};
    }

    auto ret = next->rLock();
    if (!ret.second) {
        return {std::vector<uint8_t>(), false};
    }
    return next->fullKey(ret.first);
}

std::tuple<int, std::vector<uint8_t>, bool> Node::prefixMismatch(std::vector<uint8_t> key, int depth, Node* parent, uint64_t version, uint64_t parentVersion) {
    auto empty = std::vector<uint8_t>();
    if (prefixLen <= maxPrefixLen) {
        return {checkPrefix(key, depth), empty, true};
    }

    std::vector<uint8_t> fKey;
    bool ok = false;
    while (true) {
        if (!lockCheck(version) || !parent->lockCheck(parentVersion)) {
            return {0, empty, false};
        }
        if (ok) {
            break;
        }
        auto ret = fullKey(version);
        fKey = ret.first;
        ok = ret.second;
    }

    int i = depth;
    auto l = std::min((int) key.size(), depth+prefixLen);
    for (; i < l; i++) {
        if (key[i] != fKey[i]) {
            break;
        }
    }
    return {i - depth, fKey, true};
}

bool Node::insertOpt(std::vector<uint8_t> key, void* value, int depth, Node* parent, uint64_t parentVersion, std::atomic<void*>& nodeLoc) {
    uint64_t version;
    Node* nextNode;
    std::atomic<void*> nextLoc;
    Node* n = this;

RECUR:
    auto ret = n->rLock();
    version = ret.first;
    if (!ret.second) {
        return false;
    }

    auto mismatch = n->prefixMismatch(key, depth, parent, version, parentVersion);
    if (!std::get<2>(mismatch)) {
        return false;
    }

    auto p = std::get<0>(mismatch);
    auto fKey = std::get<1>(mismatch);

    if (p != n->prefixLen) {
        if (!parent->upgradeToLock(parentVersion)) {
            return false;
        }

        if (!n->upgradeToLockWithNode(version, parent)) {
            return false;
        }
        n->insertSplitPrefix(key, fKey, value, depth, p, nodeLoc);
        n->unlock();
        parent->unlock();
        return true;
    }
    depth += n->prefixLen;
    if (depth == key.size()) {
        if (!n->upgradeToLock(version)) {
            return false;
        }
        if (!parent->rUnlockWithNode(parentVersion, n)) {
            return false;
        }
        n->updatePrefixLeaf(key, value);
        n->unlock();
        return true;
    }

    auto child = n->findChild(key[depth]);
    if (!n->lockCheck(version)) {
        return false;
    }

    nextNode = std::get<0>(child);
    nextLoc = std::get<1>(child);

    if (!nextNode) {
        if (n->isFull()) {
            if (!parent->upgradeToLock(parentVersion)) {
                return false;
            }
            if (!n->upgradeToLockWithNode(version, parent)) {
                return false;
            }
            n->expandInsert(key[depth], new Leaf(key, value), nodeLoc);
            n->unlockObsolete();
            parent->unlock();
        } else {
            if (!n->upgradeToLock(version)) {
                return false;
            }
            if (!parent->rUnlockWithNode(parentVersion, n)) {
                return false;
            }
            n->insertChild(key[depth], new Leaf(key, value));
            n->unlock();
        }
        return true;
    }

    if (!parent->rUnlock(parentVersion)) {
        return false;
    }
    
    if (nextNode->type == tNodeLeaf) {
        if (!n->upgradeToLock(version)) {
            return false;
        }
        auto l = (Leaf*) nextNode;
        l->updateOrExpand(key, value, depth+1, nextLoc);
        n->unlock();
        return true;
    }

    depth += 1;
    parent = n;
    parentVersion = version;
    nodeLoc.store(nextLoc);
    n = nextNode;
    goto RECUR;
}
