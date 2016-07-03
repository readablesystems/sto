#pragma once

#include <queue>
#include "FlatCombining.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

template <typename T, class PriorityQueue = std::priority_queue<T>, 
         bool Opacity = false, template <typename> class W = TOpaqueWrapped>
class FCPriorityQueue: public Shared, public flat_combining::container {
public:
    typedef T               value_type;          ///< Value type
    typedef PriorityQueue   priority_queue_type; ///< Sequential priority queue class
    typedef typename W<value_type>::version_type version_type;

protected:
    // Priority queue operation IDs
    enum fc_operation {
        op_push = flat_combining::req_Operation,
        op_pop,
        op_top,
        op_clear,
        op_empty
    };

    // Flat combining publication list record
    struct fc_record: public flat_combining::publication_record
    {
        union {
            value_type const *  pValPush; // Value to push
            value_type *        pValTop;  // Top destination
        };
        bool            is_empty; // true if the queue is empty
    };

    typedef flat_combining::kernel< fc_record> fc_kernel;

    fc_kernel               fc_kernel_;
    priority_queue_type     pq_;
    version_type pqversion_;

public:
    /// Initializes empty priority queue object
    FCPriorityQueue() : pqversion_(0) {}

    /// Initializes empty priority queue object and gives flat combining parameters
    FCPriorityQueue(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : fc_kernel_( nCompactFactor, nCombinePassCount ), pqversion_(0)
    {}

    /// Inserts a new element in the priority queue
    bool push(value_type const& val) {
        fc_record * pRec = fc_kernel_.acquire_record();
        pRec->pValPush = &val;

        fc_kernel_.combine( op_push, pRec, *this );

        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        
        Sto::item(this,-1).add_write(val);
        return true;
    }

    /// Removes the top element from priority queue
    value_type top() {
        value_type val;
        fc_record * pRec = fc_kernel_.acquire_record();
        pRec->pValTop = &val;

        fc_kernel_.combine( op_top, pRec, *this );

        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );

        if (pRec->is_empty) Sto::item(this,-1).observe(pqversion_);
        return val;
    }

    /// Removes the top element from priority queue
    bool pop() {
        fc_record * pRec = fc_kernel_.acquire_record();

        fc_kernel_.combine( op_pop, pRec, *this );

        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );

        if (!pRec->is_empty) Sto::item(this,0).add_write(0);
        else Sto::item(this,-1).observe(pqversion_);
        return !pRec->is_empty;
    }

    /// Clears the priority queue
    void clear()
    {
        fc_record * pRec = fc_kernel_.acquire_record();

        fc_kernel_.combine( op_clear, pRec, *this );

        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
    }

    /// Returns the number of elements in the priority queue.
    size_t unsafe_size() const {
        return pq_.size();
    }

    /// Checks if the priority queue is empty
    bool empty() {
        fc_record * pRec = fc_kernel_.acquire_record();

        fc_kernel_.combine( op_empty, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        return pRec->is_empty;
    }

public: // flat combining cooperation, not for direct use!
    void fc_apply( fc_record * pRec ) {
        assert( pRec );

        // this function is called under FC mutex, so switch TSan off
        CDS_TSAN_ANNOTATE_IGNORE_RW_BEGIN;

        switch ( pRec->op() ) {
        case op_push:
            assert( pRec->pValPush );
            pq_.push( *(pRec->pValPush) );
            break;
        case op_pop:
            pRec->is_empty = pq_.empty();
            if ( !pRec->is_empty ) {
                pq_.pop();
            }
            break;
        case op_top:
            assert( pRec->pValTop );
            pRec->is_empty = pq_.empty();
            if ( !pRec->is_empty ) {
                *(pRec->pValTop) = std::move( pq_.top());
            }
            break;
        case op_clear:
            while ( !pq_.empty() )
                pq_.pop();
            break;
        case op_empty:
            pRec->is_empty = pq_.empty();
            break;
        default:
            assert(false);
            break;
        }

        CDS_TSAN_ANNOTATE_IGNORE_RW_END;
    }
private:
    bool lock(TransItem& item, Transaction& txn) override {
        if ((item.key<int>() == -1) && !pqversion_.is_locked_here())  {
            return txn.try_lock(item, pqversion_); 
        } 
        return true;
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        // check if we read off the write_list. We should only abort if both: 
        //      1) we saw the queue was empty during a pop/front and read off our own write list 
        //      2) someone else has pushed onto the queue before we got here
        if (item.key<int>() == -1)
            return item.check_version(pqversion_);
        // shouldn't reach this
        assert(0);
        return false;
    }

    void install(TransItem& item, Transaction& txn) override {
        // install pops if the txn marked a value as deleted
        if (item.key<int>() == 0) {
        }
        // install pushes
        if (item.key<int>() == -1) {
        }
        // set pqversion appropriately
        if (!pqversion_.is_locked_here()) {
            pqversion_.lock();
            pqversion_.set_version(txn.commit_tid());
        }    
    }
    
    void unlock(TransItem& item) override {
        (void)item;
        if (pqversion_.is_locked_here()) {
            pqversion_.unlock();
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        (void)item;
        (void)committed;
        if (pqversion_.is_locked_here()) {
            pqversion_.unlock();
        }
    }
};
