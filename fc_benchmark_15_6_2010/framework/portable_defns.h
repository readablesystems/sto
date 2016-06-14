#ifndef __PORTABLE_DEFNS_H__
#define __PORTABLE_DEFNS_H__

#if defined(SPARC)
#include "sparc_defns.h"
#elif defined(SPARC64)
#include "sparc64_defns.h"
#elif defined(INTEL)
#include "intel_defns.h"
#elif defined(INTEL64)
#include "intel64_defns.h"
#elif defined(WIN32)
#include "win_defns.h"
#elif defined(WIN64)
#include "win64_defns.h"
#elif defined(PPC)
#include "ppc_defns.h"
#elif defined(IA64)
#include "ia64_defns.h"
#elif defined(MIPS)
#include "mips_defns.h"
#elif defined(ALPHA)
#include "alpha_defns.h"
#else
#error "A valid architecture has not been defined"
#endif

#if defined(WIN32) || defined(WIN64)
	#define __thread__ __declspec(thread)
#else
	#define __thread__ __thread
#endif


#include <time.h>
#include <math.h>
#include <signal.h>
#include <sstream>
#include <iostream>
#include <limits.h>
#include <sys/timeb.h>
#include <stdlib.h>


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef null
#define null (0)
#endif

#define final const
#define boolean bool

#define System_out_println(x) {fprintf(stdout, x); fprintf(stdout, "/n");}
#define System_err_println(x)  std::cerr<<(x)<<std::endl 
#define System_out_format printf

#endif /* __PORTABLE_DEFNS_H__ */
