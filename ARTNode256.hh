#pragma once
#include <cassert>
#include <iostream>
#include <vector>

#include "ARTNode.hh"

static const int ARTNODE256_CAPACITY = 256;
static const int ARTNODE256_SHRINK_THRESHOLD = 48;

template <typename V> class ARTTree;

template <typename V>
class ARTNode256 : public ARTNode<V> {
public:
  ARTNode256(ARTTree<V>* tree) : ARTNode<V>(tree), children_(), num_children_() {}

  // Assumes the size of 'pairs' is less than or equal
  // to the capacity of this node.
  ARTNode256(ARTTree<V>* tree, std::vector<byte_pointer_pair>* pairs) : ARTNode<V>(tree), children_(), num_children_(pairs->size()) {
    for (unsigned int i = 0; i < pairs->size(); i++) {
      children_[pairs->at(i).byte] = pairs->at(i).node_pointer;
    }
  }

private:
  
  void insert_record(ARTRecord<V>* record) override {
    unsigned char last_byte = RECORD_BYTE;
    children_[last_byte] = (void *)record;
    num_children_++;
  }

  void remove_record() override {
    unsigned char last_byte = RECORD_BYTE;
    children_[last_byte] = NULL;
    num_children_--;
  }

  ARTNode<V>* find_child(unsigned char byte) override {
    return (ARTNode<V>*)children_[byte];
  }

  ARTRecord<V>* find_record() override {
    return (ARTRecord<V>*)children_[(int)RECORD_BYTE];
  }

  void add_child(ARTNode<V>* child, unsigned char next_byte) override {
    children_[next_byte] = (void *)child;
    num_children_++;
  }

  void replace_child(ARTNode<V>* new_child, unsigned char next_byte) {
    children_[next_byte] = (void *)new_child;
  }

  std::vector<void *>* get_children() override {
    std::vector<void *>* vector = new std::vector<void *>();
    for (int i = 0; i < ARTNODE256_CAPACITY; i++) {
      if (i == int('\0')) {
        continue;
      }
      if (children_[i] != NULL) {
        vector->push_back(children_[i]);
      }
    }
    return vector;
  }

  bool is_full() override {
    return (num_children_ == ARTNODE256_CAPACITY);
  }

  bool is_empty() override {
    return (num_children_ == 0);
  }

  bool must_shrink() override {
    return (num_children_ <= ARTNODE256_SHRINK_THRESHOLD);
  }

  ARTNode<V>* grow() override {
    assert(false && "Cannot grow the node anymore!");
  }

  void print_info() override {
    ARTNode<V>::print_info();
    if (num_children_ == 0) {
      std::cout << "None\n" << std::endl;
      return;
    }
    
    for (int i = 0; i < ARTNODE256_CAPACITY; i++) {
      if (children_[i] != NULL) {
        std::cout << "- index \'" << char(i) << "\': " <<
          children_[i] << std::endl;
      }
    }
    std::cout << std::endl;
  }

private:
  void* children_[ARTNODE256_CAPACITY];
  int num_children_;
};
