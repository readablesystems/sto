#include "cpp_framework.h"

//------------------------------------------------------------------------------
// START
// File    : cpp_framework.h
// Author  : Ms.Moran Tzafrir;  email: morantza@gmail.com
// Written : 13 April 2009
// 
// Multi-Platform C++ framework
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

const int CCP::Integer::MIN_VALUE = INT_MIN;
const int CCP::Integer::MAX_VALUE = INT_MAX;
const int CCP::Integer::SIZE = 32;

__thread__	CCP::Thread* CCP::_g_tls_current_thread = null;

//------------------------------------------------------------------------------
// END
// File    : cpp_framework.h
// Author  : Ms.Moran Tzafrir  email: morantza@gmail.com
// Written : 13 April 2009
// 
// Multi-Platform C++ framework
//
// Copyright (C) 2009 Moran Tzafrir.
// You can use this file only by explicit written approval from Moran Tzafrir.
//------------------------------------------------------------------------------

