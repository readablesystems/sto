#pragma once

#include <iostream>
#include <atomic>
#include <regex>
#include <dirent.h>

#include "util.hh"
#include "macros.hh"
#include "circbuf.hh"
#include "spinlock.hh"
#include "Transaction.hh"
#include "MassTrans.hh"
#include <fcntl.h>
#include "ckp_params.hh"

#define NUM_TH_CKP 4

extern volatile uint64_t fsync_sync[NUM_TH_CKP];

class data_writer {
public:
  virtual ~data_writer() {}
  virtual size_t data_write(char *buf, size_t write_bytes) {return 0; }
  virtual void data_flush() {}
  virtual void data_fsync() {}
  virtual void finish(std::string file_name, std::string file_symlink_name) {} // called after each complete range is over
  virtual void done() {} // called after all of the ranges assigned have been written to disk
  virtual void write_to_disk() {}
};

#define DATA_STRIPE_PAR_BUF (4*1024*1024)

class data_stripe_par_writer : public data_writer {
public:
  data_stripe_par_writer(std::string filename, std::vector<std::string> writedirs, int id_inp) : writedirs_(writedirs) {
    num_files = writedirs.size();
    for (size_t i = 0; i < writedirs.size(); i++) {
      std::string fullpath(writedirs[i]);
      fullpath.append(filename);
      int fd = open((char *)fullpath.c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0777);
      if (fd < 0) {
        std::cerr << "bad file name: " << fullpath << std::endl;
        perror("File descriptor not valid");
        assert(false);
      }
      file_fds[i] = fd;
      total_bytes_written[i] = 0;
    }
    cur_file = 0;
    temp_buf = (char *) malloc(DATA_STRIPE_PAR_BUF);
    cur_buf_ptr = temp_buf;
    cur_bytes = 0;
    
    id = id_inp;
  }
  
  ~data_stripe_par_writer() {
    free(temp_buf);
  }
  
  bool has_space(size_t write_bytes) {
    return (cur_bytes + write_bytes < DATA_STRIPE_PAR_BUF);
  }
  
  size_t data_write(char *buf, size_t write_bytes) {
    // assumes the user has already called has_space()!
    memcpy(cur_buf_ptr, buf, write_bytes);
    cur_buf_ptr += write_bytes;
    cur_bytes += write_bytes;
    return write_bytes;
  }
  
  inline void data_flush() {
    // Currently assuming that compression is not enabled
    size_t already_written = 0;
    while (already_written < cur_bytes) {
      while (fsync_sync[id] > 0) {
        usleep(1000);
      }
      size_t w = 16 * 1024;
      if (cur_bytes - already_written < w) {
        w = cur_bytes - already_written;
      }
      
      ssize_t ret_bytes = write(file_fds[cur_file], temp_buf + already_written, w);
      
      if (ret_bytes < 0) {
        perror("data_stripe_par compress flush failed");
        assert(false);
      }
      
      already_written += (size_t) ret_bytes;
      total_bytes_written[cur_file] += ret_bytes;
      
      if (total_bytes_written[cur_file] > 2*1024*1024) {
        while (fsync_sync[id] > 0) {
          usleep(1000);
        }
        fsync(file_fds[cur_file]);
        total_bytes_written[cur_file] = 0;
      }
    }
    
    cur_bytes =  0;
    cur_buf_ptr = temp_buf;
    cur_file = (cur_file + 1) % num_files;
  }
  
  void data_fsync() {
    for (int i = 0; i < num_files; i++) {
      fsync(file_fds[i]);
    }
  }
  
  void finish(std::string file_name, std::string file_symlink_name) {
    // rename all of the files in here to the correct files
    for (size_t i = 0; i < writedirs_.size(); i++) {
      std::string dir = writedirs_[i];
      std::string checkpoint_symlink(dir);
      checkpoint_symlink.append(file_symlink_name);
      std::string checkpoint_file_name_final(dir);
      checkpoint_file_name_final.append(file_name);
      if (rename(checkpoint_file_name_final.c_str(), checkpoint_symlink.c_str()) < 0) {
        perror("Renaming failure for checkpoint files");
        assert(false);
      }
    }
  }
  
  void done() {
    for (int i = 0; i < num_files; i++) {
      close(file_fds[i]);
      file_fds[i] = -1;
    }
    num_files = 0;
  }
  
  std::vector<std::string> writedirs_;
  int file_fds[30];
  int num_files;
  int cur_file;
  size_t cur_bytes;
  char *temp_buf;
  char *cur_buf_ptr;
  int id;
  size_t total_bytes_written[10];
};

struct buffer {
  
  static const uint64_t MAX_SIZE = 32*1024*1024;
  
  buffer() {
    head_ = (char*) malloc(MAX_SIZE);
    used = 0;
    cur = head_;
  }
  
  ~buffer() {
    free(head_);
  }
  
  inline const char* head() const {
    return head_;
  }
  inline size_t size() const {
    return used;
  }
  
  inline size_t free_bytes() const {
    return MAX_SIZE - used;
  }
  
  inline bool write(char * ptr, size_t size) {
    if (free_bytes() > size) {
      memcpy(cur, ptr, size);
      cur += size;
      used += size;
      return true;
    }
    return false;
  }
  
  inline bool write_32(uint32_t integer) {
    if (free_bytes() > 4) {
      memcpy(cur, &integer, 4);
      cur += 4;
      used += 4;
      return true;
    }
    return false;
  }
  
  inline ssize_t write_to_fd(int fd) const {
    return ::write(fd, head_, used);
  }
  
  inline void rewind() {
    cur = head_;
    used = 0;
  }
  
private:
  char *head_;
  char *cur;
  size_t used;
  
  buffer(const buffer&) = delete;
  buffer& operator=(const buffer&) = delete;
};


class checkpoint_scan_callback : public concurrent_btree::low_level_search_range_callback {
public:
  typedef concurrent_btree::key_type key_type;
  typedef concurrent_btree::value_type value_type;
  typedef concurrent_btree::node_base_type node_opaque_t;
  typedef concurrent_btree::versioned_value_type versioned_value;
  
  checkpoint_scan_callback() {
    eol = '\n';
    fd = -1;
    max_epoch = 0;
    ckp_epoch = 0;
    writer = NULL;
  }
  
  ~checkpoint_scan_callback() {}
  
  void on_resp_node(const node_opaque_t *n, uint64_t version) {}
  
  bool invoke (const key_type &k, versioned_value v,
               const node_opaque_t *n, uint64_t version);
  
  inline void write_txn(char * start_tid, char* key, uint32_t key_length,
                        char* value, uint32_t value_length) {
    
    if (enable_datastripe_par || enable_par_ckp) {
      size_t total_size = 8 + 4 + key_length + 4 + value_length;
      if ( !((data_stripe_par_writer *) writer)->has_space(total_size)) {
        write_to_disk();
      }
    }
    write_to_buffer(start_tid, 8);
    write_to_buffer(key_length);
    write_to_buffer(key, key_length);
    write_to_buffer(value_length);
    write_to_buffer(value, value_length);
    
  }
  
  inline void write_to_buffer(uint32_t size) {
    if (enable_datastripe) {
      if (fd == -1) {
        open_file();
      }
      if (writer->data_write((char *) &size, 4) < 4) {
        this->write_to_disk();
        writer->data_write((char *)&size, 4);
      }
    } else if (enable_datastripe_par || enable_par_ckp) {
      writer->data_write((char *) &size, 4);
    } else {
      if (!buf.write_32(size)) {
        this->write_to_disk();
        buf.rewind();
        buf.write_32(size);
      }
    }
  }
  
  inline void write_to_buffer(char *ptr, size_t size) {
    if (enable_datastripe) {
      if (fd == -1) {
        open_file();
      }
      if (writer->data_write(ptr, size) < size) {
        this->write_to_disk();
        writer->data_write(ptr, size);
      }
    } else if (enable_datastripe_par || enable_par_ckp) {
      writer->data_write(ptr, size);
    } else {
      if (!buf.write(ptr, size)) {
        this->write_to_disk();
        buf.rewind();
        buf.write(ptr, size);
      }
    }
  }
  
  inline void set_tree_id(uint64_t tree_id) {
    this->tree_id = tree_id;
  }
  
  void open_file();
  void write_to_disk();
  
  inline void clear_buffer() {
    buf.rewind();
  }
  
  void data_fsync() {
    if (enable_datastripe_par || enable_par_ckp)
      writer->data_fsync();
  }
  
  void finish() {
    if (enable_datastripe) {
      writer->data_fsync();
      writer->finish(checkpoint_file_symlink_name, std::string("ckp_").append(std::to_string(tree_id)));
      writer->done();
      delete writer;
      fd = -1;
    } else if (enable_datastripe_par || enable_par_ckp) {
      writer->finish(checkpoint_file_symlink_name, std::string("ckp_").append(std::to_string(tree_id)));
      writer->done();
      delete writer;
      fd = -1;
    } else {
      
      fsync(fd);
      close(fd);
      fd = -1;
      
      std::string checkpoint_symlink(checkpoint_file_stem);
      checkpoint_symlink.append("ckp_");
      checkpoint_symlink.append(std::to_string(tree_id));
      
      // relink and delete old file
      char buf[512];
      memcpy(buf, checkpoint_symlink.c_str(), checkpoint_file_stem.size());
      char *buf_ptr = buf + strlen(buf);
      if (readlink(checkpoint_symlink.c_str(), buf_ptr, 128) > 0) {
        remove(checkpoint_symlink.c_str());
        if (symlink(checkpoint_file_symlink_name.c_str(), checkpoint_symlink.c_str()) == 0) {
        }
        if (remove((const char *) buf) < 0) {
          perror("Remove failed");
          assert(false);
        }
      }
    }
  }
  
  buffer buf;
  char eol;
  
public:
  uint64_t max_epoch;
  uint64_t ckp_epoch;
  uint64_t tree_id;
  int fd;
  data_writer *writer;
  std::string checkpoint_file_name;
  std::string checkpoint_file_symlink_name;
  std::string checkpoint_file_stem;
  int idx;

};

class Checkpointer {
  
  typedef concurrent_btree::node_base_type node;
  typedef concurrent_btree::internode_type i_node;
  typedef concurrent_btree::leaf_type l_node;
  typedef concurrent_btree::key_type key_type;
  typedef concurrent_btree::key_slice key_slice;
  typedef int Value;
  typedef std::vector<MassTrans<Value> *> tree_list_type;
  
  
  struct checkpoint_tree_key {
    concurrent_btree* tree;
    key_type lowkey;
    key_type highkey;
    checkpoint_tree_key(concurrent_btree* t, key_type l, key_type h) :
    tree(t), lowkey(l), highkey(h) {}
  };
  
  static void _checkpoint_walk_multi_range(std::vector<checkpoint_tree_key>* range_list, int count,
                                           uint64_t* max, uint64_t ckp_epoch);
  static void _range_query_par(concurrent_btree* btr, key_type *lower, key_type* higher, int i,
                               uint64_t* max_epoch, uint64_t ckp_epoch);
  
  static key_type* _tree_walk_1(concurrent_btree* tree);
  static l_node* _walk_to_right_1(node* root);
  
  // Variables
  static tree_list_type _tree_list;
  static size_t NUM_QUERIES;
  static int fd;
  // static checkpoint_scan_callback callback;
  
public:
  static void AddTree(MassTrans<Value> *tree);
  // logfile_base is used when truncating the log
  static void Init(std::vector<MassTrans<Value> *> *btree_list,
                   std::vector<std::string> logfile_base, bool is_test = false);
  static void checkpoint(std::vector<std::string> logfile_base);
  static void log_truncate(uint64_t epoch, std::vector<std::string> logfile_base);
  static void StartThread(bool is_test, std::vector<std::string> logfile_base);
  //static std::regex log_reg;
  // TODO: add support for truncating the log
};
