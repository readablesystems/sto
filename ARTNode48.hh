#pragma once
#include <iostream>
#include <vector>

#include "ARTNode.hh"

static const int ARTNODE48_CAPACITY = 48;
static const int ARTNODE48_SHRINK_THRESHOLD = 16;

template <typename V> class ARTTree;

template <typename V>
class ARTNode48 : public ARTNode<V> {
public:
  
  ARTNode48(ARTTree<V>* tree) : ARTNode<V>(tree), children_(), index_to_children_(), num_children_() {
    std::fill(index_to_children_, index_to_children_ + 256, EMPTY);
  }

  // Assumes the size of 'pairs' is less than or equal
  // to the capacity of this node.
  ARTNode48(ARTTree<V>* tree, std::vector<byte_pointer_pair>* pairs) : ARTNode<V>(tree), children_(), num_children_(pairs->size()) {
    std::fill(index_to_children_, index_to_children_ + 256, EMPTY);
    for (unsigned int i = 0; i < pairs->size(); i++) {
      index_to_children_[pairs->at(i).byte] = i;
      children_[i] = pairs->at(i).node_pointer;
    }
  }
  
private:

  char find_open_index() {
    assert(!this->is_full() && "B");
    for(int i = 0; i < ARTNODE48_CAPACITY; i++) {
      if(children_[i] == NULL)
        return i;
    }
    assert(false && "Expected to find an open index.");
  }
  
  void insert_record(ARTRecord<V>* record) override {
    unsigned char last_byte = RECORD_BYTE;
    char open_index = find_open_index();
    children_[(int)open_index] = (void *)record;
    index_to_children_[last_byte] = open_index;
    num_children_++;
  }

  void remove_record() override {
    unsigned char last_byte = RECORD_BYTE;
    children_[index_to_children_[last_byte]] = NULL;
    index_to_children_[last_byte] = EMPTY;
    num_children_--;
  }

  ARTNode<V>* find_child(unsigned char byte) override {
    return (index_to_children_[byte] == EMPTY) ? NULL : (ARTNode<V>*)children_[index_to_children_[byte]];
  }

  ARTRecord<V>* find_record() override {
    return (index_to_children_[(int)RECORD_BYTE] == EMPTY) ? NULL : (ARTRecord<V>*)children_[index_to_children_[(int)RECORD_BYTE]];
  }

  void add_child(ARTNode<V>* child, unsigned char next_byte) override {
    char open_index = find_open_index();
    index_to_children_[next_byte] = open_index;
    children_[(int)open_index] = (void *)child;
    num_children_++;
  }

  void replace_child(ARTNode<V>* new_child, unsigned char next_byte) {
    children_[index_to_children_[next_byte]] = (void *)new_child;
  }


  std::vector<void *>* get_children() override {
    std::vector<void *>* vector = new std::vector<void *>();
    for (int i = 0; i < ARTNODE48_CAPACITY; i++) {
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
    for (int i = 0; i < 256; i++) {
      if (index_to_children_[i] != EMPTY) {
        byte_pointer_pair pair = { (unsigned char)i, children_[index_to_children_[i]] };
        vector->push_back(pair);
      }
    }
    return vector;
  }

  bool is_full() override {
    return (num_children_ == ARTNODE48_CAPACITY);
  }

  bool is_empty() override {
    return (num_children_ == 0);
  }

  bool must_shrink() override {
    return (num_children_ <= ARTNODE48_SHRINK_THRESHOLD);
  }

  ARTNode<V>* grow() override {
    ARTNode<V>* new_node = new ARTNode256<V>(this->tree_, retrieve_byte_pointer_pairs());
    return new_node;
  }

  void print_info() override {
    ARTNode<V>::print_info();
    if (num_children_ == 0) {
      std::cout << "None\n" << std::endl;
      return;
    }
    
    for (int i = 0; i < ARTNODE48_CAPACITY; i++) {
      if (children_[i] != NULL) {
        std::cout << "- index \'" << char(i) << "\': " <<
          children_[i] << std::endl;
      }
    }
    std::cout << std::endl;
  }
  
private:
  void* children_[ARTNODE48_CAPACITY];
  int index_to_children_[256];
  int num_children_;
};
