#pragma once

#include <iostream>
#include <atomic>
#include <map>
#include <regex>
#include <thread>
#include <mutex>

#include <dirent.h>

#include "util.hh"
#include "circbuf.hh"
#include "spinlock.hh"
#include "Transaction.hh"
#include "MassTrans.hh"
#include <fcntl.h>
#include "ckp_params.hh"

struct read_buffer {
  static const uint64_t MAX_SIZE = 32*1024*1024;
  static const size_t max_reg_ptrs = 20;
  
  read_buffer(int input_fd) {
    fd = input_fd;
    head = (uint8_t *) malloc(MAX_SIZE);
    sz_ = MAX_SIZE;
    cur = tail = head;
    pin_ = nullptr;
  }
  ~read_buffer() {
    free(head);
  }
  
  // for debug only
  inline void print() const {
    for (uint64_t i = 0; i < MAX_SIZE; i++) {
      printf("%c ", *(head+i));
    }
    printf("\n");
  }
  inline unsigned char debug_cur_char() const {
    return *cur;
  }
  
  inline size_t bytes_left() const {
    return tail - cur;
  }
  inline void set_pin() {
    pin_ = cur;
  }
  inline void clear_pin() {
    pin_ = nullptr;
  }
  inline char* pin() const {
    return (char*) pin_;
  }
  
  inline ssize_t pin_bytes(size_t size) {
    if ((size_t) (tail - cur) >= size || load() >= size) {
      pin_ = pin_ ? pin_ : cur;
      ssize_t offset = cur - pin_;
      cur += size;
      return offset;
    } else
      return (ssize_t) -1;
  }
  template <typename T>
  inline bool read_object(T* x) {
    if ((size_t) (tail - cur) >= sizeof(T) || load() >= sizeof(T)) {
      memcpy(x, cur, sizeof(T));
      cur += sizeof(T);
      return true;
    } else
      return false;
  }
  inline char* pinned_bytes(ssize_t offset, size_t size) const {
    assert(offset >= 0 && offset + (ssize_t) size <= (cur - pin_));
    return (char*) &pin_[offset];
  }
  
  template <typename T>
  inline const uint8_t* read_(const uint8_t* buf, T* obj) {
    const T *p = (const T *) buf;
    *obj = *p;
    return (const uint8_t *) (p + 1);
  }
  
  inline bool read_32_vs(uint32_t *number) {
    uint8_t * new_pos;
    if ((tail - cur) < sizeof(uint32_t)) {
      // reload more data and try again
      load();
      if ((tail - cur) < sizeof(uint32_t)) {
        return false;
      } else {
         new_pos = (uint8_t *) read_(cur, number);
      }
    } else {
       new_pos = (uint8_t *) read_(cur, number);
    }
    cur = new_pos;
    return true;
  }
  
  inline bool read_64_s(uint64_t *number) {
    uint8_t *new_pos;
    if ((tail - cur) < sizeof(uint64_t)) {
      // reload more data and try again
      load();
      if ((tail - cur) < sizeof(uint64_t)) {
        return false;
      } else {
        new_pos = (uint8_t *) read_(cur, number);
      }
    } else {
      new_pos = (uint8_t *) read_(cur, number);
    }
    cur = new_pos;
    return true;
  }
  
  char* make_space(size_t space);
  void take_space(size_t space);
  
private:
  uint8_t *head;
  uint8_t *cur;
  uint8_t *tail;
  uint8_t *pin_;
  size_t sz_;
  
  int fd;
  
  read_buffer(const read_buffer&) = delete;
  read_buffer& operator=(const read_buffer&) = delete;
  
  inline void shift();
  size_t load();
};



class Recovery {
  public :
  
#define NTHREADS_PER_DISK 8
#define NUM_TH_CKP 4
  
  typedef std::map<uint64_t, concurrent_btree*> txn_btree_map_type;
  typedef concurrent_btree btree_type;
  typedef btree_type::node_base_type node;
  typedef btree_type::internode_type internal_node;
  typedef btree_type::leaf_type leaf_node;
  typedef btree_type::node_type node_type;
  typedef btree_type::versioned_value_type versioned_value_type;
  typedef concurrent_btree::key_type key_type;
  typedef leaf_node::leafvalue_type leafvalue_type;
  typedef node::key_type tree_key_type; // this would be the internal masstree key type for... masstree


  static void Init() {}
  static txn_btree_map_type * recover(std::vector<std::string> logfile_base, uint64_t test_tid = 0);
  
  static void replay_checkpoint(uint64_t ptid);
  static void replay_logs(std::vector<std::string> file_list,
                          uint64_t ckp_tid,
                          uint64_t persist_tid, size_t th,
                          int index);
  
private:
  static void _read_checkpoint_file(std::string diskname, uint64_t ptid);
  static uint64_t _read_min_tid(char *tree_id);
  static void recover_ckp_thread(Recovery::btree_type* btree, std::string filename, uint64_t ptid);
  
  static txn_btree_map_type *btree_map;
  static uint64_t min_recovery_tid;
  static std::mutex mtx;
};
