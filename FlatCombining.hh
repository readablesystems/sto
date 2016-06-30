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
#include "Transaction.hh"

namespace flat_combining {
    
    /// Flat combining internal statistics
    template < typename Counter = std::atomic<unsigned> >
    struct stat
    {
        typedef Counter counter_type;   ///< Event counter type

        counter_type    nOperationCount   ;   ///< How many operations have been performed
        counter_type    nCombiningCount   ;   ///< Combining call count
        
        /// Returns current combining factor = nOperationCount / nCombiningCount
        double combining_factor() const {
            return nCombiningCount ? double( double(nOperationCount) / double(nCombiningCount) ) : 0.0;
        }

        void    onOperation()               { ++nOperationCount; }
        void    onCombining()               { ++nCombiningCount; }
    };


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
        atomics::atomic<unsigned int>           nAge;       ///< Age of the record
        atomics::atomic<publication_record *> pNext; ///< Next record in publication list
        void *                              pOwner;    ///< [internal data] Pointer to \ref kernel object that manages the publication list

        /// Initializes publication record
        publication_record()
            : nRequest( req_EmptyRecord )
            , nState( inactive )
            , nAge( 0 )
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
    template < typename PublicationRecord >
    class kernel
    {
    public:
        typedef PublicationRecord   publication_record_type;   ///< publication record type
        typedef typename TransactionTid::type global_lock_type;   ///< Global lock type
        typedef typename cds::backoff::delay_of<2> back_off;           ///< back-off strategy type
        typedef typename CDS_DEFAULT_ALLOCATOR allocator;          ///< Allocator type (used for allocating publication_record_type data)
        typedef typename cds::opt::v::relaxed_ordering memory_model;    ///< C++ memory model

    protected:
        typedef cds::details::Allocator< publication_record_type, allocator >   cxx11_allocator; ///< internal helper cds::details::Allocator

    protected:
        atomics::atomic<unsigned int>  nCount;   ///< Total count of combining passes. Used as an age.
        publication_record_type *                               pHead;    ///< Head of publication list
        boost::thread_specific_ptr< publication_record_type >   pThreadRec;   ///< Thread-local publication record
        mutable struct flat_combining::stat<>   stat_;       ///< Internal statistics
        mutable global_lock_type    global_dslock_;    ///< Global mutex
        unsigned int const          nCompactFactor;    ///< Publication list compacting factor
        unsigned int const          nCombinePassCount; ///< Number of combining passes

    public:
        /// Initializes the object
        kernel() : 
            nCount(0)
            , pHead( nullptr )
            , pThreadRec( tls_cleanup )
            , global_dslock_(0)
            , nCompactFactor( (unsigned int)( cds::beans::ceil2( nCompactFactor ) - 1 ))   // binary mask
            , nCombinePassCount( 8 )
        {
            init();
        }

        /// Initializes the object
        kernel(unsigned int nCombinePassCount) ///< Number of combining passes for combiner thread
            : nCount(0)
            , pHead( nullptr )
            , pThreadRec( tls_cleanup )
            , global_dslock_(0)
            , nCompactFactor( (unsigned int)( cds::beans::ceil2( nCompactFactor ) - 1 ))   // binary mask
            , nCombinePassCount( nCombinePassCount )
        {
            init();
        }

        /// Destroys the objects and mark all publication records as inactive
        ~kernel()
        {
            // mark all publication record as detached
            for ( publication_record * p = pHead; p; ) {
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
            publication_record_type * pRec = pThreadRec.get();
            if ( !pRec ) {
                // Allocate new publication record
                pRec = cxx11_allocator().New();
                pRec->pOwner = reinterpret_cast<void *>( this );
                pThreadRec.reset( pRec );
            }

            if ( pRec->nState.load( memory_model::memory_order_acquire ) != active )
                publish( pRec );

            assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_EmptyRecord );

            return pRec;
        }

        /// Marks publication record for the current thread as empty
        void release_record( publication_record_type * pRec ) {
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
        void combine( unsigned int nOpId, publication_record_type * pRec, Container& owner ) {
            assert( nOpId >= req_Operation );
            assert( pRec );
            pRec->nRequest.store( nOpId, memory_model::memory_order_release );

            stat_.onOperation();
            try_combining( owner, pRec );
        }

        /// Marks \p rec as executed
        void operation_done( publication_record& rec ) {
            rec.nRequest.store( req_Response, memory_model::memory_order_release );
        }

        stat<>& statistics() const { return stat_; }

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

        static void free_publication_record( publication_record_type * pRec ) {
            cxx11_allocator().Delete( pRec );
        }

        void init()
        {
            assert( pThreadRec.get() == nullptr );
            publication_record_type * pRec = cxx11_allocator().New();
            pHead = pRec;
            pRec->pOwner = this;
            pThreadRec.reset( pRec );
        }

        void publish( publication_record_type * pRec )
        {
            assert( pRec->nState.load( memory_model::memory_order_relaxed ) == inactive );
            pRec->nState.store( active, memory_model::memory_order_release );
            pRec->nAge.store( nCount.load(memory_model::memory_order_relaxed), memory_model::memory_order_release );

            // Insert record to publication list
            if ( pHead != static_cast<publication_record *>(pRec) ) {
                publication_record * p = pHead->pNext.load(memory_model::memory_order_relaxed);
                if ( p != static_cast<publication_record *>( pRec )) {
                    do {
                        pRec->pNext = p;
                        // Failed CAS changes p
                    } while ( !pHead->pNext.compare_exchange_weak( p, static_cast<publication_record *>(pRec),
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
            if ( TransactionTid::try_lock(global_dslock_) ) {
                // The thread becomes a combiner
                // The record pRec can be excluded from publication list. Re-publish it
                republish( pRec );

                combining( owner );
                assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_Response );
                TransactionTid::unlock(global_dslock_);
            }
            else {
                // There is another combiner, wait while it executes our request
                if ( !wait_for_combining( pRec ) ) {
                    // The thread becomes a combiner
                    // The record pRec can be excluded from publication list. Re-publish it
                    republish( pRec );

                    combining( owner );
                    assert( pRec->nRequest.load( memory_model::memory_order_relaxed ) == req_Response );
                    TransactionTid::unlock(global_dslock_);
                }
            }
        }

        template <class Container>
        void combining( Container& owner )
        {
            // The thread is a combiner
            assert( !TransactionTid::try_lock(global_dslock_) );

            unsigned int const nCurAge = nCount.fetch_add( 1, memory_model::memory_order_relaxed ) + 1;

            unsigned int nEmptyPassCount = 0;
            unsigned int nUsefulPassCount = 0;
            for ( unsigned int nPass = 0; nPass < nCombinePassCount; ++nPass ) {
                if ( combining_pass( owner, nCurAge ))
                    ++nUsefulPassCount;
                else if ( ++nEmptyPassCount > nUsefulPassCount )
                    break;
            }

            stat_.onCombining();
            if ( (nCurAge & nCompactFactor) == 0 )
                compact_list( nCurAge );
        }

        template <class Container>
        bool combining_pass( Container& owner, unsigned int const nCurAge )
        {
                publication_record* pPrev = nullptr;
                publication_record* p = pHead;
                bool bOpDone = false;
                while ( p ) {
                    switch ( p->nState.load( memory_model::memory_order_acquire )) {
                        case active:
                            if ( p->op() >= req_Operation ) {
                                p->nAge.store( nCurAge, memory_model::memory_order_release );
                                owner.fc_apply( static_cast<publication_record_type*>(p) );
                                operation_done( *p );
                                bOpDone = true;
                            }
                            break;
                        case inactive:
                            // Only pHead can be inactive in the publication list
                            assert( p == pHead );
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

                if ( TransactionTid::try_lock(global_dslock_) ) {
                    if ( pRec->nRequest.load( memory_model::memory_order_acquire ) == req_Response ) {
                        assert(TransactionTid::is_locked_here(global_dslock_));
                        TransactionTid::unlock(global_dslock_);
                        break;
                    }
                    // The thread becomes a combiner
                    return false;
                }
            }
            return true;
        }

        void compact_list( unsigned int const nCurAge ) {
            // Thinning publication list
            publication_record * pPrev = nullptr;
            for ( publication_record * p = pHead; p; ) {
                if ( p->nState.load( memory_model::memory_order_acquire ) == active
                  && p->nAge.load( memory_model::memory_order_acquire ) + nCompactFactor < nCurAge )
                {
                    if ( pPrev ) {
                        publication_record * pNext = p->pNext.load( memory_model::memory_order_acquire );
                        if ( pPrev->pNext.compare_exchange_strong( p, pNext,
                            memory_model::memory_order_release, atomics::memory_order_relaxed ))
                        {
                            p->nState.store( inactive, memory_model::memory_order_release );
                            p = pNext;
                            continue;
                        }
                    }
                }
                pPrev = p;
                p = p->pNext.load( memory_model::memory_order_acquire );
            }
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
                pHead = static_cast<publication_record_type *>( p->pNext.load( memory_model::memory_order_acquire ));
                free_publication_record( static_cast<publication_record_type *>( p ));
                return pHead;
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
