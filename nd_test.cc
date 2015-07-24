#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>
#include <map>

#include "Transaction.hh"
#include "clp.h"
#include "rubis.hh"

const int nthreads = 4;
int ntrans = 1000000;
int nitems = 5;
int nusers = 5;
int maxBid = 1000000000;
int opspertrans = 1;
typedef int value_type;

#define NON_DET 1

#if NON_DET
typedef Rubis_nd type;
#else
typedef Rubis type;
#endif

type* db;

void startAndWait();
void* run(void* x);
void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}
void check();

struct _bid {
  uint64_t bid;
  std::string bidder;
};

std::vector<std::map<int, _bid*>*> max_bid_maps;
std::vector<std::map<int, int>*> num_bids_maps;

int main(int argc, char *argv[]) {
    
    Transaction::epoch_advance_callback = [] (unsigned) {
        // just advance blindly because of the way Masstree uses epochs
        globalepoch++;
    };
    
    db = new type();
    type::thread_init();
    
    for (int i = 0; i < nthreads; i++) {
      max_bid_maps.push_back(new std::map<int, _bid*>);
      num_bids_maps.push_back(new std::map<int, int>);
    }
 
    // Prepopulate items with bid 0
    for (int i = 0; i < nitems; i++) {
        Transaction t;
        db->createItem(t, i);
        t.commit();
    }
    Transaction t;
    db->pushLocalData(t);
  
    struct timeval tv1,tv2;
    struct rusage ru1,ru2;
    gettimeofday(&tv1, NULL);
    getrusage(RUSAGE_SELF, &ru1);
    startAndWait();
    gettimeofday(&tv2, NULL);
    getrusage(RUSAGE_SELF, &ru2);
    
    printf("real time: ");
    print_time(tv1,tv2);
    
    printf("utime: ");
    print_time(ru1.ru_utime, ru2.ru_utime);
    printf("stime: ");
    print_time(ru1.ru_stime, ru2.ru_stime);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
    
    // Validation
    check();    
}

struct data {
  int id;
};

void startAndWait() {
    pthread_t tids[nthreads];
    data ids[nthreads];
    for (int i = 0; i < nthreads; i++) {
        ids[i].id = i;
        pthread_create(&tids[i], NULL, run, &ids[i]);
    }
    
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }
}

void* run(void* x) {
    int id = ((data*) x)->id;
    Transaction::threadid = id;
    
    type::thread_init();
    
    int N = ntrans/nthreads;
    int OPS = opspertrans;
    for(int i = 0; i < N; ++i) {
        
        bool done = false;
        while(!done) {
            try {
                uint32_t seed = i*3 + (uint32_t)id*N*7;
                srand(seed);
                
                Transaction& t = Transaction::get_transaction();
                //for (int j = 0; j < OPS; j++) {
                    int item = rand() % nitems;
		    int user = rand() % nusers;
                    std::string bidder = std::string("user") + std::to_string(user);
                    //int bid = (rand() % 100) + (maxBid * i / (N * 100)) ;
                    uint64_t curMax = db->getBid(t, item);
                    uint64_t bid = curMax + 10;
		    //std::cout << "Bidding on item " << item << " bid = " << bid << " cur max = " << curMax << std::endl;
		    if (bid > curMax) {
                        db->setBid(t, item, bidder, bid);
                    }
                //}
                done = t.try_commit();
                if (done) {
                  std::map<int, _bid*> local_max_bid_map = *max_bid_maps[id];
                  std::map<int, _bid*>::iterator it = local_max_bid_map.find(item);
		  if (it != local_max_bid_map.end()) {
 		    if (it->second->bid < bid ) {
                      it->second->bid = bid;
 		      it->second->bidder = bidder;
                    }
                  } else {
                    _bid* entry = new _bid;
		    entry->bid = bid;
		    entry->bidder = bidder;
	 	    local_max_bid_map[item] = entry; 	
                  }

		  std::map<int, int> local_num_bids_map = *num_bids_maps[id];
		  std::map<int, int>::iterator nit = local_num_bids_map.find(item);
		  if (nit != local_num_bids_map.end()) {
		    local_num_bids_map[item] = nit->second + 1;
                  } else {
	            local_num_bids_map[item] = 1;
                  }
                }
            }catch (Transaction::Abort E) {}
            
        }
    }
    
    return nullptr;
}

void check() {

  std::map<int, uint64_t> final_max_bid_map;
  std::map<int, int> final_num_bids_map;
  std::map<int, std::vector<std::string>> final_max_bidder_map;

  for (int i = 0; i < nthreads; i++) {
    std::map<int, _bid*> local_max_bid_map = *max_bid_maps[i];
    std::map<int, int> local_num_bids_map = *num_bids_maps[i];

    for (int item = 0; item < nitems; item++) {
      _bid* ent = local_max_bid_map[item];
      if (ent == NULL) continue;
      if (final_max_bid_map.find(item) == final_max_bid_map.end()) {
        std::cout << ent->bid << std::endl;
        final_max_bid_map[item] = ent->bid;
	std::vector<std::string> bidders_map;
	bidders_map.push_back(ent->bidder);
	final_max_bidder_map[item] = bidders_map;
        final_num_bids_map[item] = local_num_bids_map[item];
      } else {
	int prevBid = final_max_bid_map[item];
	int currBid = ent->bid;

        if (currBid == prevBid) {
	  final_max_bidder_map[item].push_back(ent->bidder);
        }
	if (currBid > prevBid) {
          final_max_bidder_map[item].clear();
	  final_max_bidder_map[item].push_back(ent->bidder);
        }
        final_num_bids_map[item] += local_num_bids_map[item];
      }
    }
  }
  
  for (int item = 0; item < nitems; item++) {
    Transaction t;
    uint64_t highest_bid = db->getBid(t, item);
    std::string highest_bidder = db->getBidder(t, item);
    uint64_t num_bids = db->getNumBids(t, item);
    t.commit();
    std::cout << "highest " <<  highest_bid << "exp " << final_max_bid_map[item] << std::endl;
    assert(highest_bid == final_max_bid_map[item]);
    std::vector<std::string> max_bidders = final_max_bidder_map[item];
#if !NON_DET
    assert(max_bidders.size() == 1);
#endif
   
    assert(std::find(max_bidders.begin(), max_bidders.end(), highest_bidder) != max_bidders.end());
    assert(num_bids == final_num_bids_map[item]);
  }
}
