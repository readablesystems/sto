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

#define FC 0
#define ITER 0

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

template <typename T, class Queue = std::deque<val_wrapper<T>>>
class FCQueue : public flat_combining::container
{
public:
    typedef T           value_type;     ///< Value type
    typedef Queue       queue_type;     ///< Sequential queue class

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
    int last_deleted_index_;   // index of the item in q_ last marked deleted. 
                                    // -1 indicates an empty q_
 
public:
    /// Initializes empty queue object
    FCQueue() : last_deleted_index_(-1) {}

    /// Initializes empty queue object and gives flat combining parameters
    FCQueue(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : fc_kernel_( nCompactFactor, nCombinePassCount ), last_deleted_index_(-1)
    {}

    // Adds an item to the write list of the txn, to be installed at commit time
    bool push( value_type const& v ) {
#if !FC
        q_.push_back({v,0,0});
        return true;
#else
        // XXX USING FC
        fc_record * pRec = fc_kernel_.acquire_record();
        val_wrapper<value_type> vw = {v, 0, 0};
        pRec->pValPush = &vw; 
        fc_kernel_.combine( op_push, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        return true;
#endif
    }

    // Marks an item as poisoned in the queue
    bool pop( value_type& val ) {
        // try marking an item in the queue as deleted
#if !FC
        if (!q_.empty()) {
            vw = std::move(q_.front())
            val = vw.val;
            q_.pop_front();
            return true;
        }
        return false;
#else
        fc_record * pRec = fc_kernel_.acquire_record();
        val_wrapper<value_type> vw = {val, 0, TThread::id()};
        pRec->pValPop = &vw;
        fc_kernel_.combine( op_mark_deleted, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );

        if (!pRec->is_empty) {
            // install the deleted item
            pRec = fc_kernel_.acquire_record();
            auto val = T();
            val_wrapper<value_type> vw = {val, 0, TThread::id()};
            pRec->pValEdit = &vw;
            fc_kernel_.combine( op_install_pops, pRec, *this );
            assert( pRec->is_done() );
            fc_kernel_.release_record( pRec );

            // clear all the popped values at the front of the queue 
            pRec = fc_kernel_.acquire_record();
            val = T();
            vw = {val, 0, 0};
            pRec->pValEdit = &vw;
            fc_kernel_.combine( op_clear_popped, pRec, *this );
            assert( pRec->is_done() );
            fc_kernel_.release_record( pRec );

            return true;
        } 
        // queue was empty
        return false;
#endif
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
#if !ITER
            assert( pRec->pValPop );
            pRec->is_empty = q_.empty();
            if ( !pRec->is_empty) {
                *(pRec->pValPop) = std::move(q_.front());
                q_.pop_front();
            }
            break;
#else
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
                        break;
                    }
                }
            }
            // didn't find any non-deleted items, queue is empty
            pRec->is_empty = true;
            break;
#endif

        case op_install_pops: {
#if !ITER
            break;
#else
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
#endif
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
};

#endif // #ifndef FCQUEUE_H
