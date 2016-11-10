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

#ifndef FCQUEUENT1_H 
#define FCQUEUENT1_H

#include <deque>
#include <list>
#include "FlatCombining.hh"
#include "Transaction.hh"
#include "TWrapped.hh"

template <typename T, 
         template <typename> class W = TOpaqueWrapped,
         class Queue = std::deque<T>>
class FCQueueNT1 : public flat_combining::container
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
        op_pop,         // Pop
        op_clear,       // Clear
        op_empty        // Empty
    };


    // Flat combining publication list record
    struct fc_record: public flat_combining::publication_record
    {
        union {
            value_type const *  pValPush; // Value to enqueue
        };
        bool            is_empty; // true if the queue is empty
    };

    flat_combining::kernel<fc_record> fc_kernel_;
    queue_type  q_;
    version_type queueversion_;

public:
    /// Initializes empty queue object
    FCQueueNT1() : queueversion_(0) {}

    /// Initializes empty queue object and gives flat combining parameters
    FCQueueNT1(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : fc_kernel_( nCompactFactor, nCombinePassCount ), queueversion_(0) {}

    bool push( value_type const& v ) {
        auto pRec = fc_kernel_.acquire_record();
        pRec->pValPush = &v; 
        fc_kernel_.combine( op_push, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        return true;
    }

    bool pop() {
        auto pRec = fc_kernel_.acquire_record();
        fc_kernel_.combine( op_pop, pRec, *this );
        assert( pRec->is_done() );
        fc_kernel_.release_record( pRec );
        return (!pRec->is_empty);
    }

    // Clears the queue
    // Non-transactional
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
            pRec->is_empty = q_.empty();
            if ( !pRec->is_empty) {
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
                Num Republish Rec\t %lu\n\
                Num Create Rec\t %lu\n\
                Num Delete Rec\t %lu\n\
                Num Passive Calls\t %lu\n\
                Num Passive Iters\t %lu\n\
                Num Passive Wait Wakeups\t %lu\n\
                Num Passive->Combiner\t %lu\n\
                Num Useful Passes\t %lu\n",
        fc_kernel_.statistics().nOperationCount.get(), fc_kernel_.statistics().nCombiningCount.get(), fc_kernel_.statistics().combining_factor(),
        fc_kernel_.statistics().nCompactPublicationList.get(), fc_kernel_.statistics().nDeactivatePubRecord.get(), fc_kernel_.statistics().nActivatePubRecord.get(), 
        fc_kernel_.statistics().nRepublishPubRecord.get(), fc_kernel_.statistics().nPubRecordCreated.get(), fc_kernel_.statistics().nPubRecordDeleted.get(),
        fc_kernel_.statistics().nPassiveWaitCall.get(), fc_kernel_.statistics().nPassiveWaitIteration.get(), fc_kernel_.statistics().nPassiveWaitWakeup.get(),
        fc_kernel_.statistics().nPassiveToCombiner.get(), fc_kernel_.statistics().nUsefulPasses.get());
    }
};

#endif // #ifndef FCQUEUENT1_H
