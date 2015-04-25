#include <stdio.h>
#include <vector>
#include <string>


// this defines some parameters that can be used with logging, checkpointing, recovery

// variables, followed by the old corresponding macros
extern int enable_ckp; // CKP
extern int enable_ckp_compress; // CKP_COMPRESS
extern int enable_datastripe; // DATASTRIPE
extern int enable_datastripe_par; // DATASTRIPE_PAR
extern int enable_par_ckp; // PAR_CKP
extern int enable_par_log_replay; // PAR_LOG_REPLAY
extern std::vector<std::vector<std::string> > ckpdirs; // a list of list of directories. Each list is supposed
// to be assigned to a different checkpoint thread

extern int reduced_ckp;
extern std::string root_folder; // this folder stores pepoch, cepoch, and other information
extern int nckp_threads;
extern int ckp_compress;
extern std::string sockfile;
extern int run_for_ckp; 
extern int recovery_test;
