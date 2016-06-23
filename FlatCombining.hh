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

#ifndef FLAT_COMBINING_H
#define FLAT_COMBINING_H

#include <mutex>
#include <cds/algo/atomic.h>
#include <cds/details/allocator.h>
#include <cds/algo/backoff_strategy.h>
#include <cds/sync/spinlock.h>
#include <cds/opt/options.h>
#include <cds/algo/int_algo.h>
#include <boost/thread/tss.hpp>     // thread_specific_ptr

/// Flat combining
/**
    Flat combining (FC) technique is invented by Hendler, Incze, Shavit and Tzafrir in their paper
    [2010] <i>"Flat Combining and the Synchronization-Parallelism Tradeoff"</i>.
    The technique converts a sequential data structure to its concurrent implementation.
    A few structures are added to the sequential implementation: a <i>global lock</i>,
    a <i>count</i> of the number of combining passes, and a pointer to the <i>head</i>
    of a <i>publication list</i>. The publication list is a list of thread-local records
    of a size proportional to the number of threads that are concurrently accessing the shared object.

    Each thread \p t accessing the structure to perform an invocation of some method \p m
    on the shared object executes the following sequence of steps:
    <ol>
    <li>Write the invocation opcode and parameters (if any) of the method \p m to be applied
    sequentially to the shared object in the <i>request</i> field of your thread local publication
    record (there is no need to use a load-store memory barrier). The <i>request</i> field will later
    be used to receive the response. If your thread local publication record is marked as active
    continue to step 2, otherwise continue to step 5.</li>
    <li>Check if the global lock is taken. If so (another thread is an active combiner), spin on the <i>request</i>
    field waiting for a response to the invocation (one can add a yield at this point to allow other threads
    on the same core to run). Once in a while while spinning check if the lock is still taken and that your
    record is active. If your record is inactive proceed to step 5. Once the response is available,
    reset the request field to null and return the response.</li>
    <li>If the lock is not taken, attempt to acquire it and become a combiner. If you fail,
    return to spinning in step 2.</li>
    <li>Otherwise, you hold the lock and are a combiner.
    <ul>
        <li>Increment the combining pass count by one.</li>
        <li>Execute a \p fc_apply() by traversing the publication list from the head,
        combining all nonnull method call invocations, setting the <i>age</i> of each of these records
        to the current <i>count</i>, applying the combined method calls to the structure D, and returning
        responses to all the invocations. This traversal is guaranteed to be wait-free.</li>
        <li>If the <i>count</i> is such that a cleanup needs to be performed, traverse the publication
        list from the <i>head</i>. Starting from the second item (we always leave the item pointed to
        by the head in the list), remove from the publication list all records whose <i>age</i> is
        much smaller than the current <i>count</i>. This is done by removing the node and marking it
        as inactive.</li>
        <li>Release the lock.</li>
    </ul>
    <li>If you have no thread local publication record allocate one, marked as active. If you already
    have one marked as inactive, mark it as active. Execute a store-load memory barrier. Proceed to insert
    the record into the list with a successful CAS to the <i>head</i>. Then proceed to step 1.</li>
    </ol>

    As the test results show, the flat combining technique is suitable for non-intrusive containers
    like stack, queue, deque. For intrusive concurrent containers the flat combining demonstrates
    less impressive results.
*/

namespace flat_combining {

    /// Special values of publication_record::nRequest
    enum request_value
    {
        req_EmptyRecord,    ///< Publication record is empty
        req_Response,       ///< Operation is done
        req_Operation       ///< First operation id for derived classes
    };

    /// publication_record state
    enum record_state {
        inactive,       ///< Record is inactive
        active,         ///< Record is active
        removed         ///< Record should be removed
    };

    /// Record of publication list
    /**
        Each data structure based on flat combining contains a class derived from \p %publication_record
    */
    struct publication_record {
        atomics::atomic<unsigned int>    nRequest;   ///< Request field (depends on data structure)
        atomics::atomic<unsigned int>    nState;     ///< Record state: inactive, active, removed
        atomics::atomic<publication_record *> pNext; ///< Next record in publication list
        void *                              pOwner;    ///< [internal data] Pointer to \ref kernel object that manages the publication list

        /// Initializes publication record
        publication_record()
            : nRequest( req_EmptyRecord )
            , nState( inactive )
            , pNext( nullptr )
            , pOwner( nullptr )
        {}

        /// Returns the value of \p nRequest field
        unsigned int op() const
        {
            return nRequest.load( atomics::memory_order_relaxed );
        }

        /// Checks if the operation is done
        bool is_done() const
        {
            return nRequest.load( atomics::memory_order_relaxed ) == req_Response;
        }
    };

    /// The kernel of flat combining
    /**
        Template parameters:
        - \p PublicationRecord - a type derived from \ref publication_record

        The kernel object should be a member of a container class. The container cooperates with flat combining
        kernel object. There are two ways to interact with the kernel:
        - One-by-one processing the active records of the publication list. This mode provides by \p combine() function:
          the container acquires its publication record by \p acquire_record(), fills its fields and calls
          \p combine() function of its kernel object. If the current thread becomes a combiner, the kernel
          calls \p fc_apply() function of the container for each active non-empty record. Then, the container
          should release its publication record by \p release_record(). Only one pass through the publication
          list is possible.
    */
    template <
        typename PublicationRecord
    >
    class kernel
    {
    public:
        typedef PublicationRecord   publication_record_type;   ///< publication record type
        typedef typename cds::sync::spin global_lock_type;   ///< Global lock type
        typedef typename cds::backoff::delay_of<2> back_off;           ///< back-off strategy type
        typedef typename CDS_DEFAULT_ALLOCATOR allocator;          ///< Allocator type (used for allocating publication_record_type data)
        typedef typename cds::opt::v::relaxed_ordering memory_model;    ///< C++ memory model

    protected:
        //@cond
        typedef cds::details::Allocator< publication_record_type, allocator >   cxx11_allocator; ///< internal helper cds::details::Allocator
        typedef std::lock_guard<global_lock_type> lock_guard;
        //@endcond

    protected:
        publication_record_type *   m_pHead;    ///< Head of publication list
        boost::thread_specific_ptr< publication_record_type >   m_pThreadRec;   ///< Thread-local publication record
        mutable global_lock_type    m_Mutex;    ///< Global mutex
        unsigned int const          m_nCombinePassCount; ///< Number of combining passes

    public:
        /// Initializes the object
        kernel() : m_pHead( nullptr )
            , m_pThreadRec( tls_cleanup )
            , m_nCombinePassCount( 8 )
        {
            init();
        }

        /// Initializes the object
        kernel(unsigned int nCombinePassCount) ///< Number of combining passes for combiner thread
            : m_pHead( nullptr )
            , m_pThreadRec( tls_cleanup )
            , m_nCombinePassCount( nCombinePassCount )
        {
            init();
        }

        /// Destroys the objects and mark all publication records as inactive
        ~kernel()
        {
            // mark all publication record as detached
            for ( publication_record * p = m_pHead; p; ) {
                p->pOwner = nullptr;

                publication_record * pRec = p;
                p = p->pNext.load( memory_model::memory_order_relaxed );
                if ( pRec->nState.load( memory_model::memory_order_acquire ) == removed )
                    free_publication_record( static_cast<publication_record_type *>( pRec ));
            }
        }

        /// Gets publication list record for the current thread
        /**
            If there is no publication record for the current thread
            the function allocates it.
        */
        publication_record_type * acquire_record()
        {
            publication_record_type * pRec = m_pThreadRec.get();
            if ( !pRec ) {
                // Allocate new publication record
                pRec = cxx11_allocator().New();
                pRec->pOwner = reinterpret_cast<void *>( this );
                m_pThreadRec.reset( pRec );
            }

            if ( pRec->nState.load( memory_model::memory_order_acquire ) != active )
                publish( pRec );

            assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_EmptyRecord );

            return pRec;
        }

        /// Marks publication record for the current thread as empty
        void release_record( publication_record_type * pRec )
        {
            assert( pRec->is_done() );
            pRec->nRequest.store( req_EmptyRecord, memory_model::memory_order_release );
        }

        /*
            pRec is the publication record acquiring by acquire_record earlier.
            owner is a container that is owner of flat combining kernel object.
            As a result the current thread can become a combiner or can wait for
            another combiner performs pRec operation.

            If the thread becomes a combiner, the kernel calls owner.fc_apply
            for each active non-empty publication record.
        */
        template <class Container>
        void combine( unsigned int nOpId, publication_record_type * pRec, Container& owner )
        {
            assert( nOpId >= req_Operation );
            assert( pRec );
            pRec->nRequest.store( nOpId, memory_model::memory_order_release );

            try_combining( owner, pRec );
        }

        /// Marks \p rec as executed
        void operation_done( publication_record& rec )
        {
            rec.nRequest.store( req_Response, memory_model::memory_order_release );
        }

    private:
        static void tls_cleanup( publication_record_type * pRec )
        {
            // Thread done
            // pRec that is TLS data should be excluded from publication list
            if ( pRec ) {
                if ( pRec->pOwner ) {
                    // kernel is alive
                    pRec->nState.store( removed, memory_model::memory_order_release );
                }
                else {
                    // kernel already deleted
                    free_publication_record( pRec );
                }
            }
        }

        static void free_publication_record( publication_record_type * pRec )
        {
            cxx11_allocator().Delete( pRec );
        }

        void init()
        {
            assert( m_pThreadRec.get() == nullptr );
            publication_record_type * pRec = cxx11_allocator().New();
            m_pHead = pRec;
            pRec->pOwner = this;
            m_pThreadRec.reset( pRec );
        }

        void publish( publication_record_type * pRec )
        {
            assert( pRec->nState.load( memory_model::memory_order_relaxed ) == inactive );
            pRec->nState.store( active, memory_model::memory_order_release );

            // Insert record to publication list
            if ( m_pHead != static_cast<publication_record *>(pRec) ) {
                publication_record * p = m_pHead->pNext.load(memory_model::memory_order_relaxed);
                if ( p != static_cast<publication_record *>( pRec )) {
                    do {
                        pRec->pNext = p;
                        // Failed CAS changes p
                    } while ( !m_pHead->pNext.compare_exchange_weak( p, static_cast<publication_record *>(pRec),
                        memory_model::memory_order_release, atomics::memory_order_relaxed ));
                }
            }
        }

        void republish( publication_record_type * pRec )
        {
            if ( pRec->nState.load( memory_model::memory_order_relaxed ) != active ) {
                // The record has been excluded from publication list. Reinsert it
                publish( pRec );
            }
        }

        template <class Container>
        void try_combining( Container& owner, publication_record_type * pRec )
        {
            if ( m_Mutex.try_lock() ) {
                // The thread becomes a combiner
                lock_guard l( m_Mutex, std::adopt_lock_t() );

                // The record pRec can be excluded from publication list. Re-publish it
                republish( pRec );

                combining( owner );
                assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_Response );
            }
            else {
                // There is another combiner, wait while it executes our request
                if ( !wait_for_combining( pRec ) ) {
                    // The thread becomes a combiner
                    lock_guard l( m_Mutex, std::adopt_lock_t() );

                    // The record pRec can be excluded from publication list. Re-publish it
                    republish( pRec );

                    combining( owner );
                    assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_Response );
                }
            }
        }

        template <class Container>
        void combining( Container& owner )
        {
            // The thread is a combiner
            assert( !m_Mutex.try_lock() );

            for ( unsigned int nPass = 0; nPass < m_nCombinePassCount; ++nPass )
                if ( !combining_pass( owner ))
                    break;
        }

        template <class Container>
        bool combining_pass( Container& owner )
        {
            publication_record * pPrev = nullptr;
            publication_record * p = m_pHead;
            bool bOpDone = false;
            while ( p ) {
                switch ( p->nState.load( memory_model::memory_order_acquire )) {
                    case active:
                        if ( p->op() >= req_Operation ) {
                            owner.fc_apply( static_cast<publication_record_type *>(p) );
                            operation_done( *p );
                            bOpDone = true;
                        }
                        break;
                    case inactive:
                        // Only m_pHead can be inactive in the publication list
                        assert( p == m_pHead );
                        break;
                    case removed:
                        // The record should be removed
                        p = unlink_and_delete_record( pPrev, p );
                        continue;
                    default:
                        /// ??? That is impossible
                        assert(false);
                }
                pPrev = p;
                p = p->pNext.load( memory_model::memory_order_acquire );
            }
            return bOpDone;
        }

        bool wait_for_combining( publication_record_type * pRec )
        {
            back_off bkoff;
            while ( pRec->nRequest.load( memory_model::memory_order_acquire ) != req_Response ) {

                // The record can be excluded from publication list. Reinsert it
                republish( pRec );

                bkoff();

                if ( m_Mutex.try_lock() ) {
                    if ( pRec->nRequest.load( memory_model::memory_order_acquire ) == req_Response ) {
                        m_Mutex.unlock();
                        break;
                    }
                    // The thread becomes a combiner
                    return false;
                }
            }
            return true;
        }

        publication_record * unlink_and_delete_record( publication_record * pPrev, publication_record * p )
        {
            if ( pPrev ) {
                publication_record * pNext = p->pNext.load( memory_model::memory_order_acquire );
                if ( pPrev->pNext.compare_exchange_strong( p, pNext,
                    memory_model::memory_order_release, atomics::memory_order_relaxed ))
                {
                    free_publication_record( static_cast<publication_record_type *>( p ));
                }
                return pNext;
            }
            else {
                m_pHead = static_cast<publication_record_type *>( p->pNext.load( memory_model::memory_order_acquire ));
                free_publication_record( static_cast<publication_record_type *>( p ));
                return m_pHead;
            }
        }
    };

    class container
    {
    public:
        template <typename PubRecord>
        void fc_apply( PubRecord * )
        {
            assert( false );
        }

        template <typename Iterator>
        void fc_process( Iterator, Iterator )
        {
            assert( false );
        }
    };

} // namespace flat_combining

#endif // #ifndef FLAT_COMBINING_H
