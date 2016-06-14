////////////////////////////////////////////////////////////////////////////////
// File    : main.cpp
// Author  : Ms.Moran Tzafrir  email: morantza@gmail.com
// Written : 13 October 2009
// 
// Multi-Platform C++ framework example benchamrk programm
//
// Copyright (C) 2009 Moran Tzafrir.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
////////////////////////////////////////////////////////////////////////////////
// TODO:
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//INCLUDE DIRECTIVES
////////////////////////////////////////////////////////////////////////////////
#include <string>
#include <iostream>

//general includes .....................................
#include "../framework/cpp_framework.h"
#include "Configuration.h"

//FC research includes .................................
#include "../data_structures/FCQueue.h"
#include "../data_structures/MSQueue.h"
#include "../data_structures/BasketQueue.h"
#include "../data_structures/LFStack.h"
#include "../data_structures/FCStack.h"
#include "../data_structures/LazySkipList.h"
#include "../data_structures/OyamaQueue.h"
#include "../data_structures/OyamaQueueCom.h"
#include "../data_structures/ComTreeQueue.h"
#include "../data_structures/EliminationStack.h"
#include "../data_structures/LFSkipList.h"
#include "../data_structures/FCPairingHeap.h"
#include "../data_structures/FCSkipList.h"

////////////////////////////////////////////////////////////////////////////////
//CONSTS
////////////////////////////////////////////////////////////////////////////////
static final int _ACTIONS_ARY_SIZE = 2*1024*1024;

////////////////////////////////////////////////////////////////////////////////
//GLOBALS
////////////////////////////////////////////////////////////////////////////////
static int						_num_ds;
static ITest*					_gDS[1024];
static Configuration			_gConfiguration;

static Random					_gRand;
static int						_gActionAry[_ACTIONS_ARY_SIZE];
static int						_gTotalRandNum;

static int						_g_thread_fill_table_size;
static int*						_gRandNumAry;
static tick_t volatile *		_gThreadResultAry;

static int						_gNumProcessors;
	   int						_gNumThreads;
static int						_gThroughputTime;

static Thread**					_gThreads;
static AtomicInteger			_gThreadStartCounter(0);
static AtomicInteger			_gThreadEndCounter(0);
static VolatileType<tick_t>		_gStartTime(0UL);
static VolatileType<tick_t>		_gEndTime(0UL);
static VolatileType<int>		_gIsStopThreads(0UL);

static tick_t					_gResult = 0L;
static tick_t					_gResultAdd = 0L;
static tick_t					_gResultRemove = 0L;
static tick_t					_gResultPeek = 0L;

static _u64 volatile			_seed;
static boolean					_is_tm=false;
static boolean					_is_view=false;

////////////////////////////////////////////////////////////////////////////////
//FORWARD DECLARETIONS
////////////////////////////////////////////////////////////////////////////////
void RunBenchmark();
void PrepareActions();
void PrepareRandomNumbers(final int size);
int NearestPowerOfTwo(final int x);
ITest* CreateDataStructure(char* final alg_name);

////////////////////////////////////////////////////////////////////////////////
//TYPEDEFS
////////////////////////////////////////////////////////////////////////////////
class MixThread : public Thread {
public:
	final int _threadNo;

	MixThread (final int inThreadNo) : _threadNo(inThreadNo) {}

	void run() {
		//fill table ...........................................................
		for (int iNum=0; iNum < (_g_thread_fill_table_size/16); ++iNum) {

				for (int iDb=0; iDb<_num_ds; ++iDb) {
					for (int i=0; i<16; ++i) {
						_gDS[iDb]->add(_threadNo, Random::getRandom(_seed, _gConfiguration._capacity) + 2);
					}
				}

		}
		for (int iDb=0; iDb<_num_ds; ++iDb) {
			_gDS[iDb]->cas_reset(_threadNo);
		}

		//save start benchmark time ............................................
		final int start_counter = _gThreadStartCounter.getAndIncrement();
		if(start_counter == (_gNumThreads-1))
			_gStartTime = System::currentTimeMillis();
		while((_gNumThreads) != _gThreadStartCounter.get()) {int i=_gThreadStartCounter.get();}

		//start thread benchmark ...............................................
		tick_t action_counter = 0;
		int iNumAdd = start_counter*1024;
		int iNumRemove = start_counter*1024;
		int iNumContain = start_counter*1024;
		int iOp = start_counter*128;
		do {
			final int op = _gActionAry[iOp];

			if(1==op) {
				for (int iDb=0; iDb<_num_ds; ++iDb) {
					final int enq_value = _gDS[iDb]->add(_threadNo, _gRandNumAry[iNumAdd]);
					++iNumAdd;
					if(iNumAdd >= _gTotalRandNum)
						iNumAdd=0;
				}
				++action_counter;
			} else if(2==op) {
				for (int iDb=0; iDb<_num_ds; ++iDb) {
					final int deq_value = _gDS[iDb]->remove(_threadNo, _gRandNumAry[iNumRemove]);
					++iNumRemove;
					if(iNumRemove >= _gTotalRandNum) { iNumRemove=0; }
				}
				++action_counter;
			} else {
				for (int iDb=0; iDb<_num_ds; ++iDb) {
					_gDS[iDb]->contain(_threadNo, _gRandNumAry[iNumContain]);
					++iNumContain;
					if(iNumContain >= _gTotalRandNum) {	iNumContain=0; }
				}
				++action_counter;
			}

			++iOp;
			if (iOp >= _ACTIONS_ARY_SIZE) {
				iOp=0;
			}

			if (_gConfiguration._read_write_delay > 0) {
				_gDS[0]->post_computation(_threadNo);
			}

			if (0 != _gIsStopThreads) {
				break;
			}

		} while(true);

		//save end benchmark time ..............................................
		final int end_counter = _gThreadEndCounter.getAndIncrement();
		if(end_counter == (_gNumThreads-1)) {
			_gEndTime = System::currentTimeMillis();
		}

		//save thread benchmark result .........................................
		_gThreadResultAry[_threadNo] = action_counter;
	}
};

////////////////////////////////////////////////////////////////////////////////
//MAIN
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {

	//..........................................................................
	_seed = Random::getSeed();

	//contaminate memory manager ...............................................
	for (int i=0; i< (1024*64); ++i) {
		void* final rand_mem = malloc( _gRand.nextInt(128)+1 );
		free(rand_mem);
	}
	
	//read benchmark configuration .............................................
	if(!_gConfiguration.read(argc, argv)) {
		if(!_gConfiguration.read()) {
			System_out_println("USAGE: <algorithm> <testNum> <numThreads> <numActions> <maxKey> <insertOps> <deleteOps> <unrandom> <loadFactor> <badHash> <initialCount> <_throughput_time>");
			System::exit(-1);
		}
	}

	//initialize global variables ..............................................
	_gNumProcessors     = 1; //Runtime.getRuntime().availableProcessors();
	_gNumThreads        = _gConfiguration._no_of_threads;
	_gTotalRandNum      = Math::Max(_gConfiguration._capacity, 4*1024*1024);
	_gThroughputTime    = _gConfiguration._throughput_time;

	//prepare the random numbers ...............................................
	System_err_println("");
	System_err_println("    START create random numbers.");
	PrepareActions();
	PrepareRandomNumbers(_gTotalRandNum);
	System_err_println("    END   creating random numbers.");
	System_err_println("");
	//System.gc();

	//run the benchmark ........................................................
	RunBenchmark();

	//print results ............................................................
	if(0 == _gConfiguration._is_dedicated_mode) {
		System_out_format(" %d", (unsigned int)_gResult);
	} else {
		System_out_format(" %d %d %d", (unsigned int)_gResultAdd, (unsigned int)_gResultRemove, (unsigned int)_gResultPeek);
	}
	Thread::sleep(1*1000);
	for (int iDb=0; iDb<_num_ds; ++iDb) {
		_gDS[iDb]->print_custom();
		delete _gDS[iDb]; 
		_gDS[iDb] = null;
	}
}

////////////////////////////////////////////////////////////////////////////////
//HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////
void RunBenchmark() {
	//print test information ...................................................
	System_err_println("Benchmark Curr: ");
	System_err_println("--------------");
	System_err_println("    numOfThreads:      " + Integer::toString( _gConfiguration._no_of_threads));

	System_err_println("    Algorithm1 Name:   " + std::string(_gConfiguration._alg1_name));
	System_err_println("    Algorithm1 Num:    " + Integer::toString(_gConfiguration._alg1_num));
	System_err_println("    Algorithm2 Name:   " + std::string(_gConfiguration._alg2_name));
	System_err_println("    Algorithm2 Num:    " + Integer::toString(_gConfiguration._alg2_num));
	System_err_println("    Algorithm3 Name:   " + std::string(_gConfiguration._alg3_name));
	System_err_println("    Algorithm3 Num:    " + Integer::toString(_gConfiguration._alg3_num));
	System_err_println("    Algorithm4 Name:   " + std::string(_gConfiguration._alg4_name));
	System_err_println("    Algorithm4 Num:    " + Integer::toString(_gConfiguration._alg4_num));

	System_err_println("    NumProcessors:     " + Integer::toString(_gNumProcessors));
	System_err_println("    testNo:            " + Integer::toString(_gConfiguration._test_no));

	System_err_println("    addOps:            " + Integer::toString(_gConfiguration._add_ops));
	System_err_println("    removeOps:         " + Integer::toString(_gConfiguration._remove_ops));
	System_err_println("    throughput_time:   " + Integer::toString(_gConfiguration._throughput_time));
	System_err_println("    is_dedicated_mode: " + Integer::toString(_gConfiguration._is_dedicated_mode));
	System_err_println("    tm_status:         " + Integer::toString(_gConfiguration._tm_status) + (std::string)("   (0=Norm; else View)"));
	System_err_println("    read_write_delay:  " + Integer::toString(_gConfiguration._is_dedicated_mode));

	_is_view = (0 != _gConfiguration._tm_status);

	char _sprintf_str[1024];
	sprintf(_sprintf_str, "%f",  _gConfiguration._load_factor);
	System_err_println("    loadFactor:        " + (std::string)(_sprintf_str));

	System_err_println("    initialCapacity:   " + Integer::toString(_gConfiguration._capacity));
	System_err_println("");
	ITest::_num_post_read_write = _gConfiguration._read_write_delay;

	//create appropriate data-structure ........................................
	_num_ds=0;
	for (int i=0; i<(_gConfiguration._alg1_num); ++i) {
		ITest* tmp = CreateDataStructure(_gConfiguration._alg1_name);
		if(null != tmp) {
			_gDS[_num_ds++] = tmp;
		}
	}
	for (int i=0; i<(_gConfiguration._alg2_num); ++i) {
		ITest* tmp = CreateDataStructure(_gConfiguration._alg2_name);
		if(null != tmp) {
			_gDS[_num_ds++] = tmp;
		}
	}
	for (int i=0; i<(_gConfiguration._alg3_num); ++i) {
		ITest* tmp = CreateDataStructure(_gConfiguration._alg3_name);
		if(null != tmp) {
			_gDS[_num_ds++] = tmp;
		}
	}
	for (int i=0; i<(_gConfiguration._alg4_num); ++i) {
		ITest* tmp = CreateDataStructure(_gConfiguration._alg4_name);
		if(null != tmp) {
			_gDS[_num_ds++] = tmp;
		}
	}

	//calculate how much each thread should add the data-structure initially ...
	final int table_size  = (int)((_gConfiguration._capacity) * (_gConfiguration._load_factor));
	_g_thread_fill_table_size = table_size / _gNumThreads;

	//create benchmark threads .................................................
	System_err_println("    START creating threads.");
	_gThreads               =  new Thread*[_gNumThreads];
	_gThreadResultAry       =  new tick_t[_gNumThreads];
	memset((void*)_gThreadResultAry, 0, sizeof(int)*_gNumThreads);

	final int num_add_threads       = (int) Math::ceil(_gNumThreads * (_gConfiguration._add_ops)/100.0);
	final int num_remove_threads    = (int) Math::floor(_gNumThreads * (_gConfiguration._remove_ops)/100.0);
	final int num_peek_threads      = _gNumThreads - num_add_threads - num_remove_threads;

	if(0 == _gConfiguration._is_dedicated_mode) {
		for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
			_gThreads[iThread] =  new MixThread(iThread);
		}
	} else {
		System_err_println("    num_add_threads:    " + Integer::toString(num_add_threads));
		System_err_println("    num_remove_threads: " + Integer::toString(num_remove_threads));
		System_err_println("    num_peek_threads:   " + Integer::toString(num_peek_threads));

		int curr_thread=0;
		for(int iThread = 0; iThread < num_add_threads; ++iThread) {
			_gThreads[curr_thread] =  new MixThread(curr_thread);
			++curr_thread;
		}
		for(int iThread = 0; iThread < num_remove_threads; ++iThread) {
			_gThreads[curr_thread] =  new MixThread(curr_thread);
			++curr_thread;
		}
		for(int iThread = 0; iThread < num_peek_threads; ++iThread) {
			_gThreads[curr_thread] =  new MixThread(curr_thread);
			++curr_thread;
		}
	}
	System_err_println("    END   creating threads.");
	System_err_println("");
	Thread::yield();

	//start the benchmark threads ..............................................
	System_err_println("    START threads.");
	for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
		_gThreads[iThread]->start();
	}
	System_err_println("    END START  threads.");
	System_err_println("");

	//wait the throughput time, and then signal the threads to terminate ...
	Thread::yield();
	Thread::sleep(_gThroughputTime*1000);
	_gIsStopThreads = 1;

	//join the threads .........................................................
	for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
		Thread::yield();
		Thread::yield();
		_gThreads[iThread]->join();
	}
	System_err_println("    ALL threads terminated.");
	System_err_println("");

	//calculate threads results ................................................
	_gResult = 0;
	_gResultAdd = 0;
	_gResultRemove = 0;
	_gResultPeek = 0;
	if(0 == _gConfiguration._is_dedicated_mode) {
		for(int iThread = 0; iThread < _gNumThreads; ++iThread) {
			_gResult += _gThreadResultAry[iThread];
		}
	} else {
		int curr_thread=0;
		for(int iThread = 0; iThread < num_add_threads; ++iThread) {
			_gResultAdd += _gThreadResultAry[curr_thread];
			++curr_thread;
		}
		for(int iThread = 0; iThread < num_remove_threads; ++iThread) {
			_gResultRemove += _gThreadResultAry[curr_thread];
			++curr_thread;
		}
		for(int iThread = 0; iThread < num_peek_threads; ++iThread) {
			_gResultPeek += _gThreadResultAry[curr_thread];
			++curr_thread;
		}
	}

	//print benchmark results ..................................................
	for (int iDb=0; iDb<_num_ds; ++iDb) {
		System_err_println("    " + std::string(_gDS[iDb]->name()) + " Num elm: " + Integer::toString(_gDS[iDb]->size()));
	}

	//free resources ...........................................................
	delete [] _gRandNumAry;
	delete [] _gThreadResultAry;
	delete [] _gThreads;

	_gRandNumAry = null;
	_gThreadResultAry = null;
	_gThreads = null;

	//return benchmark results ................................................
	_gResult         /= (long)(_gEndTime - _gStartTime);
	_gResultAdd      /= (long)(_gEndTime - _gStartTime);
	_gResultRemove   /= (long)(_gEndTime - _gStartTime);
	_gResultPeek     /= (long)(_gEndTime - _gStartTime);
}

ITest* CreateDataStructure(char* final alg_name) {
	//queue ....................................................................
	if(0 == strcmp(alg_name, "fcqueue")) {
		return (new FCQueue());
	}
	if(0 == strcmp(alg_name, "msqueue")) {
		return (new MSQueue());
	}
	if(0 == strcmp(alg_name, "basketqueue")) {
		return (new BasketsQueue());
	}
	if(0 == strcmp(alg_name, "oyqueue")) {
		return (new OyamaQueue());
	}
	if(0 == strcmp(alg_name, "oyqueuecom")) {
		return (new OyamaQueueCom());
	}
	if(0 == strcmp(alg_name, "ctqueue")) {
		return (new ComTreeQueue());
	}

	//stack ....................................................................
	if(0 == strcmp(alg_name, "lfstack")) {
		return (new LFStack());
	}
	if(0 == strcmp(alg_name, "fcstack")) {
		return (new FCStack());
	}
	if(0 == strcmp(alg_name, "elstack")) {
		return (new EliminationStack());
	}

	//skiplist .................................................................
	if(0 == strcmp(alg_name, "lfskiplist")) {
		return (new LFSkipList());
	}
	if(0 == strcmp(alg_name, "fcpairheap")) {
		return (new FCPairHeap());
	}
	if(0 == strcmp(alg_name, "fcskiplist")) {
		return (new FCSkipList());
	}
	if(0 == strcmp(alg_name, "lazyskiplist")) {
		return (new LazySkipList());
	}

	//..........................................................................
	return null;
}


void PrepareActions() {
	final int add_limit = _gConfiguration._add_ops;
	final int remove_limit = add_limit + _gConfiguration._remove_ops;

	for(int iAction=0; iAction < _ACTIONS_ARY_SIZE; ++iAction) {
		final int rand_num = _gRand.nextInt(1024*1024)%100;

		if(rand_num < 0 || rand_num >= 100) {
			System_err_println("PrepareActions: Error random number" + Integer::toString(rand_num));
			System::exit(1);
		}

		if(rand_num < add_limit)
			_gActionAry[iAction] = 1;
		else if(rand_num < remove_limit)
			_gActionAry[iAction] = 2;
		else
			_gActionAry[iAction] = 3;
	}
}
void PrepareRandomNumbers(final int size) {
	_gRandNumAry = new int[size];
	for (int iRandNum = 0; iRandNum < size; ++iRandNum) {
		if(0 == _gConfiguration._capacity) {
			_gRandNumAry[iRandNum] = iRandNum+2;
		} else { 
			_gRandNumAry[iRandNum] = _gRand.nextInt(_gConfiguration._capacity) + 2;
			if(_gRandNumAry[iRandNum] <= 0 || _gRandNumAry[iRandNum] >= (_gConfiguration._capacity+2)) {
				System_err_println("PrepareRandomNumbers: Error random number" + Integer::toString(_gRandNumAry[iRandNum]));
				System::exit(1);
			}
		}
	}
}

int NearestPowerOfTwo(final int x) {
	int mask = 1;
	while(mask < x) {
		mask <<= 1;
	}
	return mask;
}
