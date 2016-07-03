/*
    This file is a part of libcds - Concurrent Data Structures library

    (C) Copyright Maxim Khizhinsky (libcds.dev@gmail.com) 2006-2016

    Source code repo: http://github.com/khizmax/libcds/
    Download: http://sourceforge.net/projects/libcds/files/
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.     
*/

#ifndef FCQUEUE_H 
#define FCQUEUE_H

#include <deque>
#include <list>
#include "FlatCombining.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

#define LOCKQV 1

// Tells the combiner thread the flags associated with each item in the q
template <typename T>
struct val_wrapper {
    T val;
    uint8_t flags; 
};

template <typename T, 
         class Queue = std::deque<val_wrapper<T>>,
         template <typename> class W = TOpaqueWrapped>
class FCQueue : public flat_combining::container, public Shared
{
public:
    typedef T           value_type;     ///< Value type
    typedef Queue       queue_type;     ///< Sequential queue class

    // STO
    typedef typename W<value_type>::version_type version_type;

    // For thread-specific txns
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<0;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<2;
   
    // For the publication list records
    static constexpr uint8_t delete_bit = 1<<0;
    static constexpr uint8_t popped_bit = 1<<1;

private:
    // Queue operation IDs
    enum fc_operation {
        op_push = flat_combining::req_Operation, // Push
        op_pop,         // Pop (mark as poisoned)
        op_clear,       // Clear
        op_empty        // Empty
    };

    // Flat combining publication list record
    struct fc_record: public flat_combining::publication_record
    {
        union {
            val_wrapper<value_type> const *  pValPush; // Value to enqueue
            val_wrapper<value_type> *        pValPop;  // Pop destination
        };
        bool            is_empty; // true if the queue is empty
    };

    flat_combining::kernel<fc_record> fc_kernel_;
    queue_type  q_;
    version_type queueversion_;

    std::atomic<int> num_mark_iter;
    std::atomic<int> num_mark_tries;
    std::atomic<int> num_marked;
    std::atomic<int> num_clear_tries;
    std::atomic<int> num_cleared;
    std::atomic<int> num_install_iter;
    std::atomic<int> num_install_tries;
    std::atomic<int> num_installed;
    std::atomic<int> num_undone;
    std::atomic<int> num_undo_tries;

public:
    /// Initializes empty queue object
    FCQueue() : queueversion_(0),
                num_mark_iter(0), num_mark_tries(0), num_marked(0),
                num_clear_tries(0), num_cleared(0),
                num_install_iter(0), num_install_tries(0), num_installed(0),
                num_undone(0), num_undo_tries(0)
    {}

    /// Initializes empty queue object and gives flat combining parameters
    FCQueue(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : fc_kernel_( nCompactFactor, nCombinePassCount ), queueversion_(0),
            num_mark_iter(0), num_mark_tries(0), num_marked(0),
            num_clear_tries(0), num_cleared(0),
            num_install_iter(0), num_install_tries(0), num_installed(0),
            num_undone(0), num_undo_tries(0) {}

    ~FCQueue() { 
        fprintf(stderr, "Iter Depth / Attempts:\n\
                Marked: %d / %d\t Successful: %d\n\
                Install: %d / %d\t Successful: %d\n\
                Clear Attempts: %d\t Successful: %d\n\
                Undo Attempts: %d\t Successful: %d\n",
                int(num_mark_iter), int(num_mark_tries), int(num_marked),
                int(num_install_iter), int(num_install_tries), int(num_installed),
                int(num_clear_tries), int(num_cleared),
                int(num_undo_tries), int(num_undone));
    }

    // Adds an item to the write list of the txn
    bool push( value_type const& v ) {
        // add the item to the write list (to be applied at commit time)
        auto item = Sto::item(this, -1);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<value_type>();
                std::list<value_type> write_list;
                if (!is_empty(item)) {
                    write_list.push_back(val);
                    item.clear_flags(empty_bit);
                }
                write_list.push_back(v);
                item.clear_write();
                item.add_write(write_list);
                item.add_flags(list_bit);
            }
            else {
                auto& write_list = item.template write_value<std::list<value_type>>();
                write_list.push_back(v);
            }
        }
        else item.add_write(v);
        return true;
    }

    // Marks an item as poisoned in the queue
    bool pop( value_type& val ) {
        // try marking an item in the queue as deleted
        auto pRec = fc_kernel_.acquire_record();
        val_wrapper<value_type> vw = {val, 0};
        pRec->pValPop = &vw;
        fc_kernel_.combine( op_pop, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        if (!pRec->is_empty) {
            val = std::move(pRec->pValPop->val);
            return true;
        } else return false;
    }

    // Clears the queue
    void clear() {
        auto pRec = fc_kernel_.acquire_record();
        fc_kernel_.combine( op_clear, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
    }

    // Returns the number of elements in the queue.
    // Non-transactional
    size_t size() const {
        return q_.size();
    }

    // Checks if the queue is empty
    bool empty() {
        auto pRec = fc_kernel_.acquire_record();
        fc_kernel_.combine( op_empty, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        return pRec->is_empty;
    }

public: // flat combining cooperation, not for direct use!
    /*
        The function is called by flat_combining::kernel "flat combining kernel"
        object if the current thread becomes a combiner. Invocation of the function means that
        the queue should perform an action recorded in pRec.
    */
    void fc_apply( fc_record * pRec ) {
        assert( pRec );

        // this function is called under FC mutex, so switch TSan off
        CDS_TSAN_ANNOTATE_IGNORE_RW_BEGIN;

        // do the operation requested
        switch ( pRec->op() ) {
        case op_push:
            assert( pRec->pValPush );
            q_.push_back( *(pRec->pValPush) );
            break;
        case op_pop: {
            assert( pRec->pValPop );
            pRec->is_empty = q_.empty();
            if ( !pRec->is_empty ) {
                *(pRec->pValPop) = std::move( q_.front());
                q_.pop_front();
            }
            break;
        }
        case op_clear:
            while ( !q_.empty() )
                q_.pop_back();
            break;
        case op_empty:
            pRec->is_empty = q_.empty();
            break;
        default:
            assert(false);
            break;
        }
        CDS_TSAN_ANNOTATE_IGNORE_RW_END;
    }

    void print_statistics() { 
        fprintf(stderr, "\
                Num Operations\t %lu\n\
                Num Combines\t %lu\n\
                Compacting Factor\t %f\n\
                Num Compacting PubList\t %lu\n\
                Num Deactivate Rec\t %lu\n\
                Num Activate Rec\t %lu\n\
                Num Create Rec\t %lu\n\
                Num Delete Rec\t %lu\n\
                Num Passive Calls\t %lu\n\
                Num Passive Iters\t %lu\n\
                Num Passive Wait Wakeups\t %lu\n\
                Num Passive->Combiner\t %lu\n",
        fc_kernel_.statistics().nOperationCount.get(), fc_kernel_.statistics().nCombiningCount.get(), fc_kernel_.statistics().combining_factor(),
        fc_kernel_.statistics().nCompactPublicationList.get(), fc_kernel_.statistics().nDeactivatePubRecord.get(), 
        fc_kernel_.statistics().nActivatePubRecord.get(), fc_kernel_.statistics().nPubRecordCreated.get(), fc_kernel_.statistics().nPubRecordDeleted.get(),
        fc_kernel_.statistics().nPassiveWaitCall.get(), fc_kernel_.statistics().nPassiveWaitIteration.get(), fc_kernel_.statistics().nPassiveWaitWakeup.get(),
        fc_kernel_.statistics().nPassiveToCombiner.get());
    }

private:
    // Fxns for the combiner thread
    bool has_delete(const val_wrapper<value_type>& val) {
        return val.flags & delete_bit;
    }
    
    bool is_popped(const val_wrapper<value_type>& val) {
        return val.flags & popped_bit;
    }

    // STO-specific functions for txn commit protocol
    bool is_rw(const TransItem& item) {
        return item.flags() & read_writes;
    }
 
    bool is_list(const TransItem& item) {
        return item.flags() & list_bit;
    }
 
    bool is_empty(const TransItem& item) {
        return item.flags() & empty_bit;
    }

    bool lock(TransItem& item, Transaction& txn) override {
        if ((item.key<int>() == -1) && !queueversion_.is_locked_here())  {
#if LOCKQV
            queueversion_.lock();
            //return txn.try_lock(item, queueversion_); 
#else
        (void)txn;
#endif
        } 
        return true;
    }

    bool check(TransItem& item, Transaction& t) override {
        (void) t;
        // check if we read off the write_list. We should only abort if both: 
        //      1) we saw the queue was empty during a pop/front and read off our own write list 
        //      2) someone else has pushed onto the queue before we got here
        if (item.key<int>() == -1)
            return item.check_version(queueversion_);
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
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<value_type>>();
                while (!write_list.empty()) {
                    auto val = write_list.front();
                    write_list.pop_front();
                    val_wrapper<value_type> vw = {val, 0};
                    auto pRec = fc_kernel_.acquire_record();
                    pRec->pValPush = &vw; 
                    fc_kernel_.combine( op_push, pRec, *this );
                    assert( pRec->is_done() );
                    fc_kernel_.release_record( pRec );
                }
            }
            else if (!is_empty(item)) {
                auto& val = item.template write_value<value_type>();
                val_wrapper<value_type> vw = {val, 0};
                auto pRec = fc_kernel_.acquire_record();
                pRec->pValPush = &vw; 
                fc_kernel_.combine( op_push, pRec, *this );
                assert( pRec->is_done() );
                fc_kernel_.release_record( pRec );
            }
        }
        // set queueversion appropriately
#if !LOCKQV
        if (!queueversion_.is_locked_here()) {
            queueversion_.lock();
        }
#endif
        queueversion_.set_version(txn.commit_tid());
    }

    void unlock(TransItem& item) override {
        (void)item;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        (void)item;
        (void)committed;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }
};
#endif // #ifndef FCQUEUE_H
