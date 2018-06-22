#include "node.hh"

void copyNode(Node* newNode, Node* n) {
    newNode->numChildren = n->numChildren;
    newNode->prefixLen = n->prefixLen;
    memcpy(newNode->prefix, n->prefix, maxPrefixLen*sizeof(n->prefix[0]));
    newNode->prefixLeaf = n->prefixLeaf;
}

void Node4::growAndInsert(char key, void* child, std::atomic<void*> nodeLoc) {
    Node16* newNode = new Node16();
    memcpy(newNode->keys, keys, 4*sizeof(keys[0]));
    memcpy(newNode->children, children, 5*sizeof(children[0]));
    copyNode(newNode, this);
    newNode->insertChild(key, child);
    nodeLoc.store(newNode);
}

void Node16::growAndInsert(char key, void* child, std::atomic<void*> nodeLoc) {
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

void Node48::growAndInsert(char key, void* child, std::atomic<void*> nodeLoc) {
    Node256* newNode = new Node256();
    for (int i = 0; i < 48; i++) {
        auto idx = index[i];
        if (idx > 0) {
            newNode->children[i] = children[idx-1];
        }
    }
    copyNode(newNode, this);
    newNode->insertChild(key, child);
    nodeLoc.store(newNode);
}

void Node256::growAndInsert(char key, void* child, std::atomic<void*> nodeLoc) {
    insertChild(key, child);
}

std::vector<uint8_t> slice(const std::vector<uint8_t>& v, int start=0, int end=-1) {
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

void Leaf::updateOrExpand(std::vector<uint8_t> key, void* value, int depth, std::atomic<void*> nodeLoc) {
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
    auto l = this->prefixLeaf;
    if (!l) {
        // std::atomic_store(this->prefixLeaf, new Leaf(key, value));
    } else {
        l->value = value;
    }
}

bool shouldShrink(Node* parent) {
    switch (type) {
        case tNode4:
            if (!parent) {
                return false;
            }
    }
}
