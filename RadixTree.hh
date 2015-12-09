#pragma once

#include <utility>
#include <memory>
#include <cassert>

#include "Transaction.hh"
#include "versioned_value.hh"

// Transforms keys into arrays. These arrays should be ordered in the same way
// as the keys. Each element of the array addresses one level of the radix tree.
// Currently only constant-length keys are supported.
template <typename K>
class DefaultKeyTransformer {
public:
  static void transform(const K &key, uint8_t *buf);
  static constexpr int buf_size();
};

template <typename K, typename V, typename KeyTransformer = DefaultKeyTransformer<K> >
class RadixTree : public Shared {
public:
  static constexpr uint8_t span = 4;
  static constexpr uint8_t fanout = (1 << span);

  typedef TransactionTid::type version_t;

private:
  struct tree_node {
    void *children[fanout];
    uint8_t parent_index;
    tree_node *parent;
    version_t version;
    V *value;

    tree_node(): tree_node(nullptr, 0) {}

    tree_node(tree_node *parent, uint8_t parent_index):
      children(),
      parent_index(parent_index),
      parent(parent),
      version(TransactionTid::increment_value),
      value(nullptr) {}
  };

  typedef versioned_value_struct<V> versioned_value;
  struct leaf_node : public versioned_value {
    tree_node *parent;
    uint8_t parent_index;

    leaf_node(): leaf_node(nullptr, 0) {}

    leaf_node(tree_node *parent, uint8_t parent_index):
      versioned_value(),
      parent(parent),
      parent_index(parent_index) {}
  };

  static constexpr auto ver_insert_bit = TransactionTid::user_bit1;

  static constexpr auto item_put_bit = TransItem::user0_bit << 0;
  static constexpr auto item_remove_bit = TransItem::user0_bit << 1;

  static constexpr auto item_empty_bit = TransItem::user0_bit << 2;

public:
  RadixTree() : transformer(), root() {}

public:
  bool trans_get(const K &key, V &value) {
    leaf_node *leaf;
    tree_node *parent;
    uint8_t index;
    version_t node_version;
    std::tie(leaf, parent, index, node_version) = get_value_or_node(key);
    if (!leaf) {
      // not found, add the version of the node to detect inserts
      auto item = Sto::item(this, parent);
      item.template add_read<version_t>(node_version);
      item.add_flags(item_empty_bit);
      return false;
    }

    auto item = Sto::item(this, leaf);
    if (item.has_write()) {
      // Return the value directly from the item if this transaction performed a write.
      if (item.flags() & item_remove_bit) {
        return false;
      } else { // put
        value = item.template write_value<V>();
        return true;
      }
    }

    version_t ver;
    value = atomic_read(leaf, ver);

    // If the version has changed from the last read, this transaction can't possibly complete.
    if (item.has_read() && !TransactionTid::same_version(item.template read_value<version_t>(), ver)) {
      Sto::abort();
      return false;
    }

    item.template add_read<version_t>(ver);

    return !!(ver & TransactionTid::valid_bit);
  }

  bool get(const K &key, V &value) {
    leaf_node *leaf;
    tree_node *parent;
    uint8_t index;
    version_t node_version;
    std::tie(leaf, parent, index, node_version) = get_value_or_node(key);
    if (!leaf) {
      return false;
    }

    if (!(leaf->version() & TransactionTid::valid_bit)) {
      return false;
    }
    version_t ver;
    value = atomic_read(leaf, ver);
    return true;
  }

private:
  // If the key exists in the tree, returns a pointer to the versioned value,
  // the parent node, the index of the key within that node and the node
  // version.
  //
  // If the key does not exist, returns the lowest node in the tree that was
  // found while searching for the key.
  //
  // If any node is locked on the path down the tree, a tuple with null
  // pointers and zeroes is returned.
  std::tuple<leaf_node*, tree_node*, uint8_t, version_t> get_value_or_node(const K &key) {
    constexpr int t_key_sz = KeyTransformer::buf_size();
    uint8_t t_key[t_key_sz]; transformer.transform(key, t_key);

    tree_node *cur = &root;
    version_t node_version;
    void *next;

    for (int i = 0; i < t_key_sz; i++) {
      version_t node_version2;
      // atomically read the node version and next pointer
      while (true) {
        node_version2 = cur->version;
        fence();
        next = cur->children[t_key[i]];
        fence();
        node_version = cur->version;
        if (TransactionTid::is_locked(node_version)) {
          Sto::abort();
          return std::make_tuple(nullptr, nullptr, 0, 0);
        }
        if (node_version2 == node_version) {
          break;
        }
      }
      if (next == nullptr) {
        return std::make_tuple(nullptr, cur, 0, node_version);
      }
      if (i < t_key_sz - 1) {
        cur = static_cast<tree_node *>(next);
      }
    }

    return std::make_tuple(static_cast<leaf_node *>(next), cur,
        t_key[t_key_sz-1], node_version);
  }

  V atomic_read(leaf_node *leaf, version_t &ver) {
    version_t ver2;
    while (true) {
      ver2 = leaf->version();
      fence();
      V value = leaf->read_value();
      fence();
      ver = leaf->version();
      if (TransactionTid::is_locked(ver)) {
        Sto::abort();
        return false;
      }
      if (ver == ver2) {
        return value;
      }
    }
  }

public:
  void trans_put(const K &key, const V &value) {
    // Add nodes if they don't exist yet.
    auto leaf = insert_nodes(key, true);
    if (!leaf) {
      // aborted
      return;
    }
    auto item = Sto::item(this, leaf);
    // if there's a remove already, replace it with the put
    if (item.has_write() && (item.flags() & item_remove_bit)) {
      item.clear_flags(item_remove_bit);
    }
    item.add_write(value);
    item.add_flags(item_put_bit);
  }

  void put(const K &key, const V &value) {
    auto leaf = insert_nodes(key, false);

    TransactionTid::lock(leaf->version());

    version_t ver = leaf->version() + TransactionTid::increment_value;
    ver |= TransactionTid::valid_bit;
    ver &= ~ver_insert_bit;
    leaf->version() = ver;
    leaf->writeable_value() = value;

    TransactionTid::unlock(leaf->version());
  }

private:
  leaf_node *insert_nodes(const K &key, bool transactional) {
    constexpr int t_key_sz = KeyTransformer::buf_size();
    uint8_t t_key[t_key_sz];
    transformer.transform(key, t_key);

    void *cur = static_cast<void *>(&root);

    for (int i = 0; i < t_key_sz; i++) {
      tree_node *node = static_cast<tree_node *>(cur);
      bool item_updated = false;
      void *next = node->children[t_key[i]];
      if (next == nullptr) {
        // allocate the new node
        void *new_node;
        if (i == t_key_sz - 1) {
          // allocate a versioned value with insert bit set
          auto leaf = new leaf_node(node, t_key[i]);
          leaf->version() = ver_insert_bit;
          new_node = static_cast<void *>(leaf);
        } else {
          // allocate a tree node
          new_node = static_cast<void *>(new tree_node(node, t_key[i]));
        }

        TransactionTid::lock(node->version);
        // Even if this transaction aborts, new tree_nodes/versioned_values
        // might end up being used by some other insert. Therefore, we update
        // the node version now.
        if (node->children[t_key[i]] == nullptr) {

          auto new_ver = node->version + TransactionTid::increment_value;

          if (transactional) {
            item_updated = true;
            if (!update_node_item(node, new_ver)) {
              return nullptr;
            }
          }

          // Increment the version number of the current node.
          TransactionTid::set_version(node->version, new_ver);
          // Insert the new node.
          next = new_node;
          node->children[t_key[i]] = next;

        } else {
          // Someone else already inserted the node/value.
          if (i == t_key_sz - 1) {
            delete static_cast<leaf_node *>(new_node);
          } else {
            delete static_cast<tree_node *>(new_node);
          }
        }

        TransactionTid::unlock(node->version);
      }

      // always add the node above the leaf to the readset
      if (transactional && !item_updated && i == t_key_sz - 1) {
        // XXX: this is a race on node->version
        if (!update_node_item(node, node->version)) {
          return nullptr;
        }
      }
      cur = next;
    }

    return static_cast<leaf_node *>(cur);
  }

  bool update_node_item(tree_node *node, version_t new_ver) {
    // Add/update this node to the readset. This ensures gets that were
    // not found are not incorrectly aborted, and also ensures that the
    // new node is not hard deleted by another transaction.
    auto item = Sto::item(this, node);
    if (item.has_read()) {
      auto old_ver = item.template read_value<version_t>();
      if (!TransactionTid::same_version(node->version, old_ver)) {
        TransactionTid::unlock(node->version);
        Sto::abort();
        return false;
      }
      item.update_read(old_ver, new_ver & ~TransactionTid::lock_bit);
    } else {
      item.add_flags(item_empty_bit);
      item.add_read(new_ver & ~TransactionTid::lock_bit);
    }
    return true;
  }

public:
  void trans_remove(const K &key) {
    leaf_node *leaf;
    tree_node *parent;
    uint8_t index;
    version_t node_version;
    std::tie(leaf, parent, index, node_version) = get_value_or_node(key);
    if (!leaf && parent) {
      // not found, add the version of the node to detect inserts
      // XXX: this seems a bit weird - makes remove not exactly a blind write
      auto item = Sto::item(this, parent);
      item.template add_read<version_t>(node_version);
      item.add_flags(item_empty_bit);
      return;
    }

    auto item = Sto::item(this, leaf);
    // if there's a put already, replace it with the remove
    if (item.has_write() && (item.flags() & item_put_bit)) {
      item.clear_flags(item_put_bit);
    }
    item.add_write(true);
    item.add_flags(item_remove_bit);
  }

  void remove(const K &key) {
    leaf_node *leaf;
    tree_node *parent;
    uint8_t index;
    version_t node_version;
    std::tie(leaf, parent, index, node_version) = get_value_or_node(key);
    if (!leaf) {
      return;
    }

    TransactionTid::lock(leaf->version());
    version_t ver = leaf->version() + TransactionTid::increment_value;
    ver &= ~(TransactionTid::valid_bit | ver_insert_bit);
    leaf->version() = ver;
    TransactionTid::unlock(leaf->version());
  }

  /*
  iterator trans_query(const K &start) {
    return iterator(this, start);
  }

  class iterator {
  public:
    iterator(const RadixTree *tree, const K &start) :
      tree(tree),
      start(start),
      end(end) {}

    bool next() {
      if (cur == nullptr) {
        return false;
      }

      int parent_index = -1;
      while (true) {
        bool child_found = false;
        if (parent_index < 0) {
          for (int i = 0; i < fanout; i++) {
            if (cur->children[i] != nullptr) {
              child_found = true;
              cur = children[i];
            }
          }
        } else {
          cur = cur->parent;
          for (int i = parent_index + 1; i < fanout; i++) {
            if (cur->children[i] != nullptr) {
              child_found = true;
              cur = cur->children[i];
              parent_index = -1;
            }
          }
        }

        if (child_found && cur->value != nullptr) {
          return true;
        } else if (cur == tree->root) {
          cur == nullptr;
          return false;
        } else {
          // go up the tree
          parent_index = cur->parent_index;
        }
      }
    }

  private:
    RadixTree *tree;
    K start;
    tree_node *cur;
  }

  */

  virtual void lock(TransItem& item) {
    leaf_node *leaf = item.template key<leaf_node *>();
    TransactionTid::lock(leaf->version());
  }

  virtual bool check(const TransItem& item, const Transaction&) {
    auto flags = item.flags();
    version_t read_ver = item.template read_value<version_t>();
    if (flags & item_empty_bit) {
      tree_node *node = item.template key<tree_node *>();
      return TransactionTid::same_version(node->version, read_ver)
        && (!TransactionTid::is_locked(node->version) || item.has_write());
    } else {
      leaf_node *leaf = item.template key<leaf_node *>();
      return TransactionTid::same_version(leaf->version(), read_ver)
        && (!TransactionTid::is_locked(leaf->version()) || item.has_write());
    }
  }

  virtual void install(TransItem& item, const Transaction&) {
    leaf_node *leaf = item.template key<leaf_node *>();
    auto new_ver = leaf->version() + TransactionTid::increment_value;
    auto flags = item.flags();
    if (flags & item_put_bit) {
      new_ver |= TransactionTid::valid_bit;
      new_ver &= ~ver_insert_bit;
      leaf->writeable_value() = item.template write_value<V>();
    } else if (flags & item_remove_bit) {
      new_ver &= ~TransactionTid::valid_bit;
      new_ver &= ~ver_insert_bit;

      // XXX: this breaks opacity, we also should do this for failed inserts
      auto parent = leaf->parent;
      TransactionTid::lock(parent->version);
      //Transaction::rcu_free(parent->children[leaf->parent_index]);
      parent->children[leaf->parent_index] = nullptr;
      parent->version = parent->version + TransactionTid::increment_value;
      TransactionTid::unlock(parent->version);
    }
    TransactionTid::set_version(leaf->version(), new_ver);
  }

  virtual void unlock(TransItem& item) {
    leaf_node *leaf = item.template key<leaf_node *>();
    TransactionTid::unlock(leaf->version());
  }

  /*
  virtual void cleanup(TransItem& item, bool committed) {
  }
  */

private:
  const KeyTransformer transformer;
  tree_node root;
};

template<>
void DefaultKeyTransformer<uint64_t>::transform(const uint64_t &key, uint8_t *buf) {
  for (size_t i = 0; i < sizeof(key)*2; i++) {
    buf[2*sizeof(key)-i-1] = (key >> (4*i)) & 0xf;
  }
}

template<>
constexpr int DefaultKeyTransformer<uint64_t>::buf_size() {
  return sizeof(uint64_t) * 2;
}
