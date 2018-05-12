#pragma once
#include <cassert>
#include <iostream>
#include <vector>

#include "ARTNode.hh"

static const int ARTNODE16_CAPACITY = 16;
static const int ARTNODE16_SHRINK_THRESHOLD = 4;

template <typename V> class ARTTree;

template <typename V>
class ARTNode16 : public ARTNode<V> {
public:

  ARTNode16(ARTTree<V>* tree) : ARTNode<V>(tree) , children_(), num_children_() {
    std::fill(byte_keys_, byte_keys_ + ARTNODE16_CAPACITY, EMPTY);
  }

  // Assumes the size of 'pairs' is less than or equal
  // to the capacity of this node.
  ARTNode16(ARTTree<V>* tree, std::vector<byte_pointer_pair>* pairs) : ARTNode<V>(tree), children_(), num_children_(pairs->size()) {
    std::fill(byte_keys_, byte_keys_ + ARTNODE16_CAPACITY, EMPTY);
    for (unsigned int i = 0; i < pairs->size(); i++) {
      byte_keys_[i] = pairs->at(i).byte;
      children_[i] = pairs->at(i).node_pointer;
    }
  }

private:
  char find_open_index() {
    // TODO: The ART paper talks of keeping the byte_keys array sorted, so we'll have to later implement this.
    for(int i = 0; i < ARTNODE16_CAPACITY; i++) {
      if(byte_keys_[i] == EMPTY) {
        return i;
      }
    }
    assert(false && "Expected to find an open index");
  }

  // Returns the index at which the byte is found. Return EMPTY if
  // not found.
  char search_index(unsigned char byte) {
    // TODO: The paper talks about doing binary search here to speed
    // up the process of searching. Let's implement this later.
    for(int i = 0; i < ARTNODE16_CAPACITY; i++) {
      if(byte_keys_[i] == byte) {
        return i;
      }
    }
    return EMPTY;
  }
  

  void insert_record(ARTRecord<V>* record) override {
    unsigned char last_byte = RECORD_BYTE;
    char open_index = find_open_index();
    byte_keys_[(int)open_index] = last_byte;
    children_[(int)open_index] = (void *)record;
    num_children_++;
  }

  void remove_record() override {
    unsigned char last_byte = RECORD_BYTE;
    char index = search_index(last_byte);
    byte_keys_[(int)index] = EMPTY;
    children_[(int)index] = NULL;
    num_children_--;
  }

  ARTNode<V>* find_child(unsigned char byte) override {
    char index = search_index(byte);
    return (index == EMPTY) ? NULL : (ARTNode<V> *)children_[(int)index];
  }

  ARTRecord<V>* find_record() override {
    char index = search_index(RECORD_BYTE);
    return (index == EMPTY) ? NULL : (ARTRecord<V> *)children_[(int)index];
  }

  void add_child(ARTNode<V>* child, unsigned char next_byte) override {
    char open_index = find_open_index();
    byte_keys_[(int)open_index] = next_byte;
    children_[(int)open_index] = (void *)child;
    num_children_++;
  }

  void replace_child(ARTNode<V>* new_child, unsigned char next_byte) {
    char index = search_index(next_byte);
    children_[(int)index] = (void *)new_child;
  }


  std::vector<void *>* get_children() override {
    std::vector<void *>* vector = new std::vector<void *>();
    for (int i = 0; i < ARTNODE16_CAPACITY; i++) {
      if (i == int('\0')) {
        continue;
      }
      if (children_[i] != NULL) {
        vector->push_back(children_[i]);
      }
    }
    return vector;
  }

  std::vector<byte_pointer_pair>* retrieve_byte_pointer_pairs() {
    std::vector<byte_pointer_pair>* vector = new std::vector<byte_pointer_pair>();
    for (int i = 0; i < ARTNODE16_CAPACITY; i++) {
      if (byte_keys_[i] != EMPTY) {
        byte_pointer_pair pair = { (unsigned char)byte_keys_[i], children_[i] };
        vector->push_back(pair);
      }
    }
    return vector;
  }

  bool is_full() override {
    return (num_children_ == ARTNODE16_CAPACITY);
  }

  bool is_empty() override {
    return (num_children_ == 0);
  }

  bool must_shrink() override {
    return (num_children_ <= ARTNODE16_SHRINK_THRESHOLD);
  }

  ARTNode<V>* grow() override {
    ARTNode<V>* new_node = new ARTNode48<V>(this->tree_, retrieve_byte_pointer_pairs());
    return new_node;
  }

  void print_info() override {
    ARTNode<V>::print_info();
    if (num_children_ == 0) {
      std::cout << "None\n" << std::endl;
      return;
    }
    
    for (int i = 0; i < ARTNODE16_CAPACITY; i++) {
      if (children_[i] != NULL) {
        std::cout << "- index \'" << char(i) << "\': " <<
          children_[i] << std::endl;
      }
    }
    std::cout << std::endl;
  }

private:
  int byte_keys_[ARTNODE16_CAPACITY];
  void* children_[ARTNODE16_CAPACITY];
  int num_children_;
    
};
