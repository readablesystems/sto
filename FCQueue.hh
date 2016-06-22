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

// Tells the combiner thread the flags associated with each item in the 
template <typename T>
struct val_wrapper {
    T val;
    uint8_t flags; 
    int threadid;
};

template <typename T, 
         class Queue = std::deque<val_wrapper<T>>,
         template <typename> class W = TOpaqueWrapped>
class FCQueue : public Shared,
    public flat_combining::container
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
        op_mark_deleted,  // Pop (mark as poisoned)
        op_install_pops, // Pop (marking as not-a-value)
        op_undo_mark_deleted, // Cleanup Pops (mark poisoned values with specified threadid as clean)
        op_clear_popped, // Clear Pops at front which were successfully popped
        op_clear,       // Clear
        op_empty        // Empty
    };


    // Flat combining publication list record
    struct fc_record: public flat_combining::publication_record
    {
        union {
            val_wrapper<value_type> const *  pValPush; // Value to enqueue
            val_wrapper<value_type> *        pValPop;  // Pop destination
            val_wrapper<value_type> *        pValEdit;  // Value to edit 
        };
        bool            is_empty; // true if the queue is empty
    };

    flat_combining::kernel< fc_record> fc_kernel_;
    queue_type  q_;
    version_type queueversion_;
    int last_deleted_index_;   // index of the item in q_ last marked deleted. 
                                    // -1 indicates an empty q_
 
public:
    /// Initializes empty queue object
    FCQueue() : queueversion_(0), last_deleted_index_(-1) {}

    /// Initializes empty queue object and gives flat combining parameters
    FCQueue(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : fc_kernel_( nCompactFactor, nCombinePassCount ), queueversion_(0), last_deleted_index_(-1)
    {}

    // Adds an item to the write list of the txn, to be installed at commit time
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
        fc_record * pRec = fc_kernel_.acquire_record();
        val_wrapper<value_type> vw = {val, 0, TThread::id()};
        pRec->pValPop = &vw;
        fc_kernel_.combine( op_mark_deleted, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );

        if (!pRec->is_empty) {
            val = std::move(pRec->pValPop->val);
            // we marked an item as deleted in the queue, 
            // so we have to install at commit time
            Sto::item(this,0).add_write();
            return true;
        } else { // queue is empty
            auto pushitem = Sto::item(this,-1);
            // add a read of the queueversion
            if (!pushitem.has_read()) pushitem.observe(queueversion_);
            // try read-my-writes
            if (pushitem.has_write()) {
                if (is_list(pushitem)) {
                    auto& write_list = pushitem.template write_value<std::list<value_type>>();
                    // if there is an element to be pushed on the queue, return addr of queue element
                    if (!write_list.empty()) {
                        write_list.pop_front();
                        return true;
                    }
                    else return false;
                }
                // not a list, has exactly one element
                else if (!is_empty(pushitem)) {
                    pushitem.add_flags(empty_bit);
                    return true;
                }
                else return false;
            }
            // fail if trying to read from an empty queue
            else return false;  
        }
    }

    // Clears the queue
    void clear() {
        fc_record * pRec = fc_kernel_.acquire_record();
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
        fc_record * pRec = fc_kernel_.acquire_record();
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

        int threadid;
        // do the operation requested
        switch ( pRec->op() ) {
        case op_push:
            assert( pRec->pValPush );
            q_.push_back( *(pRec->pValPush) );
            break;

        case op_mark_deleted:
            assert( pRec->pValPop );
            if ( !q_.empty() ) {
                for (auto it = q_.begin(); it != q_.end(); ++it) {
                    if (!has_delete(*it) && !is_popped(*it)) {
                        it->threadid = pRec->pValPop->threadid;
                        it->flags = delete_bit;
                        *(pRec->pValPop) = *it; 
                        pRec->is_empty = false;
                        auto index = it-q_.begin();
                        last_deleted_index_ = (last_deleted_index_ > index) ? last_deleted_index_ : index;
                        return;
                    }
                }
            }
            // didn't find any non-deleted items, queue is empty
            pRec->is_empty = true;
            break;

        case op_install_pops: {
            threadid = pRec->pValEdit->threadid;
            bool found = false;
            // we should only install if the txn actually did mark an item
            // as deleted in the queue. this implies that the queue cannot be 
            // nonempty
            assert(last_deleted_index_ != -1);
            for (auto it = q_.begin(); it != q_.begin() + last_deleted_index_ + 1; ++it) {
                if (has_delete(*it) && (threadid == it->threadid)) {
                    found = true;
                    it->flags = popped_bit;
                }
            }
            assert(found);
            break;
        }
        
        case op_undo_mark_deleted: {
            threadid = pRec->pValEdit->threadid;
            auto begin_it = q_.begin();
            auto new_di = 0;
          
            if (last_deleted_index_ != -1) {
                for (auto it = begin_it; it != begin_it + last_deleted_index_ + 1; ++it) {
                    if (has_delete(*it)) {
                        if (threadid == it->threadid) {
                            it->flags = 0;
                        } else {
                            new_di = it-begin_it;
                        }
                    }
                }
            }
            // set the last_deleted_index_ to the next greatest deleted element's index in the queue
            // (which could be unchanged)
            last_deleted_index_ = new_di;
            break;
        } 

        case op_clear_popped: 
            // remove all popped values from txns that have committed their pops
            // XXX should this be done elsewhere?
            while( !q_.empty() && is_popped(q_.front()) ) {
                if (last_deleted_index_ != -1) --last_deleted_index_;
                q_.pop_front();
            }
            break;

        // the following are non-transactional
        case op_clear:
            while ( !q_.empty() )
                q_.pop_back();
            last_deleted_index_ = -1;
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
            return txn.try_lock(item, queueversion_);
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
            fc_record * pRec = fc_kernel_.acquire_record();
            auto val = T();
            val_wrapper<value_type> vw = {val, 0, TThread::id()};
            pRec->pValEdit = &vw;
            fc_kernel_.combine( op_install_pops, pRec, *this );
            assert( pRec->is_done() );
            fc_kernel_.release_record( pRec );
        }
        // install pushes
        if (item.key<int>() == -1) {
            assert(queueversion_.is_locked_here());
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<value_type>>();
                while (!write_list.empty()) {
                    // TODO not efficient -- should use batch processing here?
                    auto val = write_list.front();
                    write_list.pop_front();
                    fc_record * pRec = fc_kernel_.acquire_record();
                    val_wrapper<value_type> vw = {val, 0, 0};
                    pRec->pValPush = &vw; 
                    fc_kernel_.combine( op_push, pRec, *this );
                    assert( pRec->is_done() );
                    fc_kernel_.release_record( pRec );
                }
            }
            else if (!is_empty(item)) {
                auto& val = item.template write_value<value_type>();
                fc_record * pRec = fc_kernel_.acquire_record();
                val_wrapper<value_type> vw = {val, 0, 0};
                pRec->pValPush = &vw; 
                fc_kernel_.combine( op_push, pRec, *this );
                assert( pRec->is_done() );
                fc_kernel_.release_record( pRec );
            }
            // set queueversion appropriately
            queueversion_.set_version(txn.commit_tid());
        }
    }
    
    void unlock(TransItem& item) override {
        (void)item;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }

    void cleanup(TransItem& item, bool committed) override {
        (void)item;
        if (!committed) {
            // Mark all deleted items in the queue as not deleted 
            fc_record * pRec = fc_kernel_.acquire_record();
            auto val = T();
            val_wrapper<value_type> vw = {val, 0, TThread::id()};
            pRec->pValEdit = &vw;
            fc_kernel_.combine( op_undo_mark_deleted, pRec, *this );
            assert( pRec->is_done() );
            fc_kernel_.release_record( pRec );
        } else {
            // clear all the popped values at the front of the queue 
            fc_record * pRec = fc_kernel_.acquire_record();
            auto val = T();
            val_wrapper<value_type> vw = {val, 0, 0};
            pRec->pValEdit = &vw;
            fc_kernel_.combine( op_clear_popped, pRec, *this );
            assert( pRec->is_done() );
            fc_kernel_.release_record( pRec );
        }
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }
};

#endif // #ifndef FCQUEUE_H
