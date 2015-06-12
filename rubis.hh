#include "MassTrans_nd.hh"

class Rubis_nd {
  typedef MassTrans_nd<false> DB_type;
public:
  struct bid_entry {
    int item;
    std::string bidder;
    uint64_t bid;
  };

  Rubis_nd() {
    db = new DB_type();
    for (int i =0 ; i < 32; i++) {
      key_counters[i] = 0;
    } 
  }

  static void thread_init() {
    DB_type::thread_init();
  }
  
  static std::string getMaxBidKey(int item) {
    return "bid_" + std::to_string(item);
  }

  static std::string getMaxBidderKey(int item) {
    return "bidder_" + std::to_string(item);
  }

  static std::string getNumBidsKey(int item) {
    return "numBids_" + item;
  }

  void createItem(Transaction& t, int item) {
    db->ndtransMax(t, getMaxBidKey(item), 0);
    db->ndtransOPut(t, getMaxBidderKey(item), 0,  std::string(""));
    db->ndtransAdd(t, getNumBidsKey(item), 0);
  }

  void setBid(Transaction& t, int item, const std::string& bidder, uint64_t bid) {
    bid_entry* bid_ = new bid_entry;
    bid_->item = item;
    bid_->bidder = bidder;
    bid_->bid = bid;

    uint64_t localKey = key_counters[Transaction::threadid];
    key_counters[Transaction::threadid] += 1;
    std::string bidKey = "" + std::to_string(Transaction::threadid) + std::string("_") + std::to_string(localKey);

    db->transPut(t, bidKey, bid_);

    std::string maxBidKey = getMaxBidKey(item);
    db->ndtransMax(t, maxBidKey, bid);

    std::string maxBidderKey = getMaxBidderKey(item);
    db->ndtransOPut(t, maxBidderKey, bid, bidder);

    std::string numBidsKey = getNumBidsKey(item);
    db->ndtransAdd(t, numBidsKey, 1);
  }

  uint64_t getBid(Transaction& t, int item) {
    std::string maxBidKey = getMaxBidKey(item);
    void* retval;
    db->ndtransGet(t, maxBidKey, retval);
    return *((uint64_t*) retval);
  } 

  std::string getBidder(Transaction& t, int item) {
    std::string maxBidderKey = getMaxBidderKey(item);
    void* retval;
    db->ndtransGet(t, maxBidderKey, retval);
    return ((MassTrans_nd<false>::oput_entry*) retval)->value;
  }

  uint64_t getNumBids(Transaction& t, int item) {
    std::string numBidsKey = getNumBidsKey(item);
    void* retval;
    db->ndtransGet(t, numBidsKey, retval);
    return *((uint64_t*) retval);
  }

private:
  DB_type* db;
  uint64_t key_counters[32];
};

class Rubis {
  typedef MassTrans<void*> DB_type;
public:
  struct bid_entry {
    int item;
    std::string bidder;
    uint64_t bid;
  };

  Rubis() {
    db = new DB_type();
    for (int i =0 ; i < 32; i++) {
      key_counters[i] = 0;
    } 
  }

  static void thread_init() {
    DB_type::thread_init();
  }
  
  static std::string getMaxBidKey(int item) {
    return "bid_" + std::to_string(item);
  }

  static std::string getMaxBidderKey(int item) {
    return "bidder_" + std::to_string(item);
  }

  static std::string getNumBidsKey(int item) {
    return "numBids_" + item;
  }

  void createItem(Transaction& t, int item) {
    uint64_t* dflt_bid = new uint64_t;
    *dflt_bid = 0;
    std::string* dflt_bidder = new std::string;
    *dflt_bidder = "";
    uint64_t* dflt_numBids = new uint64_t;
    *dflt_numBids = 0;
    std::string maxBidKey = getMaxBidKey(item);
    std::string maxBidderKey = getMaxBidderKey(item);
    std::string numBidsKey = getNumBidsKey(item);    
    db->transPut(t, maxBidKey, dflt_bid);
    db->transPut(t, maxBidderKey, dflt_bidder);
    db->transPut(t, numBidsKey, dflt_numBids);
  }

  void setBid(Transaction& t, int item, const std::string& bidder, uint64_t bid) {
    bid_entry* bid_ = new bid_entry;
    bid_->item = item;
    bid_->bidder = bidder;
    bid_->bid = bid;

    uint64_t localKey = key_counters[Transaction::threadid];
    key_counters[Transaction::threadid] += 1;
    std::string bidKey = "" + std::to_string(Transaction::threadid) + std::string("_") + std::to_string(localKey);

    db->transPut(t, bidKey, bid_);

    std::string maxBidKey = getMaxBidKey(item);
    void* prevBid_ptr;
    db->transGet(t, maxBidKey, prevBid_ptr);
    uint64_t prevBid = *((uint64_t*) prevBid_ptr);

    if (bid > prevBid) {
      uint64_t* bid_ptr = new uint64_t;
      *bid_ptr = bid;
      db->transPut(t, maxBidKey, bid_ptr);

      std::string maxBidderKey = getMaxBidderKey(item);
      std::string* bidder_ptr = new std::string;
      *bidder_ptr =  bidder;
      db->transPut(t, maxBidderKey, bidder_ptr);
    }

    std::string numBidsKey = getNumBidsKey(item);
    void* prevNumBids_ptr;
    db->transGet(t, numBidsKey, prevNumBids_ptr);
    uint64_t prevNumBids = *((uint64_t*)prevNumBids_ptr);

    uint64_t* newNumBids_ptr = new uint64_t;
    *newNumBids_ptr = prevNumBids + 1;
    db->transPut(t, numBidsKey, newNumBids_ptr);
  }

  uint64_t getBid(Transaction& t, int item) {
    std::string maxBidKey = getMaxBidKey(item);
    void* retval;
    db->transGet(t, maxBidKey, retval);
    return *((uint64_t*) retval);
  }

  std::string getBidder(Transaction& t, int item) {
    std::string maxBidderKey = getMaxBidderKey(item);
    void* retval;
    db->transGet(t, maxBidderKey, retval);
    return *((std::string*) retval);
  }

  uint64_t getNumBids(Transaction& t, int item) {
    std::string numBidsKey = getNumBidsKey(item);
    void* retval;
    db->transGet(t, numBidsKey, retval);
    return *((uint64_t*) retval);
  }

private:
  DB_type* db;
  uint64_t key_counters[32];
};
