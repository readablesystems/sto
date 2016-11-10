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

#include <cds/algo/atomic.h>
#include <cds/details/allocator.h>
#include <cds/algo/int_algo.h>
#include <cds/opt/options.h>
#include <boost/thread/tss.hpp>     // thread_specific_ptr
#include "Transaction.hh"
#include "FlatCombiningHelpers.hh"

namespace flat_combining {

    template <typename PublicationRecord>
    class kernel {
    public:
        typedef typename TransactionTid::type global_lock_type;   //< Global lock type
        typedef wait_strategy::backoff<cds::backoff::delay_of<2>> wait_strategy; ///< Wait strategy
        typedef typename CDS_DEFAULT_ALLOCATOR allocator;          //< Allocator type (used for allocating publication_record_type data)
        typedef typename cds::opt::v::relaxed_ordering memory_model;    //< C++ memory model
        typedef typename wait_strategy::template make_publication_record<PublicationRecord>::type publication_record_type; //< Publication record type

    protected:
        typedef cds::details::Allocator< publication_record_type, allocator >   cxx11_allocator; //< internal helper cds::details::Allocator
        
        atomics::atomic<unsigned int>  nCount;   //< Total count of combining passes. Used as an age.
        publication_record_type *   pHead;    //< Head of publication list
        boost::thread_specific_ptr< publication_record_type > pThreadRec;   //< Thread-local publication record
        mutable global_lock_type    Mutex;    //< Global mutex
        mutable stat<>              Stat;     //< Internal statistics
        unsigned int const          nCompactFactor;    //< Publication list compacting factor
        unsigned int const          nCombinePassCount; //< Number of combining passes
        wait_strategy               waitStrategy;      //< Wait strategy
//        timeval                     prev_t;      //< Wait strategy

    public:
        kernel() : kernel( 1024, 8 ) {}
        
        kernel(unsigned int nCompactFactor, unsigned int nCombinePassCount)
            : nCount(0)
            , pHead( nullptr )
            , pThreadRec( tls_cleanup )
            , Mutex( 0 )
            , nCompactFactor((unsigned int)( cds::beans::ceil2( nCompactFactor ) - 1 ))   // binary mask
            , nCombinePassCount( nCombinePassCount )
            //, prev_t(0)
        {
            init();
        }

        // Destroys the objects and mark all publication records as inactive
        ~kernel()
        {
            // mark all publication record as detached
            for ( publication_record* p = pHead; p; ) {
                p->pOwner = nullptr;

                publication_record * pRec = p;
                p = p->pNext.load( memory_model::memory_order_relaxed );
                if ( pRec->nState.load( memory_model::memory_order_acquire ) == removed )
                    free_publication_record( static_cast<publication_record_type *>( pRec ));
            }
        }

        /*  
         *  Gets publication list record for the current thread
         *  If there is no publication record for the current thread
         *  the function allocates it.
        */
        publication_record_type * acquire_record()
        {
            publication_record_type * pRec = pThreadRec.get();
            if ( !pRec ) {
                // Allocate new publication record
                pRec = cxx11_allocator().New();
                pRec->pOwner = reinterpret_cast<void *>( this );
                pThreadRec.reset( pRec );
                Stat.onCreatePubRecord();
            }

            if ( pRec->nState.load( memory_model::memory_order_acquire ) != active )
                publish( pRec );

            assert( pRec->op() == req_EmptyRecord );

            return pRec;
        }

        // Marks publication record for the current thread as empty
        void release_record( publication_record_type * pRec )
        {
            assert( pRec->is_done() );
            pRec->nRequest.store( req_EmptyRecord, memory_model::memory_order_release );
        }

        // Trying to execute operation nOpId
        template <class Container>
        void combine( unsigned int nOpId, publication_record_type * pRec, Container& owner )
        {
            assert( nOpId >= req_Operation );
            assert( pRec );

            pRec->nRequest.store( nOpId, memory_model::memory_order_release );
            Stat.onOperation();

            try_combining( owner, pRec );
        }

        // Marks rec as executed
        void operation_done( publication_record& rec )
        {
            rec.nRequest.store( req_Response, memory_model::memory_order_release );
            waitStrategy.notify( *this, static_cast<publication_record_type&>( rec ));
        }

        // Internal statistics
        stat<> const& statistics() const { return Stat; }

        // For container classes based on flat combining
        stat<>& internal_statistics() const { return Stat; }

        // Returns the compact factor
        unsigned int compact_factor() const { return nCompactFactor + 1; }

        // Returns number of combining passes for combiner thread
        unsigned int combine_pass_count() const { return nCombinePassCount; }

    private:
        static void tls_cleanup( publication_record_type* pRec )
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

        static void free_publication_record( publication_record_type* pRec )
        {
            cxx11_allocator().Delete( pRec );
        }

        void init()
        {
            assert( pThreadRec.get() == nullptr );
            publication_record_type* pRec = cxx11_allocator().New();
            pHead = pRec;
            pRec->pOwner = this;
            pThreadRec.reset( pRec );
            Stat.onCreatePubRecord();
        }

        void publish( publication_record_type* pRec )
        {
            assert( pRec->nState.load( memory_model::memory_order_relaxed ) == inactive );

            pRec->nAge.store( nCount.load(memory_model::memory_order_relaxed), memory_model::memory_order_release );
            pRec->nState.store( active, memory_model::memory_order_release );

            // Insert record to publication list
            if ( pHead != static_cast<publication_record *>(pRec) ) {
                publication_record * p = pHead->pNext.load(memory_model::memory_order_relaxed);
                if ( p != static_cast<publication_record *>( pRec )) {
                    do {
                        pRec->pNext = p;
                        // Failed CAS changes p
                    } while ( !pHead->pNext.compare_exchange_weak( p, static_cast<publication_record *>(pRec),
                        memory_model::memory_order_release, atomics::memory_order_relaxed ));
                    Stat.onActivatePubRecord();
                }
            }
        }

        void republish( publication_record_type* pRec )
        {
            if ( pRec->nState.load( memory_model::memory_order_relaxed ) != active ) {
                // The record has been excluded from publication list. Reinsert it
                publish( pRec );
                Stat.onRepublishPubRecord();
            }
        }

        template <class Container>
        void try_combining( Container& owner, publication_record_type* pRec )
        {
            if ( TransactionTid::try_lock(Mutex) ) {
                // The thread becomes a combiner
                // The record pRec can be excluded from publication list. Re-publish it
                republish( pRec );

                combining( owner );
                assert( pRec->op( memory_model::memory_order_relaxed ) == req_Response );
                TransactionTid::unlock(Mutex);
            }
            else {
                // There is another combiner, wait while it executes our request
                if ( !wait_for_combining( pRec ) ) {
                    // The thread becomes a combiner
                    // The record pRec can be excluded from publication list. Re-publish it
                    republish( pRec );

                    combining( owner );
                    assert( pRec->op( memory_model::memory_order_relaxed ) == req_Response );
                    TransactionTid::unlock(Mutex);
                }
            }
        }

        template <class Container>
        void combining( Container& owner )
        {
            // The thread is a combiner
            //assert( !TransactionTid::try_lock(Mutex) );

            unsigned int const nCurAge = nCount.fetch_add( 1, memory_model::memory_order_relaxed ) + 1;
            /*if (prev_t.tv_usec == 0) {
                gettimeofday(&prev_t);
            } else {
                timeval t;
                gettimeofday(&t);
            }*/

            unsigned int nEmptyPassCount = 0;
            unsigned int nUsefulPassCount = 0;
            for ( unsigned int nPass = 0; nPass < nCombinePassCount; ++nPass ) {
                if ( combining_pass( owner, nCurAge ))
                    ++nUsefulPassCount;
                else if ( ++nEmptyPassCount > nUsefulPassCount )
                    break;
            }

            Stat.onCombining();
            if ( (nCurAge & nCompactFactor) == 0 )
                compact_list( nCurAge );
        }

        template <class Container>
        bool combining_pass( Container& owner, unsigned int nCurAge )
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
                        // ??? That is impossible
                        assert(false);
                }
                pPrev = p;
                p = p->pNext.load( memory_model::memory_order_acquire );
            }
            return bOpDone;
        }

        bool wait_for_combining( publication_record_type * pRec )
        {
            waitStrategy.prepare( *pRec );
            Stat.onPassiveWait();

            while ( pRec->op( memory_model::memory_order_acquire ) != req_Response ) {
                // The record can be excluded from publication list. Reinsert it
                republish( pRec );

                Stat.onPassiveWaitIteration();

                // Wait while operation processing
                if ( waitStrategy.wait( *this, *pRec ))
                    Stat.onWakeupByNotifying();

                if ( TransactionTid::try_lock(Mutex) ) {
                    if ( pRec->op( memory_model::memory_order_acquire ) == req_Response ) {
                        // Operation is done
                        TransactionTid::unlock(Mutex);

                        // Wake up a pending threads
                        waitStrategy.wakeup( *this );
                        Stat.onPassiveWaitWakeup();

                        break;
                    }
                    // The thread becomes a combiner
                    Stat.onPassiveToCombiner();
                    return false;
                }
            }
            return true;
        }

        void compact_list( unsigned int const nCurAge )
        {
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
                            Stat.onDeactivatePubRecord();
                            continue;
                        }
                    }
                }
                pPrev = p;
                p = p->pNext.load( memory_model::memory_order_acquire );
            }

            Stat.onCompactPublicationList();
        }

        publication_record * unlink_and_delete_record( publication_record * pPrev, publication_record * p )
        {
            if ( pPrev ) {
                publication_record * pNext = p->pNext.load( memory_model::memory_order_acquire );
                if ( pPrev->pNext.compare_exchange_strong( p, pNext,
                    memory_model::memory_order_release, atomics::memory_order_relaxed ))
                {
                    free_publication_record( static_cast<publication_record_type *>( p ));
                    Stat.onDeletePubRecord();
                }
                return pNext;
            }
            else {
                pHead = static_cast<publication_record_type *>( p->pNext.load( memory_model::memory_order_acquire ));
                free_publication_record( static_cast<publication_record_type *>( p ));
                Stat.onDeletePubRecord();
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
    };
    

} // namespace flat_combining

#endif // #ifndef FLAT_COMBINING_H
