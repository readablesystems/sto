#include <assert.h>
#include <algorithm>
#include "Tree.h"
#include "N.cpp"
#include "Key.h"
#include "Sto.hh"
#include "Transaction.hh"

namespace ART_OLC {

    Tree::Tree() : root(new N256(nullptr, 0)) {
    }

    void Tree::setLoadKey(LoadKeyFunction f) {
        loadKey = f;
    }

    Tree::Tree(LoadKeyFunction loadKey) : root(new N256(nullptr, 0)), loadKey(loadKey) {
    }

    Tree::~Tree() {
        N::deleteChildren(root);
        N::deleteNode(root);
    }

    // ThreadInfo Tree::getThreadInfo() {
    //     return ThreadInfo(this->epoche);
    // }

    std::pair<TID, N*> Tree::lookup(const Key &k) const {
        // EpocheGuardReadonly epocheGuard(threadEpocheInfo);
        restart:
        bool needRestart = false;

        N *node;
        N *parentNode = nullptr;
        uint64_t v;
        uint32_t level = 0;
        bool optimisticPrefixMatch = false;

        node = root;
        v = node->readLockOrRestart(needRestart);
        if (needRestart) goto restart;
        while (true) {
            switch (checkPrefix(node, k, level)) { // increases level
                case CheckPrefixResult::NoMatch:
                    node->readUnlockOrRestart(v, needRestart);
                    if (needRestart) goto restart;
                    return {0, node};
                case CheckPrefixResult::OptimisticMatch:
                    optimisticPrefixMatch = true;
                    // fallthrough
                case CheckPrefixResult::Match:
                    if (k.getKeyLen() <= level) {
                        return {0, node};
                    }
                    parentNode = node;
                    node = N::getChild(k[level], parentNode);
                    parentNode->checkOrRestart(v,needRestart);
                    if (needRestart) goto restart;

                    if (node == nullptr) {
                        return {0, parentNode};
                    }
                    if (N::isLeaf(node)) {
                        parentNode->readUnlockOrRestart(v, needRestart);
                        if (needRestart) goto restart;

                        TID tid = N::getLeaf(node);
                        if (level < k.getKeyLen() - 1 || optimisticPrefixMatch) {
                            return {checkKey(tid, k), parentNode};
                        }
                        return {tid, parentNode};
                    }
                    level++;
            }
            uint64_t nv = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;

            parentNode->readUnlockOrRestart(v, needRestart);
            if (needRestart) goto restart;
            v = nv;
        }
    }

    bool Tree::lookupRange(const Key &start, const Key &end, Key &continueKey, TID result[],
                                std::size_t resultSize, std::size_t &resultsFound, std::function<void(N*)> observe_node) const {
        for (uint32_t i = 0; i < std::min(start.getKeyLen(), end.getKeyLen()); ++i) {
            if (start[i] > end[i]) {
                resultsFound = 0;
                return false;
            } else if (start[i] < end[i]) {
                break;
            }
        }
        // EpocheGuard epocheGuard(threadEpocheInfo);
        TID toContinue = 0;
        std::function<void(const N *)> copy = [&result, &resultSize, &resultsFound, &toContinue, &copy](const N *node) {
            if (N::isLeaf(node)) {
                if (resultsFound == resultSize) {
                    toContinue = N::getLeaf(node);
                    return;
                }
                result[resultsFound] = N::getLeaf(node);
                resultsFound++;
            } else {
                std::tuple<uint8_t, N *> children[256];
                uint32_t childrenCount = 0;
                N::getChildren(node, 0u, 255u, children, childrenCount);
                for (uint32_t i = 0; i < childrenCount; ++i) {
                    const N *n = std::get<1>(children[i]);
                    copy(n);
                    if (toContinue != 0) {
                        break;
                    }
                }
            }
        };
        std::function<void(N *, uint8_t, uint32_t, const N *, uint64_t)> findStart = [&copy, &start, &findStart, &toContinue, this](
                N *node, uint8_t nodeK, uint32_t level, const N *parentNode, uint64_t vp) {
            if (N::isLeaf(node)) {
                copy(node);
                return;
            }
            uint64_t v;
            PCCompareResults prefixResult;

            {
                readAgain:
                bool needRestart = false;
                v = node->readLockOrRestart(needRestart);
                if (needRestart) goto readAgain;

                prefixResult = checkPrefixCompare(node, start, 0, level, loadKey, needRestart);
                if (needRestart) goto readAgain;

                parentNode->readUnlockOrRestart(vp, needRestart);
                if (needRestart) {
                    readParentAgain:
                    vp = parentNode->readLockOrRestart(needRestart);
                    if (needRestart) goto readParentAgain;

                    node = N::getChild(nodeK, parentNode);

                    parentNode->readUnlockOrRestart(vp, needRestart);
                    if (needRestart) goto readParentAgain;

                    if (node == nullptr) {
                        return;
                    }
                    if (N::isLeaf(node)) {
                        copy(node);
                        return;
                    }
                    goto readAgain;
                }
                node->readUnlockOrRestart(v, needRestart);
                if (needRestart) goto readAgain;
            }

            switch (prefixResult) {
                case PCCompareResults::Bigger:
                    copy(node);
                    break;
                case PCCompareResults::Equal: {
                    uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                    std::tuple<uint8_t, N *> children[256];
                    uint32_t childrenCount = 0;
                    v = N::getChildren(node, startLevel, 255, children, childrenCount);
                    for (uint32_t i = 0; i < childrenCount; ++i) {
                        const uint8_t k = std::get<0>(children[i]);
                        N *n = std::get<1>(children[i]);
                        if (k == startLevel) {
                            findStart(n, k, level + 1, node, v);
                        } else if (k > startLevel) {
                            copy(n);
                        }
                        if (toContinue != 0) {
                            break;
                        }
                    }
                    break;
                }
                case PCCompareResults::Smaller:
                    break;
            }
        };
        std::function<void(N *, uint8_t, uint32_t, const N *, uint64_t)> findEnd = [&copy, &end, &toContinue, &findEnd, this](
                N *node, uint8_t nodeK, uint32_t level, const N *parentNode, uint64_t vp) {
            if (N::isLeaf(node)) {
                return;
            }
            uint64_t v;
            PCCompareResults prefixResult;
            {
                readAgain:
                bool needRestart = false;
                v = node->readLockOrRestart(needRestart);
                if (needRestart) goto readAgain;

                prefixResult = checkPrefixCompare(node, end, 255, level, loadKey, needRestart);
                if (needRestart) goto readAgain;

                parentNode->readUnlockOrRestart(vp, needRestart);
                if (needRestart) {
                    readParentAgain:
                    vp = parentNode->readLockOrRestart(needRestart);
                    if (needRestart) goto readParentAgain;

                    node = N::getChild(nodeK, parentNode);

                    parentNode->readUnlockOrRestart(vp, needRestart);
                    if (needRestart) goto readParentAgain;

                    if (node == nullptr) {
                        return;
                    }
                    if (N::isLeaf(node)) {
                        return;
                    }
                    goto readAgain;
                }
                node->readUnlockOrRestart(v, needRestart);
                if (needRestart) goto readAgain;
            }
            switch (prefixResult) {
                case PCCompareResults::Smaller:
                    copy(node);
                    break;
                case PCCompareResults::Equal: {
                    uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 255;
                    std::tuple<uint8_t, N *> children[256];
                    uint32_t childrenCount = 0;
                    v = N::getChildren(node, 0, endLevel, children, childrenCount);
                    for (uint32_t i = 0; i < childrenCount; ++i) {
                        const uint8_t k = std::get<0>(children[i]);
                        N *n = std::get<1>(children[i]);
                        if (k == endLevel) {
                            findEnd(n, k, level + 1, node, v);
                        } else if (k < endLevel) {
                            copy(n);
                        }
                        if (toContinue != 0) {
                            break;
                        }
                    }
                    break;
                }
                case PCCompareResults::Bigger:
                    break;
            }
        };

        restart:
        bool needRestart = false;

        resultsFound = 0;

        uint32_t level = 0;
        N *node = nullptr;
        N *nextNode = root;
        N *parentNode;
        uint64_t v = 0;
        uint64_t vp;

        while (true) {
            parentNode = node;
            vp = v;
            node = nextNode;
            PCEqualsResults prefixResult;
            v = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
            prefixResult = checkPrefixEquals(node, level, start, end, loadKey, needRestart);
            if (needRestart) goto restart;
            if (parentNode != nullptr) {
                parentNode->readUnlockOrRestart(vp, needRestart);
                if (needRestart) goto restart;
            }
            observe_node(node);
            node->readUnlockOrRestart(v, needRestart);
            if (needRestart) goto restart;

            switch (prefixResult) {
                case PCEqualsResults::NoMatch: {
                    return false;
                }
                case PCEqualsResults::Contained: {
                    copy(node);
                    break;
                }
                case PCEqualsResults::BothMatch: {
                    uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                    uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 255;
                    if (startLevel != endLevel) {
                        std::tuple<uint8_t, N *> children[256];
                        uint32_t childrenCount = 0;
                        v = N::getChildren(node, startLevel, endLevel, children, childrenCount);
                        for (uint32_t i = 0; i < childrenCount; ++i) {
                            const uint8_t k = std::get<0>(children[i]);
                            N *n = std::get<1>(children[i]);
                            if (k == startLevel) {
                                findStart(n, k, level + 1, node, v);
                            } else if (k > startLevel && k < endLevel) {
                                copy(n);
                            } else if (k == endLevel) {
                                findEnd(n, k, level + 1, node, v);
                            }
                            if (toContinue) {
                                break;
                            }
                        }
                    } else {
                        nextNode = N::getChild(startLevel, node);
                        node->readUnlockOrRestart(v, needRestart);
                        if (needRestart) goto restart;
                        level++;
                        continue;
                    }
                    break;
                }
            }
            break;
        }
        if (toContinue != 0) {
            loadKey(toContinue, continueKey);
            return true;
        } else {
            return false;
        }
    }


    TID Tree::checkKey(const TID tid, const Key &k) const {
        Key kt;
        this->loadKey(tid, kt);
        if (k == kt) {
            return tid;
        }
        return 0;
    }

    // not thread safe
    #include <vector>
    static void printPrefix(std::tuple<uint8_t, N*> node) {
        printf("%c", std::get<0>(node));
        auto n = std::get<1>(node);
        if (n->hasPrefix()) {
            const char* prefix = (const char*) n->getPrefix();
            int prefixLen = n->getPrefixLength();
            for (int i = 0; i < prefixLen; i++) {
                printf("%c", prefix[i]);
            }
        }
    }
    void Tree::print() const {
        std::vector<std::tuple<uint8_t, N*>> parents = {std::make_tuple(0, root)};
        std::vector<std::tuple<uint8_t, N*>> childrenVec;

        uint32_t level = 0;

        while (parents.size()) {
            // get all kids
            for (std::tuple<uint8_t, N*> parent : parents) {
                N* par = std::get<1>(parent);
                printf("%d: ", level);
                printPrefix(parent);
                printf("\n");
                std::tuple<uint8_t, N*> children[256];
                uint32_t childrenCount = 0;
                N::getChildren(par, 0u, 255u, children, childrenCount);
                for (int i = 0; i < level; ++i) {
                    printf("\t");
                }
                for (uint32_t i = 0; i < childrenCount; ++i) {
                    N* child = std::get<1>(children[i]);
                    if (child) {
                        if (N::isLeaf(child)) {
                            Key k;
                            loadKey(N::getLeaf(child), k);
                            printf("%s ", k.data);
                        } else {
                            printf("\"");
                            printPrefix(children[i]);
                            printf("\" ");
                            childrenVec.push_back(children[i]);
                        }
                    }
                }
                printf("\n");
            }

            // make kids into parents
            parents = childrenVec;
            childrenVec.clear();
            ++level;
        }
    }

    Tree::ins_return_type Tree::insert(const Key &k, std::function<TID()> make_tid) {
        // EpocheGuard epocheGuard(epocheInfo);
        restart:
        bool needRestart = false;

        N *node = nullptr;
        N *nextNode = root;
        N *parentNode = nullptr;
        uint8_t parentKey, nodeKey = 0;
        uint64_t parentVersion = 0;
        uint32_t level = 0;
        TID tid = 0;

        while (true) {
            parentNode = node;
            parentKey = nodeKey;
            node = nextNode;
            auto v = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;

            uint32_t nextLevel = level;

            uint8_t nonMatchingKey;
            Prefix remainingPrefix;
            auto res = checkPrefixPessimistic(node, k, nextLevel, nonMatchingKey, remainingPrefix,
                                   this->loadKey, needRestart); // increases level
            if (needRestart) goto restart;
            switch (res) {
                case CheckPrefixPessimisticResult::NoMatch: {
                    parentNode->upgradeToWriteLockOrRestart(parentVersion, needRestart);
                    if (needRestart) goto restart;

                    node->upgradeToWriteLockOrRestart(v, needRestart);
                    if (needRestart) {
                        parentNode->writeUnlock();
                        goto restart;
                    }
                    // 1) Create new node which will be parent of node, Set common prefix, level to this node
                    auto newNode = new N4(node->getPrefix(), nextLevel - level);

                    // 2)  add node and (tid, *k) as children
                    tid = make_tid();
                    newNode->insert(k[nextLevel], N::setLeaf(tid));
                    newNode->insert(nonMatchingKey, node);

                    // 3) upgradeToWriteLockOrRestart, update parentNode to point to the new node, unlock
                    N::change(parentNode, parentKey, newNode);
                    parentNode->writeUnlock();

                    // 4) update prefix of node, unlock
                    node->setPrefix(remainingPrefix,
                                    node->getPrefixLength() - ((nextLevel - level) + 1));

                    node->writeUnlock();
                    return ins_return_type(tid, parentNode, node);
                }
                case CheckPrefixPessimisticResult::Match:
                    break;
            }
            // if (nextLevel >= k.getKeyLen()) {
            //     node->readUnlockOrRestart(v, needRestart);
            //     if (needRestart) goto restart;
            //     if (parentNode != nullptr) {
            //         parentNode->readUnlockOrRestart(parentVersion, needRestart);
            //         if (needRestart) goto restart;
            //     }
            //     return {0, ;
            // }
            level = nextLevel;
            nodeKey = k[level];
            nextNode = N::getChild(nodeKey, node);
            node->checkOrRestart(v,needRestart);
            if (needRestart) goto restart;

            if (nextNode == nullptr) {
                if (!tid) tid = make_tid();
                N* newNode = N::insertAndUnlock(node, v, parentNode, parentVersion, parentKey, nodeKey, N::setLeaf(tid), needRestart);
                if (needRestart) goto restart;
                if (newNode) {
                    return ins_return_type(tid, newNode, node);
                }
                return ins_return_type(tid, node, nullptr);
                // return ins_return_type(tid, newNode == nullptr ? node : newNode, newNode == nullptr ? nullptr : node);
            }

            if (parentNode != nullptr) {
                parentNode->readUnlockOrRestart(parentVersion, needRestart);
                if (needRestart) goto restart;
            }

            if (N::isLeaf(nextNode)) {
                node->upgradeToWriteLockOrRestart(v, needRestart);
                if (needRestart) goto restart;

                Key key;
                loadKey(N::getLeaf(nextNode), key);

                level++;
                uint32_t prefixLength = 0;
                while (key[level + prefixLength] == k[level + prefixLength]) {
                    prefixLength++;
                    if (level + prefixLength >= k.getKeyLen() || level + prefixLength >= key.getKeyLen()) {
                        node->writeUnlock();
                        return ins_return_type(N::getLeaf(nextNode), nullptr, nullptr);
                    }
                }

                auto n4 = new N4(&k[level], prefixLength);
                tid = make_tid();
                n4->insert(k[level + prefixLength], N::setLeaf(tid));
                n4->insert(key[level + prefixLength], nextNode);
                N::change(node, k[level - 1], n4);
                node->writeUnlock();
                return ins_return_type(tid, node, nullptr);
            }
            level++;
            parentVersion = v;
        }
    }

    N* Tree::remove(const Key &k, TID tid) {
        // EpocheGuard epocheGuard(threadInfo);
        restart:
        bool needRestart = false;

        N *node = nullptr;
        N *nextNode = root;
        N *parentNode = nullptr;
        uint8_t parentKey, nodeKey = 0;
        uint64_t parentVersion = 0;
        uint32_t level = 0;

        while (true) {
            parentNode = node;
            parentKey = nodeKey;
            node = nextNode;
            auto v = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;

            switch (checkPrefix(node, k, level)) { // increases level
                case CheckPrefixResult::NoMatch:
                    node->readUnlockOrRestart(v, needRestart);
                    if (needRestart) goto restart;
                    return nullptr;
                case CheckPrefixResult::OptimisticMatch:
                    // fallthrough
                case CheckPrefixResult::Match: {
                    nodeKey = k[level];
                    nextNode = N::getChild(nodeKey, node);

                    node->checkOrRestart(v, needRestart);
                    if (needRestart) goto restart;

                    if (nextNode == nullptr) {
                        node->readUnlockOrRestart(v, needRestart);
                        if (needRestart) goto restart;
                        return nullptr;
                    }
                    if (N::isLeaf(nextNode)) {
                        if (N::getLeaf(nextNode) != tid) {
                            return nullptr;
                        }
                        assert(parentNode == nullptr || node->getCount() != 1);
                        if (node->getCount() == 2 && parentNode != nullptr) {
                            parentNode->upgradeToWriteLockOrRestart(parentVersion, needRestart);
                            if (needRestart) goto restart;

                            node->upgradeToWriteLockOrRestart(v, needRestart);
                            if (needRestart) {
                                parentNode->writeUnlock();
                                goto restart;
                            }
                            // 1. check remaining entries
                            N *secondNodeN;
                            uint8_t secondNodeK;
                            std::tie(secondNodeN, secondNodeK) = N::getSecondChild(node, nodeKey);
                            if (N::isLeaf(secondNodeN)) {
                                //N::remove(node, k[level]); not necessary
                                N::change(parentNode, parentKey, secondNodeN);

                                parentNode->writeUnlock();
                                node->writeUnlockObsolete();
                                return node;
                                // Transaction::rcu_delete(node);
                                // this->epoche.markNodeForDeletion(node, threadInfo);
                            } else {
                                secondNodeN->writeLockOrRestart(needRestart);
                                if (needRestart) {
                                    node->writeUnlock();
                                    parentNode->writeUnlock();
                                    goto restart;
                                }

                                //N::remove(node, k[level]); not necessary
                                N::change(parentNode, parentKey, secondNodeN);
                                parentNode->writeUnlock();

                                secondNodeN->addPrefixBefore(node, secondNodeK);
                                secondNodeN->writeUnlock();

                                node->writeUnlockObsolete();
                                return node;
                                // Transaction::rcu_delete(node);
                                // this->epoche.markNodeForDeletion(node, threadInfo);
                            }
                        } else {
                            auto old = N::removeAndUnlock(node, v, k[level], parentNode, parentVersion, parentKey, needRestart);
                            if (needRestart) goto restart;
                            return old;
                        }
                        return nullptr;
                    }
                    level++;
                    parentVersion = v;
                }
            }
        }
    }

    inline typename Tree::CheckPrefixResult Tree::checkPrefix(N *n, const Key &k, uint32_t &level) {
        if (n->hasPrefix()) {
            if (k.getKeyLen() <= level + n->getPrefixLength()) {
                return CheckPrefixResult::NoMatch;
            }
            for (uint32_t i = 0; i < std::min(n->getPrefixLength(), maxStoredPrefixLength); ++i) {
                if (n->getPrefix()[i] != k[level]) {
                    return CheckPrefixResult::NoMatch;
                }
                ++level;
            }
            if (n->getPrefixLength() > maxStoredPrefixLength) {
                level = level + (n->getPrefixLength() - maxStoredPrefixLength);
                return CheckPrefixResult::OptimisticMatch;
            }
        }
        return CheckPrefixResult::Match;
    }

    typename Tree::CheckPrefixPessimisticResult Tree::checkPrefixPessimistic(N *n, const Key &k, uint32_t &level,
                                                                        uint8_t &nonMatchingKey,
                                                                        Prefix &nonMatchingPrefix,
                                                                        LoadKeyFunction loadKey, bool &needRestart) {
        if (n->hasPrefix()) {
            uint32_t prevLevel = level;
            Key kt;
            for (uint32_t i = 0; i < n->getPrefixLength(); ++i) {
                if (i == maxStoredPrefixLength) {
                    auto anyTID = N::getAnyChildTid(n, needRestart);
                    if (needRestart) return CheckPrefixPessimisticResult::Match;
                    loadKey(anyTID, kt);
                }
                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : n->getPrefix()[i];
                if (curKey != k[level]) {
                    nonMatchingKey = curKey;
                    if (n->getPrefixLength() > maxStoredPrefixLength) {
                        if (i < maxStoredPrefixLength) {
                            auto anyTID = N::getAnyChildTid(n, needRestart);
                            if (needRestart) return CheckPrefixPessimisticResult::Match;
                            loadKey(anyTID, kt);
                        }
                        memcpy(nonMatchingPrefix, &kt[0] + level + 1, std::min((n->getPrefixLength() - (level - prevLevel) - 1),
                                                                           maxStoredPrefixLength));
                    } else {
                        memcpy(nonMatchingPrefix, n->getPrefix() + i + 1, n->getPrefixLength() - i - 1);
                    }
                    return CheckPrefixPessimisticResult::NoMatch;
                }
                ++level;
            }
        }
        return CheckPrefixPessimisticResult::Match;
    }

    typename Tree::PCCompareResults Tree::checkPrefixCompare(const N *n, const Key &k, uint8_t fillKey, uint32_t &level,
                                                        LoadKeyFunction loadKey, bool &needRestart) {
        if (n->hasPrefix()) {
            Key kt;
            for (uint32_t i = 0; i < n->getPrefixLength(); ++i) {
                if (i == maxStoredPrefixLength) {
                    auto anyTID = N::getAnyChildTid(n, needRestart);
                    if (needRestart) return PCCompareResults::Equal;
                    loadKey(anyTID, kt);
                }
                uint8_t kLevel = (k.getKeyLen() > level) ? k[level] : fillKey;

                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : n->getPrefix()[i];
                if (curKey < kLevel) {
                    return PCCompareResults::Smaller;
                } else if (curKey > kLevel) {
                    return PCCompareResults::Bigger;
                }
                ++level;
            }
        }
        return PCCompareResults::Equal;
    }

    typename Tree::PCEqualsResults Tree::checkPrefixEquals(const N *n, uint32_t &level, const Key &start, const Key &end,
                                                      LoadKeyFunction loadKey, bool &needRestart) {
        if (n->hasPrefix()) {
            Key kt;
            for (uint32_t i = 0; i < n->getPrefixLength(); ++i) {
                if (i == maxStoredPrefixLength) {
                    auto anyTID = N::getAnyChildTid(n, needRestart);
                    if (needRestart) return PCEqualsResults::BothMatch;
                    loadKey(anyTID, kt);
                }
                uint8_t startLevel = (start.getKeyLen() > level) ? start[level] : 0;
                uint8_t endLevel = (end.getKeyLen() > level) ? end[level] : 255;

                uint8_t curKey = i >= maxStoredPrefixLength ? kt[level] : n->getPrefix()[i];
                if (curKey > startLevel && curKey < endLevel) {
                    return PCEqualsResults::Contained;
                } else if (curKey < startLevel || curKey > endLevel) {
                    return PCEqualsResults::NoMatch;
                }
                ++level;
            }
        }
        return PCEqualsResults::BothMatch;
    }
}
