#include "Recovery.hh"

typedef Recovery::txn_btree_map_type txn_btree_map_type;

txn_btree_map_type * Recovery::btree_map = new txn_btree_map_type();
uint64_t Recovery::min_recovery_tid = std::numeric_limits<uint64_t>::max();
std::mutex Recovery::mtx;

txn_btree_map_type * Recovery::recover(std::vector<std::string> logfile_base, uint64_t test_tid) {
  std::vector<std::vector<std::string>* > file_list;
  uint64_t bytes[NTHREADS_PER_DISK * NUM_TH_CKP];
  
  for (int i = 0; i < NTHREADS_PER_DISK * NUM_TH_CKP; i++) {
    std::vector<std::string> * vec = new std::vector<std::string>();
    file_list.emplace_back(vec);
    bytes[i] = 0;
  }
  
  std::regex log_reg("old_data(.*)");
  
  std::vector<std::thread> threads;
  std::vector<std::thread> keyspace_threads;
  
  uint64_t ckp_tid = 0;
  uint64_t persist_tid = 0;
  
  if (test_tid == 0) {
    // find the persistent tid
    int fd = open(std::string(root_folder).append("/ptid").c_str(), O_RDONLY);
    if (fd > 0) {
      if (read(fd, (char *) &persist_tid, 8) < 0) {
        perror("Persist tid reading failed");
        assert(false);
      }
      close(fd);
    } else {
      perror("ptid file not found");
      assert(false);
    }
  } else {
    persist_tid = test_tid;
  }
  
  std::cout << "persist_tid is " << persist_tid << std::endl;
  
  int fd = open(std::string(root_folder).append("/ctid").c_str(), O_RDONLY);
  if (fd > 0) {
    if (read(fd, (char *) &ckp_tid, 8) < 0) {
      perror("Checkpoint tid reading failed");
      assert(false);
    }
    close(fd);
  } else {
    perror("[recover] ctid file not found");
    goto log_replay;
  }

  replay_checkpoint(persist_tid);
  
log_replay:
  for (size_t i = 0; i < logfile_base.size(); i++) {
    uint64_t all_max_tid = 0;
    std::map<uint64_t, std::string> map_list;
    
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(logfile_base[i].c_str())) != NULL) {
      while ((ent = readdir(dir)) != NULL) {
        char* file_name = ent->d_name;
        std::cmatch cm;
        if (std::regex_match(file_name, cm, log_reg, std::regex_constants::format_default)) {
          char *max_tid_char = (char *) cm[1].str().c_str();
          uint64_t max_tid = _char_to_uint64(max_tid_char);
          if (max_tid < ckp_tid) {
            continue;
          }
          std::string f(logfile_base[i]);
          f.append(std::string(file_name));
    
          map_list[max_tid] = f;
          all_max_tid = (max_tid > all_max_tid) ? max_tid : all_max_tid;
        }
      }
      closedir(dir);
    }
    
    std::string s(logfile_base[i]);
    file_list[i*NTHREADS_PER_DISK]->push_back(s.append("data.log"));
    
    std::map<uint64_t, std::string>::reverse_iterator rit = map_list.rbegin(); // reverse order of max_tid
    int file_list_idx = 0;
    for(; rit != map_list.rend(); rit++) {
      file_list[file_list_idx + i * NTHREADS_PER_DISK]->push_back(rit->second);
      file_list_idx = (file_list_idx + 1) % NTHREADS_PER_DISK;
    }
  }
  
  for (size_t i = 0; i < logfile_base.size(); i++) {
    for (int j = 0; j < NTHREADS_PER_DISK; j++) {
      threads.emplace_back(&replay_logs, *(file_list[i * NTHREADS_PER_DISK + j]), ckp_tid,
                           persist_tid, i * NTHREADS_PER_DISK + j,
                           i);
    }
  }
  
  for (size_t i = 0; i < file_list.size(); i++) {
    delete file_list[i];
  }
  
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
  
  // delete all of the extra records
  
  return btree_map;
}


// Replay logs
void Recovery::replay_logs(std::vector<std::string> file_list,
                           uint64_t ckp_tid,
                           uint64_t persist_tid,
                           size_t th,
                           int index) {
  btree_type::thread_init();
  std::cout << "Recover thread " << th <<" starting " << std::endl;
  if (file_list.size() == 0) {
    return;
  }
  
  std::vector<std::string>::iterator it = file_list.begin();
  std::vector<std::string>::iterator it_end = file_list.end();
  
  for (; it != it_end; it++) {
    std::string file_name_value = *it;
    
    int fd = open(file_name_value.c_str(), O_RDONLY);
    std::cout << "[" << th <<"] Reading log file " << file_name_value << std::endl;
    
    read_buffer buf(fd);
    
    while (true) {
      uint64_t nentries = 0; // Num of transactions
      uint64_t last_tid = 0; // last tid of all transactions
      uint64_t tid = 0;
      if (!buf.read_64_s(&nentries))
        goto end;
      if (!buf.read_64_s(&last_tid))
        goto end;
      
      if (!buf.read_64_s(&tid))
        goto end;
      
      for (uint64_t n = 0; n < nentries; n++) { // for each transaction
        bool replay = true;
        uint64_t tid = 0;
        if (!buf.read_64_s(&tid)) // transaction tid
          goto end;
        uint64_t cur_tid = tid;
        if (cur_tid > persist_tid || cur_tid < ckp_tid) {
          // we should not replay this transaction if it's bigger than the persistent tid,
          // and we don't need to replay if it's smaller than the checkpoint tid
          replay = false;
        }
        
        uint32_t num_records = 0;
        if (!buf.read_32_vs(&num_records))
          goto end;
        
        for (uint32_t i = 0; i < num_records; i++) { // For each record
          buf.set_pin();
          uint8_t tree_id;
          if (!buf.read_object(&tree_id))
            goto end;
          
          uint32_t key_size = 0;
          if (!buf.read_32_vs(&key_size))
            goto end;
          
          ssize_t key_offset = buf.pin_bytes(key_size);
          if (key_offset < 0)
            goto end;
          
          uint32_t value_size = 0;
          if (!buf.read_32_vs(&value_size))
            goto end;
          
          ssize_t value_offset = buf.pin_bytes(value_size);
          if (value_offset < 0)
            goto end;
          
          
          if (btree_map->find(tree_id) == btree_map->end()) {
            fprintf(stderr, "THIS SHOULD NOT BE CALLED! tree_id is %lu, reading %s\n", (unsigned long) tree_id, file_name_value.c_str());
            mtx.lock();
            if (btree_map->find(tree_id) == btree_map->end()) {
              (*btree_map)[tree_id] = new concurrent_btree();
            }
            mtx.unlock();
          }
          
          btree_type *btr = (*btree_map)[tree_id];
          
        retry:
          if (replay) {
            key_type key((uint8_t*) buf.pinned_bytes(key_offset, key_size), key_size);
            versioned_value_type *record = NULL;
            std::string keystr(key);
            key_type value_ptr( (uint8_t*) (buf.pinned_bytes(value_offset, value_size)), value_size);
	    std::string valuestr(value_ptr);
            if (btr->get(key, record)) {
              // Key has been found, we can simple do updates in place
              btr->lock(record);
              uint64_t old_tid = btree_type::tid_from_version(record);
              
              if (old_tid < tid) {
                if (value_size == 0) {
                  btree_type::set_tid(record, tid);
                  if (!btree_type::is_invalid(record)) {
                    btree_type::make_invalid(record);
                  }
                } else {
                  if (btree_type::is_invalid(record)) {
                    btree_type::make_valid(record);
                  }
                  record->set_value(valuestr);
                  btree_type::set_tid(record, tid);
                }
              }
              
              btr->unlock(record);
              
            } else {
              if (value_size > 0) {
                record = btree_type::versioned_value_type::make(key.data(), valuestr, tid);
                btr->lock(record);
                if (! (btr->insert_if_absent(key, record))){
                  goto retry;
                }
                btr->unlock(record);
              } else {
                btr->get(key, record);
                record = btree_type::versioned_value_type::make(key.data(), valuestr, tid);
                btr->lock(record);
                btree_type::make_invalid(record);
                if (! (btr->insert_if_absent(key, record))){
                  goto retry;
                }
                btr->unlock(record);
                
              }
              
            }
          }
          
          buf.clear_pin();
        }
      }
    }
  end:
    close(fd);
  }
  
  std::cout << "Recovery thread " << th << " done" << std::endl;
  
}

void Recovery::replay_checkpoint(uint64_t ptid) {
  std::regex reg_data("ckp_(.*)");
  std::vector<std::string> disks;
  
  for (size_t i = 0; i < ckpdirs.size(); i++) {
    disks.insert(disks.end(), ckpdirs[i].begin(), ckpdirs[i].end());
  }
  
  //find all relevant checkpoint files
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(disks[0].c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      char *file_name = ent->d_name;
      std::cmatch cm;
      if (std::regex_match(file_name, cm, reg_data, std::regex_constants::format_default)) {
        char *tree_id = (char *) cm[1].str().c_str() + 1;
        uint64_t tree_id_uint = _char_to_uint64(tree_id);
        (*btree_map)[tree_id_uint] = new concurrent_btree();
        
        uint64_t tid = _read_min_tid(tree_id);
        min_recovery_tid = tid < min_recovery_tid ? tid : min_recovery_tid;
      }
    }
    
    closedir(dir);
  }
  
  // start one thread per directory
  std::vector<std::thread> ths;
  for (auto diskname : disks) {
    ths.emplace_back(&_read_checkpoint_file, diskname, ptid);
  }
  
  for (size_t i = 0; i < ths.size(); i++) {
    ths[i].join();
  }
}


uint64_t Recovery::_read_min_tid(char *tree_id) {
  uint64_t tid;
  int fd;
  
  std::string tid_file = "tid_"; // TODO: is this correct?
  tid_file.append(std::string(tree_id));
  fd = open((char *) tid_file.c_str(), O_RDONLY, S_IRUSR|S_IWUSR);
  
  ssize_t read_size = read(fd, (char *) &tid, 8);
  if (read_size < 8) {
    return std::numeric_limits<uint64_t>::max();
  }
  close(fd);
  return tid;
}


void Recovery::recover_ckp_thread(Recovery::btree_type* btree, std::string filename, uint64_t ptid) {
  printf("Par ckp reading file %s\n", filename.c_str());
  int fd = open((char *) filename.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("checkpoint replay fd open error\n");
    assert(false);
  }
  btree_type::thread_init();
  read_buffer buf(fd);
  
  bool replay = true;
  
  while (true) {
    // the keys are in order, so there shouldn't be collisions
    replay = true;
    
    buf.set_pin();
    
    uint64_t tid;
    if (!buf.read_64_s(&tid))
      break;
    
    uint64_t cur_tid = tid;
    if (cur_tid > ptid) {
      replay = false;
    }
    
    uint32_t key_size;
    if (!buf.read_32_vs(&key_size))
      break;
    
    ssize_t key_offset = buf.pin_bytes(key_size);
    if (key_offset < 0)
      break;
    
    uint32_t value_size;
    if (!buf.read_32_vs(&value_size))
      break;
    
    ssize_t value_offset = buf.pin_bytes(value_size);
    if (value_offset < 0)
      break;
    
    if (replay) {
      key_type key((uint8_t*) buf.pinned_bytes(key_offset, key_size), key_size);
      versioned_value_type *record = NULL;
      std::string value(buf.pinned_bytes(value_offset, value_size), value_size);
      record = btree_type::versioned_value_type::make(key.data(), value, tid);
      btree->insert(key, record);
    }
    
    buf.clear_pin();
  }
  close(fd);
  
}

void Recovery::_read_checkpoint_file(std::string diskname, uint64_t ptid) {
  int counter = 0;
  
  std::regex reg_data("ckp_(.*)");
  
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(diskname.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      char *file_name = ent->d_name;
      std::cmatch cm;
      if (std::regex_match(file_name, cm, reg_data, std::regex_constants::format_default)) {
        char* tree_id = (char *) cm[1].str().c_str() + 1;
        uint64_t tree_id_uint = _char_to_uint64(tree_id);
        btree_type *tree = (*btree_map)[tree_id_uint];
        recover_ckp_thread(tree, std::string(diskname).append(file_name), ptid);
        counter++;
      }
    }
  }
  closedir(dir);
}


// Read buffer methods

inline void read_buffer::shift() {
  uint8_t* save = pin_ ? pin_ : cur;
  if (size_t delta = save - head) {
    memmove(head, save, tail - save);
    cur = cur - delta;
    tail = tail - delta;
    pin_ = pin_ ? pin_ - delta : pin_;
  }
}

size_t read_buffer::load() {
  shift();
  if (fd >= 0) {
    ssize_t read_size = read(fd, tail, (head + sz_) - tail);
    if (read_size >= 0)
      tail += read_size;
  }
  return tail - cur;
}



