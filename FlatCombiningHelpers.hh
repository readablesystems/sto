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

#ifndef FLAT_COMBINING_WAIT_STRATEGY_H
#define FLAT_COMBINING_WAIT_STRATEGY_H

#include <cds/algo/flat_combining/defs.h>
#include <cds/algo/backoff_strategy.h>
#include <mutex>
#include <condition_variable>
#include <boost/thread/tss.hpp>  // thread_specific_ptr

namespace flat_combining {
    enum request_value
    {
        req_EmptyRecord,    //< Publication record is empty
        req_Response,       //< Operation is done
        req_Operation       //< First operation id for derived classes
    };

    enum record_state {
        inactive,       //< Record is inactive
        active,         //< Record is active
        removed         //< Record should be removed
    };

    /* 
     * Each data structure based on flat combining contains a class derived from publication_record
    */
    struct publication_record {
        atomics::atomic<unsigned int>           nRequest;   //< Request field (depends on data structure)
        atomics::atomic<unsigned int>           nState;     //< Record state: inactive, active, removed
        atomics::atomic<unsigned int>           nAge;       //< Age of the record
        atomics::atomic<publication_record *>   pNext;      //< Next record in publication list
        void *                                  pOwner;     //< [internal data] Pointer to kernel object that manages the publication list

        // Initializes publication record
        publication_record()
            : nRequest( req_EmptyRecord )
            , nState( inactive )
            , nAge( 0 )
            , pNext( nullptr )
            , pOwner( nullptr )
        {}

        // Returns the value of nRequest field
        unsigned int op( atomics::memory_order mo = atomics::memory_order_relaxed ) const
        {
            return nRequest.load( mo );
        }

        // Checks if the operation is done
        bool is_done() const
        {
            return nRequest.load( atomics::memory_order_relaxed ) == req_Response;
        }
    };

    // Flat combining internal statistics
    template <typename Counter = cds::atomicity::event_counter >
    struct stat
    {
        typedef Counter counter_type;   //< Event counter type

        counter_type    nOperationCount   ;   //< How many operations have been performed
        counter_type    nCombiningCount   ;   //< Combining call count
        counter_type    nCompactPublicationList; //< Count of publication list compacting
        counter_type    nDeactivatePubRecord; //< How many publication records were deactivated during compacting
        counter_type    nActivatePubRecord;   //< Count of publication record activating
        counter_type    nPubRecordCreated ;   //< Count of created publication records
        counter_type    nPubRecordDeleted ;   //< Count of deleted publication records
        counter_type    nPassiveWaitCall;     //< Count of passive waiting call (kernel::wait_for_combining())
        counter_type    nPassiveWaitIteration;//< Count of iteration inside passive waiting
        counter_type    nPassiveWaitWakeup;   //< Count of forcing wake-up of passive wait cycle
        counter_type    nInvokeExclusive;     //< Count of exclusive calls 
        counter_type    nWakeupByNotifying;   //< How many times the passive thread be waked up by a notification
        counter_type    nPassiveToCombiner;   //< How many times the passive thread becomes the combiner

        // Returns current combining factor
        double combining_factor() const {
            return nCombiningCount.get() ? double( nOperationCount.get()) / nCombiningCount.get() : 0.0;
        }

        void    onOperation()               { ++nOperationCount;          }
        void    onCombining()               { ++nCombiningCount;          }
        void    onCompactPublicationList()  { ++nCompactPublicationList;  }
        void    onDeactivatePubRecord()     { ++nDeactivatePubRecord;     }
        void    onActivatePubRecord()       { ++nActivatePubRecord;       }
        void    onCreatePubRecord()         { ++nPubRecordCreated;        }
        void    onDeletePubRecord()         { ++nPubRecordDeleted;        }
        void    onPassiveWait()             { ++nPassiveWaitCall;         }
        void    onPassiveWaitIteration()    { ++nPassiveWaitIteration;    }
        void    onPassiveWaitWakeup()       { ++nPassiveWaitWakeup;       }
        void    onInvokeExclusive()         { ++nInvokeExclusive;         }
        void    onWakeupByNotifying()       { ++nWakeupByNotifying;       }
        void    onPassiveToCombiner()       { ++nPassiveToCombiner;       }
    };

    namespace wait_strategy {

        // Empty wait strategy
        /**
            Empty wait strategy is just spinning on request field.
            All functions are empty.
        */
        struct empty
        {
            // Metafunction for defining a publication record for flat combining technique
            template <typename PublicationRecord>
            struct make_publication_record {
                typedef PublicationRecord type; //< Metafunction result
            };

            // Prepares the strategy
            /**
                This function is called before enter to waiting cycle.
                Some strategies need to prepare its thread-local data in  rec.

                 PublicationRecord is thread's publication record of type  make_publication_record::type
            */
            template <typename PublicationRecord>
            void prepare( PublicationRecord& rec )
            {
                CDS_UNUSED( rec );
            }

            // Waits for the combiner
            /**
                The thread calls this function to wait for the combiner process
                the request.
                The function returns  true if the thread was waked up by the combiner,
                otherwise it should return  false.

                 FCKernel is a  flat_combining::kernel object,
                 PublicationRecord is thread's publication record of type  make_publication_record::type
            */
            template <typename FCKernel, typename PublicationRecord>
            bool wait( FCKernel& fc, PublicationRecord& rec )
            {
                CDS_UNUSED( fc );
                CDS_UNUSED( rec );
                return false;
            }

            // Wakes up the thread
            /**
                The combiner calls  %notify() when it has been processed the request.

                 FCKernel is a  flat_combining::kernel object,
                 PublicationRecord is thread's publication record of type  make_publication_record::type
            */
            template <typename FCKernel, typename PublicationRecord>
            void notify( FCKernel& fc, PublicationRecord& rec )
            {
                CDS_UNUSED( fc );
                CDS_UNUSED( rec );
            }

            // Moves control to other thread
            /**
                This function is called when the thread becomes the combiner
                but the request of the thread is already processed.
                The strategy may call  fc.wakeup_any() instructs the kernel
                to wake up any pending thread.

                 FCKernel is a  flat_combining::kernel object,
            */
            template <typename FCKernel>
            void wakeup( FCKernel& fc )
            {
                CDS_UNUSED( fc );
            }
        };

        // Back-off wait strategy
        /**
            Template argument  Backoff specifies back-off strategy, default is cds::backoff::delay_of<2>
        */
        template <typename BackOff = cds::backoff::delay_of<2>>
        struct backoff
        {
            typedef BackOff back_off;   //< Back-off strategy

            // Incorporates back-off strategy into publication record
            template <typename PublicationRecord>
            struct make_publication_record 
            {
                
                struct type: public PublicationRecord
                {
                    back_off bkoff;
                };
                
            };

            // Resets back-off strategy in  rec
            template <typename PublicationRecord>
            void prepare( PublicationRecord& rec )
            {
                rec.bkoff.reset();
            }

            // Calls back-off strategy
            template <typename FCKernel, typename PublicationRecord>
            bool wait( FCKernel& /*fc*/, PublicationRecord& rec )
            {
                rec.bkoff();
                return false;
            }

            // Does nothing
            template <typename FCKernel, typename PublicationRecord>
            void notify( FCKernel& /*fc*/, PublicationRecord& /*rec*/ )
            {}

            // Does nothing
            template <typename FCKernel>
            void wakeup( FCKernel& )
            {}
        };

        // Wait strategy based on the single mutex and the condition variable
        /**
            The strategy shares the mutex and conditional variable for all thread.

            Template parameter  Milliseconds specifies waiting duration;
            the minimal value is 1.
        */
        template <int Milliseconds = 2>
        class single_mutex_single_condvar
        {
        
            std::mutex m_mutex;
            std::condition_variable m_condvar;

            typedef std::unique_lock< std::mutex > unique_lock;
        

        public:
            enum {
                c_nWaitMilliseconds = Milliseconds < 1 ? 1 : Milliseconds //< Waiting duration
            };

            // Empty metafunction
            template <typename PublicationRecord>
            struct make_publication_record {
                typedef PublicationRecord type; //< publication record type
            };

            // Does nothing
            template <typename PublicationRecord>
            void prepare( PublicationRecord& /*rec*/ )
            {}

            // Sleeps on condition variable waiting for notification from combiner
            template <typename FCKernel, typename PublicationRecord>
            bool wait( FCKernel& fc, PublicationRecord& rec )
            {
                if ( fc.get_operation( rec ) >= req_Operation ) {
                    unique_lock lock( m_mutex );
                    if ( fc.get_operation( rec ) >= req_Operation )
                        return m_condvar.wait_for( lock, std::chrono::milliseconds( c_nWaitMilliseconds )) == std::cv_status::no_timeout;
                }
                return false;
            }

            // Calls condition variable function  notify_all()
            template <typename FCKernel, typename PublicationRecord>
            void notify( FCKernel& fc, PublicationRecord& rec )
            {
                m_condvar.notify_all();
            }

            // Calls condition variable function  notify_all()
            template <typename FCKernel>
            void wakeup( FCKernel& /*fc*/ )
            {
                m_condvar.notify_all();
            }
        };

        // Wait strategy based on the single mutex and thread-local condition variables
        /**
            The strategy shares the mutex, but each thread has its own conditional variable

            Template parameter  Milliseconds specifies waiting duration;
            the minimal value is 1.
        */
        template <int Milliseconds = 2>
        class single_mutex_multi_condvar
        {
        
            std::mutex m_mutex;

            typedef std::unique_lock< std::mutex > unique_lock;
        

        public:
            enum {
                c_nWaitMilliseconds = Milliseconds < 1 ? 1 : Milliseconds  //< Waiting duration
            };

            // Incorporates a condition variable into  PublicationRecord
            template <typename PublicationRecord>
            struct make_publication_record {
                // Metafunction result
                struct type: public PublicationRecord
                {
                    
                    std::condition_variable m_condvar;
                    
                };
            };

            // Does nothing
            template <typename PublicationRecord>
            void prepare( PublicationRecord& /*rec*/ )
            {}

            // Sleeps on condition variable waiting for notification from combiner
            template <typename FCKernel, typename PublicationRecord>
            bool wait( FCKernel& fc, PublicationRecord& rec )
            {
                if ( fc.get_operation( rec ) >= req_Operation ) {
                    unique_lock lock( m_mutex );
                    if ( fc.get_operation( rec ) >= req_Operation )
                        return rec.m_condvar.wait_for( lock, std::chrono::milliseconds( c_nWaitMilliseconds )) == std::cv_status::no_timeout;
                }
                return false;
            }

            // Calls condition variable function  notify_one()
            template <typename FCKernel, typename PublicationRecord>
            void notify( FCKernel& fc, PublicationRecord& rec )
            {
                rec.m_condvar.notify_one();
            }

            // Calls  fc.wakeup_any() to wake up any pending thread
            template <typename FCKernel>
            void wakeup( FCKernel& fc )
            {
                fc.wakeup_any();
            }
        };

        // Wait strategy where each thread has a mutex and a condition variable
        /**
            Template parameter  Milliseconds specifies waiting duration;
            the minimal value is 1.
        */
        template <int Milliseconds = 2>
        class multi_mutex_multi_condvar
        {
        
            typedef std::unique_lock< std::mutex > unique_lock;
        
        public:
            enum {
                c_nWaitMilliseconds = Milliseconds < 1 ? 1 : Milliseconds   //< Waiting duration
            };

            // Incorporates a condition variable and a mutex into  PublicationRecord
            template <typename PublicationRecord>
            struct make_publication_record {
                // Metafunction result
                struct type: public PublicationRecord
                {
                    
                    std::mutex              m_mutex;
                    std::condition_variable m_condvar;
                    
                };
            };

            // Does nothing
            template <typename PublicationRecord>
            void prepare( PublicationRecord& /*rec*/ ) {}

            // Sleeps on condition variable waiting for notification from combiner
            template <typename FCKernel, typename PublicationRecord>
            bool wait( FCKernel& fc, PublicationRecord& rec )
            {
                if ( fc.get_operation( rec ) >= req_Operation ) {
                    unique_lock lock( rec.m_mutex );
                    if ( fc.get_operation( rec ) >= req_Operation )
                        return rec.m_condvar.wait_for( lock, std::chrono::milliseconds( c_nWaitMilliseconds )) == std::cv_status::no_timeout;
                }
                return false;
            }

            // Calls condition variable function  notify_one()
            template <typename FCKernel, typename PublicationRecord>
            void notify( FCKernel& /*fc*/, PublicationRecord& rec )
            {
                rec.m_condvar.notify_one();
            }

            // Calls  fc.wakeup_any() to wake up any pending thread
            template <typename FCKernel>
            void wakeup( FCKernel& fc )
            {
                fc.wakeup_any();
            }
        };
    } // namespace wait_strategy
}

#endif
