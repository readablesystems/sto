#include <string>
#include <iostream>
#include <sys/time.h>
#include <assert.h>
#include <sys/resource.h>
#include <vector>
#include "Transaction.hh"
#include "TIntRange.hh"
#include "TWrapped.hh"
#include "randgen.hh"
#include "clp.h"
#define GUARDED if (TransactionGuard tguard{})
unsigned initial_seeds[64];
unsigned ops_per_trans = 1;
double usleep_fraction = 0.1;

struct results_t {
    uint64_t trans, a, b, c, rbuf_find;
    struct timeval finished;
} results[32];

void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

class TCounter0 : public TObject {
    TWrapped<int> n_;
    TVersion v_;
public:
    TCounter0(int n = 0)
        : n_(n) {
    }
    int nontrans_access() {
        return n_.access();
    }
    void increment() {
        TransProxy it = Sto::item(this, 0);
        int n = it.has_write() ? it.write_value<int>() : n_.read(it, v_);
        it.add_write(n + 1);
    }
  void decrement() {
        TransProxy it = Sto::item(this, 0);
        int n = it.has_write() ? it.write_value<int>() : n_.read(it, v_);
        it.add_write(n - 1);
  }
  bool test() const {
     TransProxy it = Sto::item(this, 0);
     return (it.has_write() ? it.write_value<int>() : n_.read(it, v_)) > 0;
  }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, v_);
    }
    bool check(TransItem& it, Transaction&) override {
        return it.check_version(v_);
    }
    void install(TransItem& it, Transaction& txn) override {
        n_.access() = it.write_value<int>();
        txn.set_version_unlock(v_, it);
    }
    void unlock(TransItem&) override {
        v_.unlock();
    }

    void print(std::ostream& w) const {
        w << "{TCounter0 " << n_.access() << " V" << v_ << "}";
    }
    friend std::ostream& operator<<(std::ostream& w, const TCounter0& tc) {
        tc.print(w);
        return w;
    }
};

class TCounter1 : public TObject {
    TWrapped<int> n_;
    TVersion v_;
    static int wval(TransProxy it) {
       return it.write_value<int>(0);
    }
    static int wval(const TransItem& it) {
       return it.write_value<int>(0);
    }
public:
    TCounter1(int n = 0)
        : n_(n) {
    }
    int nontrans_access() {
        return n_.access();
    }
  void increment() {
     auto it = Sto::item(this, 0);
     it.add_write(wval(it) + 1);
  }
  void decrement() {
     auto it = Sto::item(this, 0);
     it.add_write(wval(it) - 1);
  }
  bool test() const {
     auto it = Sto::item(this, 0);
     int n = n_.read(it, v_);
     return n + wval(it) > 0;
  }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, v_);
    }
    bool check(TransItem& it, Transaction&) override {
        return it.check_version(v_);
    }
    void install(TransItem& it, Transaction& txn) override {
        n_.access() += wval(it);
        txn.set_version_unlock(v_, it);
    }
    void unlock(TransItem&) override {
        v_.unlock();
    }

    void print(std::ostream& w) const {
        w << "{TCounter1 " << n_.access() << " V" << v_ << "}";
    }
    friend std::ostream& operator<<(std::ostream& w, const TCounter1& tc) {
        tc.print(w);
        return w;
    }
};

class TCounter2 : public TObject {
    TWrapped<int> n_;
    TVersion v_;
    //unsigned next_cache_line_[17];
    TWrapped<int> zc_n_;
    TVersion zc_v_;
    static int wval(TransProxy it) {
        return it.write_value<int>(0);
    }
    static int wval(const TransItem& it) {
        return it.write_value<int>(0);
    }
    static constexpr TransItem::flags_type zc_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type zc_set_bit = zc_bit << 1;
public:
    TCounter2(int n = 0)
        : n_(n), zc_n_(n) {
    }
    int nontrans_access() {
        return n_.access();
    }
    void increment() {
        auto it = Sto::item(this, 0);
        it.add_write(wval(it) + 1);
    }
    void decrement() {
        auto it = Sto::item(this, 0);
        it.add_write(wval(it) - 1);
    }
    bool test() const {
        auto it = Sto::item(this, 0);
        if (!it.has_write()) {
            it.add_flags(zc_bit);
            return zc_n_.read(it, zc_v_) > 0;
        } else { /* assume opacity */
            if (it.has_flag(zc_bit))
                it.clear_flags(zc_bit).clear_read();
            return n_.read(it, v_) + wval(it) > 0;
        }
    }

    bool lock(TransItem& it, Transaction& txn) override {
        bool ok = txn.try_lock(it, v_);
        if (ok) {
            int n = n_.access();
            if ((n > 0) != (n + wval(it) > 0)) {
                zc_v_.value() |= (v_.value() & (TransactionTid::nonopaque_bit - 1));
                it.add_flags(zc_set_bit);
            }
        }
        return ok;
    }
    bool check(TransItem& it, Transaction&) override {
        return it.check_version(it.has_flag(zc_bit) ? zc_v_ : v_);
    }
    void install(TransItem& it, Transaction& txn) override {
        n_.access() += wval(it);
        if (it.has_flag(zc_set_bit)) {
            zc_n_.access() = n_.access();
            txn.set_version_unlock(zc_v_, it);
        }
        txn.set_version_unlock(v_, it);
    }
    void unlock(TransItem& it) override {
        if (it.has_flag(zc_set_bit))
            zc_v_.unlock();
        v_.unlock();
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TCounter2";
        if (item.has_flag(zc_bit))
            w << ".ZC";
        if (item.has_read())
            w << " R" << item.read_value<TVersion>();
        if (item.has_write())
            w << " Δ" << item.write_value<int>();
        if (item.has_flag(zc_set_bit))
            w << "ZC";
        w << "}";
    }
    void print(std::ostream& w) const {
        w << "{TCounter2 " << n_.access() << " V" << v_ << " ZCV" << zc_v_ << "}";
    }
    friend std::ostream& operator<<(std::ostream& w, const TCounter2& tc) {
        tc.print(w);
        return w;
    }
};

class TCounter3 : public TObject {
    TWrapped<int> n_;
    TVersion v_;
    typedef TIntRange<int> pred_type;
    static int wval(TransProxy it) {
        return it.write_value<int>(0);
    }
    static int wval(const TransItem& it) {
        return it.write_value<int>(0);
    }
public:
    TCounter3(int n = 0)
        : n_(n) {
    }
    int nontrans_access() {
        return n_.access();
    }
    void increment() {
        auto it = Sto::item(this, 0);
        it.add_write(wval(it) + 1);
    }
    void decrement() {
        auto it = Sto::item(this, 0);
        it.add_write(wval(it) - 1);
    }
    bool test() const {
        TransProxy it = Sto::item(this, 0);
        if (!it.has_predicate())
            it.set_predicate(pred_type::unconstrained());
        int n = n_.snapshot(it, v_);
        bool gt = n + wval(it) > 0;
        it.predicate_value<pred_type>().observe_gt(-wval(it), gt);
        return gt;
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, v_);
    }
    bool check_predicate(TransItem& item, Transaction& txn, bool committing) override {
        TransProxy p(txn, item);
        pred_type pred = item.template predicate_value<pred_type>();
        int n = n_.wait_snapshot(p, v_, committing);
        return pred.verify(n);
    }
    bool check(TransItem& it, Transaction&) override {
        return it.check_version(v_);
    }
    void install(TransItem& it, Transaction& txn) override {
        n_.access() += wval(it);
        txn.set_version_unlock(v_, it);
    }
    void unlock(TransItem&) override {
        v_.unlock();
    }

    void print(std::ostream& w) const {
        w << "{TCounter3 " << n_.access() << " V" << v_ << "}";
    }
    friend std::ostream& operator<<(std::ostream& w, const TCounter3& tc) {
        tc.print(w);
        return w;
    }
};


static int initial_value = -100;
static double test_fraction = 0.5;
static int nthreads = 4;
static double exptime = 10;

enum { opt_nthreads = 1, opt_test_fraction, opt_initial_value, opt_seed,
       opt_ops_per, opt_time, opt_usleep_fraction };

static const Clp_Option options[] = {
  { "nthreads", 'j', opt_nthreads, Clp_ValInt, 0 },
  { "test-fraction", 'f', opt_test_fraction, Clp_ValDouble, 0 },
  { "initial-value", 'i', opt_initial_value, Clp_ValInt, 0 },
  { "opspertrans", 'o', opt_ops_per, Clp_ValUnsigned, 0 },
  { "time", 't', opt_time, Clp_ValDouble, 0 },
  { "usleep-fraction", 'u', opt_usleep_fraction, Clp_ValDouble, 0 },
  { "seed", 0, opt_seed, Clp_ValUnsigned, 0 }
};


struct result {
    uint64_t ngt;
    int final_value;
};

class Tester {
public:
    virtual result run() = 0;
};

template <typename T>
class TTester : public Tester {
public:
    static T* counter;
    static volatile unsigned go;
    static void* runfunc(void*);
    result run();
};

template <typename T> T* TTester<T>::counter;
template <typename T> volatile unsigned TTester<T>::go;

template <typename T>
void* TTester<T>::runfunc(void* arg) {
    int me = (int) (uintptr_t) arg;
    T* counter = TTester<T>::counter;
    TThread::set_id(me);
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
    unsigned test_threshold = test_fraction * transgen.max();
    unsigned increment_threshold = (0.499 * test_fraction + 0.501) * transgen.max();
    unsigned usleep_threshold = usleep_fraction * transgen.max();

    while (!go)
        relax_fence();
    uint64_t trans = 0, a = 0, b = 0, c = 0, count_test = 0, rbuf_find = 0, retry = 0;

    while (go) {
        Rand snap_transgen = transgen;
        unsigned na, nb, nc, ngt, nfind;
        while (1) {
            na = nb = nc = ngt = nfind = 0;
            ++retry;
            try {
                Sto::start_transaction();
                for (unsigned k = 0; k < ops_per_trans; ++k) {
                    unsigned op = transgen();
                    if (op < test_threshold) {
                        ngt += counter->test();
                        ++na;
                    } else if (op < increment_threshold) {
                        counter->increment();
                        ++nb;
                    } else {
                        counter->decrement();
                        ++nc;
                    }
                }
                if (transgen() < usleep_threshold)
                    usleep(1);
                if (Sto::try_commit())
                    break;
            } catch (Transaction::Abort e) {
            }
            transgen = snap_transgen;
        }
        a += na;
        b += nb;
        c += nc;
        count_test += ngt;
        rbuf_find += nfind;
        ++trans;
    }

    results[me].trans = trans;
    results[me].a = a;
    results[me].b = b;
    results[me].c = c;
    results[me].rbuf_find = rbuf_find;
    gettimeofday(&results[me].finished, NULL);
    return reinterpret_cast<void*>(count_test);
}

static bool has_epoch_advancer = false;

template <typename T>
result TTester<T>::run() {
    counter = new T(initial_value);
    pthread_t tids[nthreads];
    go = 0;

    for (uintptr_t i = 0; i < uintptr_t(nthreads); ++i)
        pthread_create(&tids[i], NULL, runfunc, reinterpret_cast<void*>(i));

    if (!has_epoch_advancer) {
        pthread_t advancer;
        pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
        pthread_detach(advancer);
        has_epoch_advancer = true;
    }

    go = 1;
    usleep(exptime * 1000000);
    go = 0;
    uint64_t total = 0;
    for (int i = 0; i < nthreads; ++i) {
        void* mine;
        pthread_join(tids[i], &mine);
        total += reinterpret_cast<uintptr_t>(mine);
    }
    std::cout << *counter << '\n';
    return result{total, counter->nontrans_access()};
}

static Tester* ttesters[] = { new TTester<TCounter0>, new TTester<TCounter1>,
    new TTester<TCounter2>, new TTester<TCounter3> };

int main(int argc, char *argv[]) {
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
  srandomdev();
  std::vector<int> tests;
  unsigned seed = 0;

  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
    case opt_nthreads:
      nthreads = clp->val.i;
      break;
    case opt_ops_per:
      ops_per_trans = clp->val.u;
      break;
    case opt_test_fraction:
      test_fraction = clp->val.d;
      break;
    case opt_initial_value:
      initial_value = clp->val.i;
      break;
    case opt_seed:
        seed = clp->val.u;
        break;
    case opt_time:
        exptime = clp->val.d;
        break;
    case opt_usleep_fraction:
        usleep_fraction = clp->val.d;
        break;
    case Clp_NotOption: {
        int testnum = atoi(clp->vstr);
        assert(testnum >= 0 && testnum <= 3);
        tests.push_back(testnum);
        break;
    }
    default:
      assert(0);
    }
  }
  Clp_DeleteParser(clp);

  if (seed)
      srandom(seed);
  else
      srandomdev();
  for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
      initial_seeds[i] = random();

    if (tests.empty())
        tests.push_back(1);
    for (auto testnum : tests) {
        struct timeval tv1,tv2;
        struct rusage ru1,ru2;
        gettimeofday(&tv1, NULL);
        getrusage(RUSAGE_SELF, &ru1);
        result r = ttesters[testnum]->run();
        gettimeofday(&tv2, NULL);
        getrusage(RUSAGE_SELF, &ru2);

        uint64_t total_trans = 0;
        for (int i = 0; i != nthreads; ++i)
            total_trans += results[i].trans;
        printf("THROUGHPUT: %f TRANS/SEC\n", total_trans / exptime);

        printf("  real time: ");
        print_time(tv1,tv2);
        printf(", utime: ");
        print_time(ru1.ru_utime, ru2.ru_utime);
        printf(", stime: ");
        print_time(ru1.ru_stime, ru2.ru_stime);

        printf("\n  ./ex-counter -j%d -t%g -f%g -u%g -i%d -o%u %d\n",
               nthreads, exptime, test_fraction, usleep_fraction,
               initial_value, ops_per_trans, testnum);
        printf("  test() true %llu, value %d\n", (unsigned long long) r.ngt, r.final_value);
        printf("  duration: ");
        for (int i = 0; i != nthreads; ++i) {
            printf(i ? ", %llu/" : "%llu/", (unsigned long long) results[i].trans);
            print_time(tv1, results[i].finished);
        }
        printf("\n");
#if STO_PROFILE_COUNTERS
        Transaction::print_stats();
        if (txp_count >= txp_total_aborts) {
            txp_counters tc = Transaction::txp_counters_combined();
            const char* sep = "";
            if (txp_count > txp_total_w) {
                printf("%stotal_n: %llu, total_r: %llu, total_w: %llu", sep, tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w));
                sep = ", ";
            }
            if (txp_count > txp_total_searched) {
                printf("%stotal_searched: %llu", sep, tc.p(txp_total_searched));
                sep = ", ";
            }
            if (txp_count > txp_total_aborts) {
                printf("%stotal_aborts: %llu (%llu aborts at commit time)\n", sep, tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
                sep = ", ";
            }
            if (*sep)
                printf("\n");
        }
        Transaction::clear_stats();
#endif
    }
}
