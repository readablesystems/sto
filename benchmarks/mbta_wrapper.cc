#include "mbta_wrapper.h"
__thread str_arena* mbta_wrapper::thr_arena;

std::string *mbta_ordered_index::arena() {
  return (*db->thr_arena)();
}

mbta_wrapper::mbta_wrapper(const std::vector<std::string> &logfiles,
        const std::vector<std::vector<unsigned>> & assignments_given,
        bool call_fsync,
        bool use_compression,
        bool fake_writes) {
    if (logfiles.empty()) return;
    std::vector<std::vector<unsigned>> assignments_used;
    
    Logger::Init(nthreads + 1, logfiles, assignments_given, &assignments_used,
        call_fsync, use_compression, fake_writes);
        
    if (verbose) {
      std::cerr << "[logging subsystem]" << std::endl;
      std::cerr << "  assignments: " << assignments_used << std::endl;
      std::cerr << "  call fsync : " << call_fsync       << std::endl;
      std::cerr << "  compression: " << use_compression  << std::endl;
      std::cerr << "  fake_writes: " << fake_writes      << std::endl;
    } 

  }
