#include "node.hh"

void copyNode(Node* newNode, Node* n) {
    newNode->numChildren = n->numChildren;
    newNode->prefixLen = n->prefixLen;
    memcpy(newNode->prefix, n->prefix, maxPrefixLen*sizeof(n->prefix[0]));
    newNode->prefixLeaf.store(n->prefixLeaf);
}

void Node4::expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) {
    Node16* newNode = new Node16();
    memcpy(newNode->keys, keys, 4*sizeof(keys[0]));
    memcpy(newNode->children, children, 5*sizeof(children[0]));
    copyNode(newNode, this);
    newNode->insertChild(key, child);
    nodeLoc.store(newNode);
}

void Node16::expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) {
    Node48* newNode = new Node48();
    memcpy(newNode->children, children, 16*sizeof(children[0]));
    newNode->slots = node48GrowSlots;
    for (int i = 0; i < 16; i++) {
        newNode->index[(uint8_t) keys[i]] = i + 1;
    }
    copyNode(newNode, this);
    newNode->insertChild(key, child);
    nodeLoc.store(newNode);
}

void Node48::expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) {
    Node256* newNode = new Node256();
    for (int i = 0; i < 48; i++) {
        auto idx = index[i];
        if (idx > 0) {
            newNode->children[i] = children[idx-1].load();
        }
    }
    copyNode(newNode, this);
    newNode->insertChild(key, child);
    nodeLoc.store(newNode);
}

void Node256::expandInsert(char key, void* child, std::atomic<void*>& nodeLoc) {
    insertChild(key, child);
}

void Leaf::updateOrExpand(std::vector<uint8_t> key, void* value, int depth, std::atomic<void*>& nodeLoc) {
    if (match(key)) {
        this->value = value;
        return;
    }

    unsigned i;
    unsigned prefixLen = std::min(this->key.size(), key.size());
    Node4* newNode = new Node4();
    for (i = depth; i < prefixLen; i++) {
        if (this->key[i] != key[i]) {
            break;
        }
    }
    newNode->prefixLen = i-depth;
    auto s = slice(key, depth, i);
    memcpy(newNode->prefix, s.data(), std::min((int) (s.size()), maxPrefixLen));
    if (i == this->key.size()) {
        newNode->prefixLeaf = this;
    } else {
        newNode->insertChild(this->key[i], this);
    }
    if (i == key.size()) {
        newNode->prefixLeaf = new Leaf(key, value);
    } else {
        newNode->insertChild(key[i], new Leaf(key, value));
    }
    nodeLoc.store(newNode);
}

void Node::updatePrefixLeaf(std::vector<uint8_t> key, void* value) {
    Leaf* l = this->prefixLeaf;
    if (!l) {
        this->prefixLeaf.store(new Leaf(key, value));
    } else {
        l->value = value;
    }
}

bool Node::shouldShrink(Node* parent) {
    switch (type) {
        case tNode4:
            if (!parent) {
                return false;
            }
            if (!prefixLeaf.load()) {
                return numChildren <= 2;
            } else {
                return numChildren <= 1;
            }
        case tNode16:
            return numChildren <= node16MinSize;
        case tNode48:
            return numChildren <= node48MinSize;
        case tNode256:
            return numChildren > 0 && numChildren <= node256MinSize;
        default:
            printf("Pls no\n");
            return false;
    }
}

bool Node4::removeChildAndShrink(char key, std::atomic<void*>& nodeLoc) {
    if (prefixLeaf != nullptr) {
        nodeLoc.store(prefixLeaf);
        return true;
    }

    for (int i = 0; i < numChildren; i++) {
        if (keys[i] != key) {
            return compressChild(i, nodeLoc);
        }
    }

    return false; // unreachable
}

bool Node4::compressChild(int idx, std::atomic<void*>& nodeLoc) {
    Node* child = (Node*) children[idx].load();
    if (child->type != tNodeLeaf) {
        if (!child->lock()) {
            return false;
        }
        int32_t prefixLen = this->prefixLen;
        if (prefixLen < maxPrefixLen) {
            prefix[prefixLen] = keys[idx];
            prefixLen++;
        }
        if (prefixLen < maxPrefixLen) {
            auto subPrefixLen = std::min(child->prefixLen, maxPrefixLen-prefixLen);
            memcpy(prefix, child->prefix + subPrefixLen, maxPrefixLen - subPrefixLen);
            prefixLen += subPrefixLen;
        }

        memcpy(child->prefix, prefix + std::min(prefixLen, maxPrefixLen), maxPrefixLen);
        child->prefixLen += this->prefixLen + 1;
        child->unlock();
    }
    nodeLoc.store(child);
    return true;
}

bool Node16::removeChildAndShrink(char key, std::atomic<void*>& nodeLoc) {
    Node4* newNode = new Node4();
    int idx = 0;
    for (int i = 0; i < numChildren; i++) {
        if (keys[i] != key) {
            newNode->keys[idx] = keys[i];
            newNode->children[idx].store(children[i]);
            idx++;
        }
    }
    copyNode(newNode, this);
    newNode->numChildren = node16MinSize - 1;
    nodeLoc.store(newNode);
    return true;
}

bool Node48::removeChildAndShrink(char key, std::atomic<void*>& nodeLoc) {
    Node16* newNode = new Node16();
    int idx = 0;
    for (int i = 0; i < 256; i++) {
        if (i != key && index[i] != 0) {
            newNode->keys[idx] = i;
            newNode->children[idx].store(children[index[i]-1]);
            idx++;
        }
    }
    copyNode(newNode, this);
    newNode->numChildren = node48MinSize - 1;
    nodeLoc.store(newNode);
    return true;
}

bool Node256::removeChildAndShrink(char key, std::atomic<void*>& nodeLoc) {
    Node48* newNode = new Node48();
    for (int i = 0; i < 256; i++) {
        if (i != key && children[i] != nullptr) {
            auto pos = newNode->allocSlot();
            newNode->index[i] = pos + 1;
            newNode->children[pos].store(children[i]);
        }
    }
    copyNode(newNode, this);
    newNode->numChildren = node256MinSize - 1;
    nodeLoc.store(newNode);
    return true;
}

bool Node::shouldCompress(Node* parent) {
    if (type == tNode4) {
        return numChildren == 1 && !parent;
    }
    return false;
}

Node* Node::firstChild() {
    switch (type) {
    case tNode4: {
        Node4* n4 = (Node4*) this;
        return (Node*) n4->children[0].load();
    }
    case tNode16: {
        Node16* n16 = (Node16*) this;
        return (Node*) n16->children[0].load();
    }
    case tNode48: {
        Node48* n48 = (Node48*) this;
        for (int i = 0; i < 256; i++) {
            auto pos = n48->index[i];
            if (pos == 0) {
                continue;
            }
            auto c = n48->children[pos-1].load();
            if (c) {
                return (Node*) c;
            }
        }
    }
    case tNode256: {
        Node256* n256 = (Node256*) this;
        for (int i = 0; i < 256; i++) {
            auto c = n256->children[i].load();
            if (c) {
                return (Node*) c;
            }
        }
    }
    }
}
