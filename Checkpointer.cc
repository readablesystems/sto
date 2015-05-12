#include "Checkpointer.hh"
#include <thread>
#include "util.hh"

std::vector<std::vector<std::string> > ckpdirs;
int run_for_ckp = 3;
volatile bool checkpoint_stop = false;
int enable_ckp = 0;
int enable_ckp_compress = 0;
int enable_datastripe = 0; // DATASTRIPE
int enable_datastripe_par = 0; // DATASTRIPE_PAR
int enable_par_ckp = 0; // PAR_CKP
int enable_par_log_replay = 0;
int reduced_ckp = 0;
int nckp_threads = 0;
int ckp_compress = 0;
int recovery_test = 0;
bool if_runner_done = false;

Checkpointer::tree_list_type Checkpointer::_tree_list;
int Checkpointer::fd = -1;
volatile uint64_t fsync_sync[NUM_TH_CKP] = {0, 0, 0, 0};

void Checkpointer::AddTree(concurrent_btree *tree) {
  _tree_list.push_back(tree);
}

void Checkpointer::Init(std::vector<concurrent_btree *> *btree_list,
                        std::vector<std::string> logfile_base, bool is_test) {
  if (btree_list != NULL) {
    size_t size = btree_list->size();
    for (size_t i = 0; i < size; i++) {
      _tree_list.push_back(btree_list->at(i));
    }
  } 
  if (!_tree_list.empty()) { 
    std::thread t(&Checkpointer::StartThread, is_test, logfile_base);
    t.detach();
  }
}

void Checkpointer::StartThread(bool is_test, std::vector<std::string> logfile_base) {
  // this loop checkpoints every once in a while
  printf("Starting checkpoint...\n");
  
  int count = 0;
  int num_checkpoints = run_for_ckp == 0 ? 10 : run_for_ckp;
  
  sleep(15);
  fprintf(stderr, "Done sleeping, starting first checkpoint\n");
  
  while (true) {
    checkpoint(logfile_base);
    if (is_test) {
      count += 1;
    
      if (count == num_checkpoints - 1) {
       // checkpoint_stop = true; // TODO: why this?
      }
    
      if (count == num_checkpoints) {
        if_runner_done = true;
        fprintf(stderr, "All checkpointer doen \n");
        break;
      }
    }
    
    sleep(10); // Magic number
  }
}

void Checkpointer::checkpoint(std::vector<std::string> logfile_base) {
  printf("Checkpoint start \n");
  Checkpointer::tree_list_type::iterator begin_it = Checkpointer::_tree_list.begin();
  Checkpointer::tree_list_type::iterator end_it = Checkpointer::_tree_list.end();
  
  uint64_t checkpoint_tid = Transaction::_TID;
  uint64_t max_tid = 0;
  
  std::vector<std::vector<checkpoint_tree_key> > range_list;
  
  for (int i = 0; i < NUM_TH_CKP; ++i) {
    range_list.push_back(std::vector<checkpoint_tree_key>());
  }
  
  // Assign each checkpointer thread a range of keys for each tree
  for (Checkpointer::tree_list_type::iterator it = begin_it; it != end_it; it++) {
    concurrent_btree * tree = *it;
    
    if (tree == NULL) {
      continue;
    }
    
    // Find max key of the tree.
    key_type *max_key = NULL;
    while (true) {
      max_key = _tree_walk_1(tree);
      if (max_key != NULL) {
        break;
      }
    }
    
    // Split keys
    size_t key_size = max_key->length() + 1;
    uint8_t* ptr = new uint8_t[key_size];
    memcpy(ptr, max_key->data(), key_size - 1);
    *(ptr + key_size) = 255;
    
    key_type higher(ptr, key_size);
    uint64_t step = atoi(higher.data()) / NUM_TH_CKP;
    step = (step == 0) ? 1 : step;
    key_type last;
    for (int i = 0; i < NUM_TH_CKP; i++) {
      key_type next;
      if (i == NUM_TH_CKP - 1) {
        next = higher;
      } else {
        uint64_t slice = step * (i + 1);
        next = key_type(new uint8_t[8], 8);
        memcpy((uint8_t*) next.data(), &slice, 8);
      }
      range_list[i].push_back(checkpoint_tree_key(tree, last, next));
      last = next;
    }
  }
  
  uint64_t tids[NUM_TH_CKP];
  std::vector<std::thread> callback_threads;
  for (int i = 0; i < NUM_TH_CKP; i++) {
    tids[i] = 0;
    callback_threads.emplace_back(&_checkpoint_walk_multi_range, &range_list[i], i, &tids[i], checkpoint_tid);
  }
  
  for (int i = 0; i < NUM_TH_CKP; i++) {
    callback_threads[i].join();
    if (tids[i] > max_tid) {
      max_tid = tids[i];
    }
  }
  
  for (int i = 0; i < NUM_TH_CKP; i++) {
    for (auto it = range_list[i].begin(); it != range_list[i].end(); ++it) {
      delete[] (uint8_t*) it->highkey.data();
    }
  }
  range_list.clear();
  
  printf("[checkpoint] Finished walking all trees \n");
  
  if (checkpoint_stop) {
    return;
  }
  
  std::cout << "Checkpoint tid is " << checkpoint_tid << std::endl;
  std::cout << "Max tid is " << max_tid << std::endl;
  std::cout << "Current tid is " << Transaction::_TID << std::endl;
  
  // write to disk the checkpoint tid
  // perform log truncation
  
  std::string t = std::to_string(std::time(NULL));
  std::string ctid_file(std::string(root_folder).append("/checkpoint_tid_"));
  std::string ctid_file_symlink("checkpoint_tid_");
  ctid_file.append(t);
  ctid_file_symlink.append(t);
  
  int ctid_fd = open(ctid_file.c_str(), O_CREAT|O_WRONLY, 0777);
  if (ctid_fd < 0) {
    perror("Checkpoint tid file failed");
    assert(false);
  }
  
  if (write(ctid_fd, (char *) &checkpoint_tid, 8) < 0) {
    perror("Write to checkpoint tid failed");
    assert(false);
  }
  fsync(ctid_fd);
  std::string ctid_symlink = std::string(root_folder).append("/ctid");
  
  if (rename(ctid_file.c_str(), ctid_symlink.c_str()) < 0) {
    perror("Renaming failure for ctid");
    assert(false);
  }
  
  close(ctid_fd);
  log_truncate(checkpoint_tid, logfile_base);
  
  std::cout << "Checkpointing done\n" << std::endl;
}

// Find the right most leaf node to find the max key
Checkpointer::key_type* Checkpointer::_tree_walk_1(concurrent_btree* tree) {
  std::vector<concurrent_btree::key_slice> key_slices;
  
restart:
  key_slices.clear();
  node* root = tree->table_.root();
  
  while (true) {
    l_node* right_most_node = Checkpointer::_walk_to_right_1(root);
    if (right_most_node == NULL) {
      continue;
    }
    
  retry:
    // since our leaf node might not be the right most node, we need to keep
    // on walking to the right until we find the last node.
    
    auto version = right_most_node->stable();
    
    while (true) {
      version = right_most_node->stable();
      l_node* lnode;
      
      if (right_most_node->deleted()) {
        lnode = right_most_node->prev_;
        if (lnode) {
          right_most_node = lnode;
        } else {
          lnode = right_most_node->safe_next();
          if (lnode) {
            right_most_node = lnode;
          } else {
            // if for some reason, the node doens't have any more
            // neighbors left, we go ahead and retry from the beginning
            goto restart;
          }
        }
      } else {
        lnode = right_most_node->safe_next();
        if (lnode) {
          right_most_node = lnode;
        } else {
          break;
        }
      }
    }
    
    int num_keys = right_most_node->permutation().size();
    int last_index = right_most_node->permutation()[num_keys - 1];
    
    if (right_most_node->value_is_layer(last_index)) {
      // there's definitely at least one more layer to go
      if (right_most_node->has_changed(version)) {
        printf("goes to retry 1\n");
        goto retry;
      }
      
      key_slices.push_back(right_most_node->ikey(last_index));
      root = right_most_node->lv_[last_index].layer();
      continue;
    } else {
      // we have found the max key
      auto max_key_str = right_most_node->get_key(last_index).unparse();
      const char * max_key_ptr = max_key_str.mutable_data();
      size_t last_key_size = max_key_str.length();
      
      if (right_most_node->has_changed(version)) {
      	printf("goes to retry \n");
      	goto retry;
      }
      
      size_t vector_size = key_slices.size() * 8;
      size_t size = vector_size + last_key_size;
      
      uint8_t *ptr = (uint8_t *) malloc(size);
      for (size_t i = 0; i < vector_size; i++) {
      	memcpy(ptr+i*8, &(key_slices[i]), 8);
      }
      
      memcpy(ptr+vector_size, max_key_ptr, last_key_size);
      key_type *k = new key_type(ptr, size);
      return k;

    }
    
  }
}

Checkpointer::l_node* Checkpointer::_walk_to_right_1(node* root) {
  std::vector<node *> node_list;
  node* cur_node = root;
  
  while (true) {
    if (cur_node->isleaf()) {
      break;
    }
    
    i_node *internal_node = static_cast<i_node*>(cur_node);
    auto version = cur_node->stable();
    
    // if the node is being garbage collected, it must have merged with
    // a sibling already, to be safe, go back and retry
    if (cur_node->deleted()) {
      cur_node = node_list.back();
      node_list.pop_back();
    }
    
    size_t num_keys = internal_node->size();
    node *last_node = internal_node->child_[num_keys];
    
    // if structure has changed, we retry at this node
    if (unlikely(cur_node->has_changed(version))) {
      continue;
    }
    
    node_list.push_back(cur_node);
    cur_node = last_node;
  }
  
  return static_cast<l_node *>(cur_node);
}

// each checkpoint worker thread executes this function: the function
// takes in range_list, which is a list of predefined ranges
// low and high are indices that indicate the range this checkpoint worker is responsible for
void Checkpointer::_checkpoint_walk_multi_range(std::vector<checkpoint_tree_key>* range_list,
                                                int count,
                                                uint64_t* max,
                                                uint64_t ckp_tid) {
  uint64_t max_tid = 0;
  uint64_t max_tid_local = 0;
  
  for (auto it = range_list->begin(); it != range_list->end(); ++it) {
    printf("[%u] Start _range_query_par for tree with id %lu\n", count, it->tree->get_tree_id());
    
    concurrent_btree* btr = it->tree;
    if (btr == NULL) {
      continue;
    }

    _range_query_par(btr, &it->lowkey, &it->highkey, count, &max_tid_local, ckp_tid);
    
    if (max_tid_local > max_tid)
      max_tid = max_tid_local;
    
    printf("[%u] _range_query_par done for tree %lu\n", count, btr->get_tree_id());
  }
  
  *max = max_tid;
}

void Checkpointer::_range_query_par(concurrent_btree* btr, key_type *lower, key_type *higher,
                                    int i, uint64_t* max_tid, uint64_t ckp_tid) {
  checkpoint_scan_callback local_callback;
  local_callback.idx = i;
  local_callback.set_tree_id(btr->get_tree_id());
  
  key_type key_lower = (key_type) *lower;
  const key_type *key_higher = (const key_type *) higher;
  local_callback.ckp_tid = ckp_tid;
  btr->search_range_call(key_lower, key_higher, local_callback, NULL);
  
  local_callback.write_to_disk();
  local_callback.data_fsync();
  
  uint64_t persist_tid = 0;
  // Need to wait until the persist_tid reaches the max_tid of checkpointed data
  while (true) {
    persist_tid = Logger::system_sync_tid_->load(std::memory_order_acquire);
    if (persist_tid >= local_callback.max_tid)
      break;   
    usleep(1000);
  }
  
  if (!checkpoint_stop)
    local_callback.finish();
  *max_tid = local_callback.max_tid;
}


void Checkpointer::log_truncate(uint64_t tid, std::vector<std::string> logfile_base) {
  std::regex log_reg2("old_data(.*)");
  
  printf("Truncating up to tid %lu\n", tid);
  
  std::vector<std::string> file_list;
  
  for (size_t i = 0; i < logfile_base.size(); i++) {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(logfile_base[i].c_str())) != NULL) {
      while ((ent = readdir(dir)) != NULL) {
        char *file_name = ent->d_name;
        std::cmatch cm;
        if (std::regex_match(file_name, cm, log_reg2, std::regex_constants::format_default)) {
          char *max_tid_char = (char *) cm[1].str().c_str();
          uint64_t max_tid = _char_to_uint64(max_tid_char);
          if (max_tid < tid) {
            std::string f(logfile_base[i]);
            f.append(std::string(file_name));
            file_list.push_back(f);
          }
        }
      }
      closedir(dir);
    }
    
  }
  
  std::vector<std::string>::iterator it = file_list.begin();
  std::vector<std::string>::iterator it_end = file_list.end();
  
  for (; it != it_end; it++) {
    if (remove((const char *) it->c_str()) < 0) {
      printf("Trying to remove %s\n", it->c_str());
      perror("Log truncation file remove error");
      assert(false);
    }
  }
}



//////////////////////////////////////
// Checkpoint_scan_callback method implementations
///////////////////////////////////////
typedef uint64_t Version;
static constexpr Version lock_bit = 1ULL<<(sizeof(Version)*8 - 1);
static constexpr Version invalid_bit = 1ULL<<(sizeof(Version)*8 - 2);
static constexpr Version valid_check_only_bit = 1ULL<<(sizeof(Version)*8 - 3);
static constexpr Version deleted_bit = 1ULL<<(sizeof(Version)*8 - 4);

static constexpr Version version_mask = ~(lock_bit|invalid_bit|valid_check_only_bit);
static constexpr uint8_t tid_shift = 1;

bool checkpoint_scan_callback::invoke (const key_type &k, versioned_value v,
                                       const node_opaque_t *n, uint64_t version) {
  char * key = (char *) k.data();
  uint64_t start_t = v.version() & ~(Version)1;
  
  std::string value = v.read_value();
  if (enable_datastripe_par || enable_par_ckp) {
    uint64_t cur_tid = start_t;
    max_tid = (cur_tid > max_tid) ? cur_tid : max_tid;
    
    if (reduced_ckp) {
      // we perform a reduced checkpoint here
      if (cur_tid > ckp_tid) {
        return true;
      }
    }
    
    if (fd == -1) {
      open_file();
    }
    write_txn((char*) &start_t, key, k.length(), (char *) value.c_str(), value.size());
      
  } else {
      write_txn((char *)&start_t, key, k.length(), (char *) value.c_str(), value.size());
  }
  
  return true;
}

void checkpoint_scan_callback::write_to_disk() {
  if (fd == -1) {
    printf("Open File called\n");
    open_file();
  }
  
  if (enable_par_ckp) {
    writer->data_flush();
  }

}

void checkpoint_scan_callback::open_file() {
  fd = 0;
  std::time_t timestamp = std::time(NULL);
  std::string t = std::to_string(timestamp);
  
  checkpoint_file_symlink_name = "checkpoint_";
  
  checkpoint_file_symlink_name.append(t);
  checkpoint_file_symlink_name.append("_");
  checkpoint_file_symlink_name.append(std::to_string(tree_id));
  
  writer = new data_stripe_par_writer(checkpoint_file_symlink_name, ckpdirs[idx], idx);
}
