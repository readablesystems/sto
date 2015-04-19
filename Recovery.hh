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


#define HASHBLOCK_SIZE 50
class hashblock {
  // this block is just a chunk of memory, but represents an entry
  // in the hashtable
  // provides chaining
  
public:
  hashblock() {
    used = 0;
    next = NULL;
    used_ptr = buf;
    //cur_ptr = buf;
    lockbit = false;
  }
  
  void done() {
    // free memory
    if (next != NULL)
      next->done();
    delete next;
  }
  
  inline void lock() {
    while (!__sync_bool_compare_and_swap(&lockbit, false, true)) { }
    //mtx.lock();
  }
  
  inline void unlock() {
    lockbit = false;
    //mtx.unlock();
  }
  
  inline uint8_t ** put(uint8_t *key, size_t key_size, uint8_t **value) {
    // this assumes that find_key() didn't find anything
    //printf("put(): used %lu, %lu\n", used, 8 + key_size + sizeof(uint8_t *));
    
    size_t total_size = 8 + key_size + sizeof(uint8_t *);
    
    if (used + total_size < HASHBLOCK_SIZE) {
      uint8_t *ptr = buf + used;
      memcpy(ptr, &key_size, 8);
      
      ptr += 8;
      memcpy(ptr, key, key_size);
      
      ptr += key_size;
      memcpy(ptr, value, sizeof(uint8_t *));
      used += total_size;
      used_ptr = buf + used;
      //printf("total_size is %lu\n", total_size);
      
      return (uint8_t **) ptr;
    } else {
      if (next == NULL) {
        //printf("hashblock size %lu, key: %p\n", sizeof(hashblock), key);
        next = new hashblock;
      }
    }
    return next->put(key, key_size, value);
  }
  
  inline uint8_t **find_key(uint8_t *key, size_t size) {
    
    if (used == 0)
      return NULL;
    
    uint8_t *cur_ptr = buf;
    
    do {
      size_t *key_size = (size_t *) cur_ptr;
      cur_ptr += 8;
      
      //printf("[%u] buf: %p, key_size: %p, cur_ptr: %p, used_ptr: %p\n", count, buf, key_size, cur_ptr, used_ptr);
      if (*key_size == size) {
        
        if (memcmp(cur_ptr, key, size) == 0) {
          //printf("Found the key\n");
          return (uint8_t **) (cur_ptr + size);
        }
        
      }
      
      cur_ptr += *key_size + 8;
      //count += 1;
    } while (cur_ptr < used_ptr);
    
    if (next != NULL) {
      //printf("Goes to next block for key\n");
      return next->find_key(key, size);
    }
    //printf("Did not find key\n");
    return NULL;
  }
  
  uint8_t buf[HASHBLOCK_SIZE];
  size_t used;
  hashblock *next;
  uint8_t *used_ptr;
  //uint8_t *cur_ptr;
  volatile bool lockbit;
};


#define TABLE_SIZE (32*1024*1204)
class hashtable { // TODO: is this required
public:
  hashtable() {
    //blocks = (hashblock *) malloc(sizeof(hashblock) * TABLE_SIZE);
    for (int i = 0; i < TABLE_SIZE; i++) {
      blocks[i] = hashblock();
    }
  }
  
  ~hashtable() {
    for (int i = 0; i < TABLE_SIZE; i++) {
      blocks[i].done();
    }
  }
  
  inline hashblock *get_block(size_t idx) {
    return blocks + idx;
  }
  
  hashblock blocks[TABLE_SIZE];
};


struct read_buffer { // TODO: do I need this?
  
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
        std::cout << "read_32_vs(): new pos isn't defined" << std::endl;
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
        //std::cout << "read_64_s(): new pos isn't defined " << (tail - cur) <<std::endl;
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
  
  typedef MassTrans<int> tree_type;
  typedef std::map<uint64_t, tree_type*> txn_btree_map_type;
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
  static txn_btree_map_type * recover(std::vector<std::string> logfile_base, uint64_t test_epoch = 0);
  
  static void replay_checkpoint(uint64_t pepoch);
  static void replay_logs(std::vector<std::string> file_list,
                          uint64_t ckp_epoch,
                          uint64_t persist_epoch, size_t th,
                          int index);
  
private:
  static void _read_checkpoint_file(std::string diskname, uint64_t pepoch);
  static uint64_t _read_min_epoch(char *tree_id);
  static void recover_ckp_thread(Recovery::btree_type* btree, std::string filename, uint64_t pepoch);
  
  static txn_btree_map_type *btree_map;
  static uint64_t min_recovery_epoch;
  static std::mutex mtx;
};