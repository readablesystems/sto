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
        typedef wait_strategy::backoff<> wait_strategy; ///< Wait strategy
        typedef typename CDS_DEFAULT_ALLOCATOR allocator;          //< Allocator type (used for allocating publication_record_type data)
        typedef typename cds::opt::v::relaxed_ordering memory_model;    //< C++ memory model
        typedef typename wait_strategy::template make_publication_record<PublicationRecord>::type publication_record_type; //< Publication record type

    protected:
        typedef cds::details::Allocator< publication_record_type, allocator >   cxx11_allocator; //< internal helper cds::details::Allocator
        
        atomics::atomic<unsigned int>  nCount;   //< Total count of combining passes. Used as an age.
        publication_record_type *   pHead;    //< Head of publication list
        boost::thread_specific_ptr< publication_record_type > pThreadRec;   //< Thread-local publication record
        mutable global_lock_type    Mutex;    //< Global mutex
        mutable stat<>                Stat;     //< Internal statistics
        unsigned int const          nCompactFactor;    //< Publication list compacting factor
        unsigned int const          nCombinePassCount; //< Number of combining passes
        wait_strategy               waitStrategy;      //< Wait strategy

    public:
        kernel() : kernel( 1024, 8 ) {}
        
        kernel(unsigned int nCompactFactor, unsigned int nCombinePassCount)
            : nCount(0)
            , pHead( nullptr )
            , pThreadRec( tls_cleanup )
            , Mutex( 0 )
            , nCompactFactor((unsigned int)( cds::beans::ceil2( nCompactFactor ) - 1 ))   // binary mask
            , nCombinePassCount( nCombinePassCount )
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
        /**
            pRec is the publication record acquiring by \ref acquire_record earlier.
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
            Stat.onOperation();

            try_combining( owner, pRec );
        }

        // Trying to execute operation nOpId in batch-combine mode
        /**
            pRec is the publication record acquiring by acquire_record() earlier.
            owner is a container that owns flat combining kernel object.
            As a result the current thread can become a combiner or can wait for
            another combiner performs pRec operation.

            If the thread becomes a combiner, the kernel calls owner.fc_process()
            giving the container the full access over publication list. This function
            is useful for an elimination technique if the container supports any kind of
            that. The container can perform multiple pass through publication list.

            owner.fc_process() has two arguments - forward iterators on begin and end of
            publication list, see \ref iterator class. For each processed record the container
            should call operation_done() function to mark the record as processed.

            On the end of %batch_combine the combine() function is called
            to process rest of publication records.
        */
        template <class Container>
        void batch_combine( unsigned int nOpId, publication_record_type* pRec, Container& owner )
        {
            assert( nOpId >= req_Operation );
            assert( pRec );

            pRec->nRequest.store( nOpId, memory_model::memory_order_release );
            Stat.onOperation();

            try_batch_combining( owner, pRec );
        }

        // Invokes Func in exclusive mode
        /**
            Some operation in flat combining containers should be called in exclusive mode
            i.e the current thread should become the combiner to process the operation.
            The typical example is empty() function.
            
            %invoke_exclusive() allows do that: the current thread becomes the combiner,
            invokes f exclusively but unlike a typical usage the thread does not process any pending request.
            Instead, after end of f call the current thread wakes up a pending thread if any.
        */
        template <typename Func>
        void invoke_exclusive( Func f )
        {
            {
                f();
            }
            waitStrategy.wakeup( *this );
            Stat.onInvokeExclusive();
        }

        // Marks rec as executed
        /**
            This function should be called by container if batch_combine mode is used.
            For usual combining (see combine() ) this function is excess.
        */
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

    public:
        // Publication list iterator
        /**
            Iterators are intended for batch processing by container's
            fc_process function.
            The iterator allows iterate through active publication list.
        */
        class iterator
        {
            friend class kernel;
            publication_record_type * pRec;

        protected:
            iterator( publication_record_type * pRec )
                : pRec( pRec )
            {
                skip_inactive();
            }

            void skip_inactive()
            {
                while ( pRec && (pRec->nState.load( memory_model::memory_order_acquire ) != active
                                || pRec->op( memory_model::memory_order_relaxed) < req_Operation ))
                {
                    pRec = static_cast<publication_record_type*>(pRec->pNext.load( memory_model::memory_order_acquire ));
                }
            }

        public:
            // Initializes an empty iterator object
            iterator() : pRec( nullptr ) {}

            // Copy ctor
            iterator( iterator const& src ) : pRec( src.pRec ) {}

            // Pre-increment
            iterator& operator++()
            {
                assert( pRec );
                pRec = static_cast<publication_record_type *>( pRec->pNext.load( memory_model::memory_order_acquire ));
                skip_inactive();
                return *this;
            }

            // Post-increment
            iterator operator++(int)
            {
                assert( pRec );
                iterator it(*this);
                ++(*this);
                return it;
            }

            // Dereference operator, can return nullptr
            publication_record_type* operator ->()
            {
                return pRec;
            }

            // Dereference operator, the iterator should not be an end iterator
            publication_record_type& operator*()
            {
                assert( pRec );
                return *pRec;
            }

            // Iterator equality
            friend bool operator==( iterator it1, iterator it2 )
            {
                return it1.pRec == it2.pRec;
            }

            // Iterator inequality
            friend bool operator!=( iterator it1, iterator it2 )
            {
                return !( it1 == it2 );
            }
        };

        // Returns an iterator to the first active publication record
        iterator begin()    { return iterator(pHead); }

        // Returns an iterator to the end of publication list. Should not be dereferenced.
        iterator end()      { return iterator(); }

    public:
        // Gets current value of rec.nRequest
        /**
            This function is intended for invoking from a wait strategy
        */
        int get_operation( publication_record& rec )
        {
            return rec.op( memory_model::memory_order_acquire );
        }

        // Wakes up any waiting thread
        /**
            This function is intended for invoking from a wait strategy
        */
        void wakeup_any()
        {
            publication_record* pRec = pHead;
            while ( pRec ) {
                if ( pRec->nState.load( memory_model::memory_order_acquire ) == active
                  && pRec->op( memory_model::memory_order_acquire ) >= req_Operation )
                {
                    waitStrategy.notify( *this, static_cast<publication_record_type&>( *pRec ));
                    break;
                }
                pRec = pRec->pNext.load( memory_model::memory_order_acquire );
            }
        }

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
        void try_batch_combining( Container& owner, publication_record_type * pRec )
        {
            if ( TransactionTid::try_lock(Mutex) ) {
                // The thread becomes a combiner
                // The record pRec can be excluded from publication list. Re-publish it
                republish( pRec );

                batch_combining( owner );
                assert( pRec->op( memory_model::memory_order_relaxed ) == req_Response );
                TransactionTid::unlock(Mutex);
            }
            else {
                // There is another combiner, wait while it executes our request
                if ( !wait_for_combining( pRec ) ) {
                    // The thread becomes a combiner
                    // The record pRec can be excluded from publication list. Re-publish it
                    republish( pRec );

                    batch_combining( owner );
                    assert( pRec->op( memory_model::memory_order_relaxed ) == req_Response );
                    TransactionTid::unlock(Mutex);
                }
            }
        }

        template <class Container>
        void combining( Container& owner )
        {
            // The thread is a combiner
            assert( !TransactionTid::try_lock(Mutex) );

            unsigned int const nCurAge = nCount.fetch_add( 1, memory_model::memory_order_relaxed ) + 1;

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

        template <class Container>
        void batch_combining( Container& owner )
        {
            // The thread is a combiner
            assert( !TransactionTid::try_lock(Mutex) );

            unsigned int const nCurAge = nCount.fetch_add( 1, memory_model::memory_order_relaxed ) + 1;

            for ( unsigned int nPass = 0; nPass < nCombinePassCount; ++nPass )
                owner.fc_process( begin(), end() );

            combining_pass( owner, nCurAge );
            Stat.onCombining();
            if ( (nCurAge & nCompactFactor) == 0 )
                compact_list( nCurAge );
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

        template <typename Iterator>
        void fc_process( Iterator, Iterator )
        {
            assert( false );
        }
    };
    

} // namespace flat_combining

#endif // #ifndef FLAT_COMBINING_H
