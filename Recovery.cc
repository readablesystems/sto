#include "Recovery.hh"

typedef Recovery::txn_btree_map_type txn_btree_map_type;

txn_btree_map_type * Recovery::btree_map = new txn_btree_map_type();
uint64_t Recovery::min_recovery_epoch = std::numeric_limits<uint64_t>::max();
std::mutex Recovery::mtx;

txn_btree_map_type * Recovery::recover(std::vector<std::string> logfile_base, uint64_t test_epoch) {
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
  
  uint64_t ckp_epoch = 0;
  uint64_t persist_epoch = 0;
  
  // find the checkpoint epoch
  std::cout << root_folder << std::endl;
  
  if (test_epoch == 0) {
    // find the persistent epoch
    int fd = open(std::string(root_folder).append("/pepoch").c_str(), O_RDONLY);
    if (fd > 0) {
      if (read(fd, (char *) &persist_epoch, 8) < 0) {
        perror("Persist epoch reading failed");
        assert(false);
      }
      close(fd);
    } else {
      perror("pepoch file not found");
      assert(false);
    }
  } else {
    persist_epoch = test_epoch;
  }
  
  std::cout << "persist_epoch is " << persist_epoch << std::endl;
  
  int fd = open(std::string(root_folder).append("/cepoch").c_str(), O_RDONLY);
  if (fd > 0) {
    if (read(fd, (char *) &ckp_epoch, 8) < 0) {
      perror("Checkpoint epoch reading failed");
      assert(false);
    }
    close(fd);
  } else {
    perror("[recover] cepoch file not found");
    goto log_replay;
  }

  replay_checkpoint(persist_epoch);
  
log_replay:
  for (size_t i = 0; i < logfile_base.size(); i++) {
    uint64_t all_max_epoch = 0;
    std::map<uint64_t, std::string> map_list;
    
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(logfile_base[i].c_str())) != NULL) {
      while ((ent = readdir(dir)) != NULL) {
        char* file_name = ent->d_name;
        std::cmatch cm;
        if (std::regex_match(file_name, cm, log_reg, std::regex_constants::format_default)) {
          char *max_epoch_char = (char *) cm[1].str().c_str();
          uint64_t max_epoch = _char_to_uint64(max_epoch_char);
          if (max_epoch < ckp_epoch) {
            continue;
          }
          std::string f(logfile_base[i]);
          f.append(std::string(file_name));
    
          map_list[max_epoch] = f;
          all_max_epoch = (max_epoch > all_max_epoch) ? max_epoch : all_max_epoch;
        }
      }
      closedir(dir);
    }
    
    std::string s(logfile_base[i]);
    file_list[i*NTHREADS_PER_DISK]->push_back(s.append("data.log"));
    
    std::map<uint64_t, std::string>::reverse_iterator rit = map_list.rbegin(); // reverse order of max_epoch
    int file_list_idx = 0;
    for(; rit != map_list.rend(); rit++) {
      file_list[file_list_idx + i * NTHREADS_PER_DISK]->push_back(rit->second);
      file_list_idx = (file_list_idx + 1) % NTHREADS_PER_DISK;
    }
  }
  
  for (size_t i = 0; i < logfile_base.size(); i++) {
    for (int j = 0; j < NTHREADS_PER_DISK; j++) {
      threads.emplace_back(&replay_logs, *(file_list[i * NTHREADS_PER_DISK + j]), ckp_epoch,
                           persist_epoch, i * NTHREADS_PER_DISK + j,
                           i);
      //replay_logs(*(file_list[i * NTHREADS_PER_DISK + j]), ckp_epoch,
       //                                 persist_epoch, i * NTHREADS_PER_DISK + j,
       //           i);
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
                           uint64_t ckp_epoch,
                           uint64_t persist_epoch,
                           size_t th,
                           int index) {
  btree_type::thread_init();
  std::cout << "Recover thread " << th <<" starting " << std::endl;
  if (file_list.size() == 0) {
    printf("No log files found\n");
    return;
  } else {
    printf("Log files found\n");
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
      uint64_t epoch = 0;
      if (!buf.read_64_s(&nentries))
        goto end;
      if (!buf.read_64_s(&last_tid))
        goto end;
      
      if (!buf.read_64_s(&epoch))
        goto end;
      
      for (uint64_t n = 0; n < nentries; n++) { // for each transaction
        bool replay = true;
        uint64_t tid = 0;
        if (!buf.read_64_s(&tid)) // transaction tid
          goto end;
        uint64_t cur_epoch = tid;
        if (cur_epoch > persist_epoch || cur_epoch < ckp_epoch) {
          // we should not replay this transaction if it's bigger than the persistent epoch,
          // and we don't need to replay if it's smaller than the checkpoint epoch
          replay = false;
        }
        
        uint32_t num_records = 0;
        if (!buf.read_32_vs(&num_records))
          goto end;
        
        for (uint32_t i = 0; i < num_records; i++) { // For each record
          buf.set_pin(); // what does this do?
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
              (*btree_map)[tree_id] = new tree_type();
            }
            mtx.unlock();
          }
          
          btree_type *btr = (*btree_map)[tree_id];
          
        retry:
          if (replay) {
            key_type key((uint8_t*) buf.pinned_bytes(key_offset, key_size), key_size);
            versioned_value_type *record = NULL;
            std::string keystr(key);
            int* value_ptr = (int*) (buf.pinned_bytes(value_offset, value_size));
            if (btr->get(key, record)) {
              // Key has been found, we can simple do updates in place
              btr->lock(record);
              uint64_t old_tid = btree_type::tid_from_version(record);
              
              if (old_tid < tid) {
                if (value_size == 0) {
                  //std::cout << "remove (found) for key " << key.data() << " for tree " << tree_id << std::endl;
                  btree_type::set_tid(record, tid);
                  if (!btree_type::is_invalid(record)) {
                    btree_type::make_invalid(record);
                  }
                  // bool success = btr->remove(key);
                  //assert(success);
                } else {
                  if (btree_type::is_invalid(record)) {
                    btree_type::make_valid(record);
                  }
                  record->set_value(*value_ptr);
                  btree_type::set_tid(record, tid);
                }
              }
              
              btr->unlock(record);
              
            } else {
              if (value_size > 0) {
                record = btree_type::versioned_value_type::make(key.data(), *value_ptr, tid);
                btr->lock(record);
                if (! (btr->insert_if_absent(key, record))){
                  goto retry;
                   // TODO: should we destroy the record
                }
                btr->unlock(record);
              } else {
                //std::cout << "remove for key " << key.data() << " for tree " << tree_id << std::endl;
                btr->get(key, record);
                //std::cout << " Reached here " <<std::endl;
                // Do nothing
                record = btree_type::versioned_value_type::make(key.data(), *value_ptr, tid);
                btr->lock(record);
                btree_type::make_invalid(record);
                if (! (btr->insert_if_absent(key, record))){
                  goto retry;
                  // TODO: should we destroy the record
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
  
  printf("Done with all of the rlrs, enquing last block\n");
  std::cout << "Recovery thread " << th << " done" << std::endl;
  
}

void Recovery::replay_checkpoint(uint64_t pepoch) {
  std::regex reg_data("ckp_(.*)");
  std::regex reg_epoch("epoch_(.*)");
  
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
        char *tree_id = (char *) cm[1].str().c_str();
        uint64_t tree_id_uint = _char_to_uint64(tree_id);
        (*btree_map)[tree_id_uint] = new tree_type();
        
        uint64_t epoch = _read_min_epoch(tree_id);
        min_recovery_epoch = epoch < min_recovery_epoch ? epoch : min_recovery_epoch;
      }
    }
    
    closedir(dir);
  }
  
  // start one thread per directory
  std::vector<std::thread> ths;
  for (auto diskname : disks) {
    ths.emplace_back(&_read_checkpoint_file, diskname, pepoch);
    //_read_checkpoint_file(diskname, pepoch);
  }
  
  for (size_t i = 0; i < ths.size(); i++) {
    ths[i].join();
  }
}


uint64_t Recovery::_read_min_epoch(char *tree_id) {
  uint64_t epoch;
  int fd;
  
  std::string epoch_file = "epoch_";
  epoch_file.append(std::string(tree_id));
  fd = open((char *) epoch_file.c_str(), O_RDONLY, S_IRUSR|S_IWUSR);
  
  ssize_t read_size = read(fd, (char *) &epoch, 8);
  if (read_size < 8) {
    return std::numeric_limits<uint64_t>::max();
  }
  close(fd);
  return epoch;
}


void Recovery::recover_ckp_thread(Recovery::btree_type* btree, std::string filename, uint64_t pepoch) {
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
    
    uint64_t cur_epoch = tid;
    if (cur_epoch > pepoch) {
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
      record = btree_type::versioned_value_type::make(key.data(), std::atoi(value.c_str()), tid);
      btree->insert(key, record);
    }
    
    buf.clear_pin();
  }
  close(fd);
  
}

void Recovery::_read_checkpoint_file(std::string diskname, uint64_t pepoch) {
  int counter = 0;
  
  std::regex reg_data("ckp_(.*)");
  
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(diskname.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      char *file_name = ent->d_name;
      std::cmatch cm;
      if (std::regex_match(file_name, cm, reg_data, std::regex_constants::format_default)) {
        char* tree_id = (char *) cm[1].str().c_str();
        uint64_t tree_id_uint = _char_to_uint64(tree_id);
        btree_type *tree = (*btree_map)[tree_id_uint];
        recover_ckp_thread(tree, std::string(diskname).append(file_name), pepoch);
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



