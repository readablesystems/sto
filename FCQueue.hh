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

#include <cds/algo/elimination_opt.h>
#include <queue>
#include "Flat_Combining.hh"
#include "Transaction.hh"

/// FCQueue related definitions
namespace fcqueue {

    /// FCQueue internal statistics
    template <typename Counter = cds::atomicity::event_counter >
    struct stat: public cds::algo::flat_combining::stat<Counter>
    {
        typedef cds::algo::flat_combining::stat<Counter>    flat_combining_stat; ///< Flat-combining statistics
        typedef typename flat_combining_stat::counter_type  counter_type;        ///< Counter type

        counter_type    m_nEnqueue     ;   ///< Count of enqueue operations
        counter_type    m_nEnqMove     ;   ///< Count of enqueue operations with move semantics
        counter_type    m_nDequeue     ;   ///< Count of success dequeue operations
        counter_type    m_nFailedDeq   ;   ///< Count of failed dequeue operations (pop from empty queue)
        counter_type    m_nCollided    ;   ///< How many pairs of enqueue/dequeue were collided, if elimination is enabled

        //@cond
        void    onEnqueue()               { ++m_nEnqueue; }
        void    onEnqMove()               { ++m_nEnqMove; }
        void    onDequeue( bool bFailed ) { if ( bFailed ) ++m_nFailedDeq; else ++m_nDequeue;  }
        void    onCollide()               { ++m_nCollided; }
        //@endcond
    };

    /// FCQueue dummy statistics, no overhead
    struct empty_stat: public cds::algo::flat_combining::empty_stat
    {
        //@cond
        void    onEnqueue()     {}
        void    onEnqMove()     {}
        void    onDequeue(bool) {}
        void    onCollide()     {}
        //@endcond
    };

    /// FCQueue type traits
    struct traits: public cds::algo::flat_combining::traits
    {
        typedef empty_stat      stat;   ///< Internal statistics
        static CDS_CONSTEXPR const bool enable_elimination = false; ///< Enable \ref cds_elimination_description "elimination"
    };

    /// Metafunction converting option list to traits
    /**
        \p Options are:
        - \p opt::lock_type - mutex type, default is \p cds::sync::spin
        - \p opt::back_off - back-off strategy, defalt is \p cds::backoff::delay_of<2>
        - \p opt::allocator - allocator type, default is \ref CDS_DEFAULT_ALLOCATOR
        - \p opt::stat - internal statistics, possible type: \p fcqueue::stat, \p fcqueue::empty_stat (the default)
        - \p opt::memory_model - C++ memory ordering model.
            List of all available memory ordering see \p opt::memory_model.
            Default is \p cds::opt::v:relaxed_ordering
        - \p opt::enable_elimination - enable/disable operation \ref cds_elimination_description "elimination"
            By default, the elimination is disabled. For queue, the elimination is possible if the queue
            is empty.
    */
    template <typename... Options>
    struct make_traits {
#   ifdef CDS_DOXYGEN_INVOKED
        typedef implementation_defined type ;   ///< Metafunction result
#   else
        typedef typename cds::opt::make_options<
            typename cds::opt::find_type_traits< traits, Options... >::type
            ,Options...
        >::type   type;
#   endif
    };

} // namespace fcqueue

template <typename T,
    class Queue = std::queue<T>,
    typename Traits = fcqueue::traits
>
class FCQueue : public Shared,
    public flat_combining::container
{
public:
    typedef T           value_type;     ///< Value type
    typedef Queue       queue_type;     ///< Sequential queue class
    typedef Traits      traits;         ///< Queue type traits

    typedef typename traits::stat  stat;   ///< Internal statistics type
    static CDS_CONSTEXPR const bool c_bEliminationEnabled = traits::enable_elimination;

    // STO
    typedef typename W<T>::version_type version_type;

    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type read_writes = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type list_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type empty_bit = TransItem::user0_bit<<3;

protected:
    /// Queue operation IDs
    enum fc_operation {
        op_enq = flat_combining::req_Operation, ///< Enqueue
        op_enq_move,    ///< Enqueue (move semantics)
        op_deq,         ///< Dequeue
        op_clear,       ///< Clear
        op_empty        ///< Empty
    };

    /// Flat combining publication list record
    struct fc_record: public flat_combining::publication_record
    {
        union {
            value_type const *  pValEnq;  ///< Value to enqueue
            value_type *        pValDeq;  ///< Dequeue destination
        };
        bool            bEmpty; ///< \p true if the queue is empty
    };

    /// Flat combining kernel
    typedef flat_combining::kernel< fc_record, traits > fc_kernel;

protected:
    fc_kernel   m_FlatCombining;
    queue_type  m_Queue;

public:
    /// Initializes empty queue object
    FCQueue() : queueversion_(0) {}

    /// Initializes empty queue object and gives flat combining parameters
    FCQueue(
        unsigned int nCompactFactor     ///< Flat combining: publication list compacting factor
        ,unsigned int nCombinePassCount ///< Flat combining: number of combining passes for combiner thread
        )
        : m_FlatCombining( nCompactFactor, nCombinePassCount ), queueversion_(0)
    {}

    /// Inserts a new element at the end of the queue
    /**
        The content of the new element initialized to a copy of \p val.

        The function always returns \p true
    */
    bool push( value_type const& val )
    {
        // add the item to the write list (to be applied at commit time)
        auto item = Sto::item(this, -1);
        if (item.has_write()) {
            if (!is_list(item)) {
                auto& val = item.template write_value<T>();
                std::list<T> write_list;
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
                auto& write_list = item.template write_value<std::list<T>>();
                write_list.push_back(v);
            }
        }
        else item.add_write(v);
    }

    /// Inserts a new element at the end of the queue (move semantics)
    /**
        \p val is moved to inserted element
    */
    bool push( value_type&& val )
    {
        fc_record * pRec = m_FlatCombining.acquire_record();
        pRec->pValEnq = &val;

        if ( c_bEliminationEnabled )
            m_FlatCombining.batch_combine( op_enq_move, pRec, *this );
        else
            m_FlatCombining.combine( op_enq_move, pRec, *this );

        assert( pRec->is_done() );
        m_FlatCombining.release_record( pRec );

        m_FlatCombining.internal_statistics().onEnqMove();
        return true;
    }

    /// Removes the next element from the queue
    /**
        \p val takes a copy of the element
    */
    bool pop( value_type& val )
    {
        fc_record * pRec = m_FlatCombining.acquire_record();
        pRec->pValDeq = &val;

        if ( c_bEliminationEnabled )
            m_FlatCombining.batch_combine( op_deq, pRec, *this );
        else
            m_FlatCombining.combine( op_deq, pRec, *this );

        assert( pRec->is_done() );
        m_FlatCombining.release_record( pRec );

        m_FlatCombining.internal_statistics().onDequeue( pRec->bEmpty );
        return !pRec->bEmpty;
    }

    /// Clears the queue
    void clear()
    {
        fc_record * pRec = m_FlatCombining.acquire_record();

        if ( c_bEliminationEnabled )
            m_FlatCombining.batch_combine( op_clear, pRec, *this );
        else
            m_FlatCombining.combine( op_clear, pRec, *this );

        assert( pRec->is_done() );
        m_FlatCombining.release_record( pRec );
    }

    /// Returns the number of elements in the queue.
    /**
        Note that <tt>size() == 0</tt> is not mean that the queue is empty because
        combining record can be in process.
        To check emptiness use \ref empty function.
    */
    size_t size() const
    {
        return m_Queue.size();
    }

    /// Checks if the queue is empty
    /**
        If the combining is in process the function waits while combining done.
    */
    bool empty()
    {
        fc_record * pRec = m_FlatCombining.acquire_record();

        if ( c_bEliminationEnabled )
            m_FlatCombining.batch_combine( op_empty, pRec, *this );
        else
            m_FlatCombining.combine( op_empty, pRec, *this );

        assert( pRec->is_done() );
        m_FlatCombining.release_record( pRec );
        return pRec->bEmpty;
    }

    /// Internal statistics
    stat const& statistics() const
    {
        return m_FlatCombining.statistics();
    }

public: // flat combining cooperation, not for direct use!
    /// Flat combining supporting function. Do not call it directly!
    /**
        The function is called by \ref flat_combining::kernel "flat combining kernel"
        object if the current thread becomes a combiner. Invocation of the function means that
        the queue should perform an action recorded in \p pRec.
    */
    void fc_apply( fc_record * pRec )
    {
        assert( pRec );

        // this function is called under FC mutex, so switch TSan off
        CDS_TSAN_ANNOTATE_IGNORE_RW_BEGIN;

        switch ( pRec->op() ) {
        case op_enq:
            assert( pRec->pValEnq );
            m_Queue.push( *(pRec->pValEnq ) );
            break;
        case op_enq_move:
            assert( pRec->pValEnq );
            m_Queue.push( std::move( *(pRec->pValEnq )) );
            break;
        case op_deq:
            assert( pRec->pValDeq );
            pRec->bEmpty = m_Queue.empty();
            if ( !pRec->bEmpty ) {
                *(pRec->pValDeq) = std::move( m_Queue.front());
                m_Queue.pop();
            }
            break;
        case op_clear:
            while ( !m_Queue.empty() )
                m_Queue.pop();
            break;
        case op_empty:
            pRec->bEmpty = m_Queue.empty();
            break;
        default:
            assert(false);
            break;
        }
        CDS_TSAN_ANNOTATE_IGNORE_RW_END;
    }

private:
    bool has_delete(const TransItem& item) {
        return item.flags() & delete_bit;
    }
    
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

        // try combining --> lock?
        fc_record * pRec = m_FlatCombining.acquire_record();
        pRec->pValEnq = &val;

        if ( c_bEliminationEnabled )
            m_FlatCombining.batch_combine( op_enq, pRec, *this );
        else
            m_FlatCombining.combine( op_enq, pRec, *this );

        assert( pRec->is_done() );
        m_FlatCombining.release_record( pRec );
        m_FlatCombining.internal_statistics().onEnqueue();
        return true;



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
        // install pops
        if (has_delete(item)) {
            // only increment head if item popped from actual q
            if (!is_rw(item)) {
                head_ = (head_+1) % BUF_SIZE;
            }
        }
        // install pushes
        else if (item.key<int>() == -1) {

            assert(queueversion_.is_locked_here());
            auto head_index = head_;
            // write all the elements
            if (is_list(item)) {
                auto& write_list = item.template write_value<std::list<T>>();
                while (!write_list.empty()) {
                    // assert queue is not out of space            
                    assert(tail_ != (head_index-1) % BUF_SIZE);
                    queueSlots[tail_] = write_list.front();
                    write_list.pop_front();
                    tail_ = (tail_+1) % BUF_SIZE;
                }
            }
            else if (!is_empty(item)) {
                auto& val = item.template write_value<T>();
                queueSlots[tail_] = val;
                tail_ = (tail_+1) % BUF_SIZE;
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
        (void)committed;
        if (queueversion_.is_locked_here()) {
            queueversion_.unlock();
        }
    }

    version_type queueversion_;
};

#endif // #ifndef FCQUEUE_H
