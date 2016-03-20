#pragma once

#if defined(BOOSTING) && defined(STM)
#include "Boosting_common.hh"
#include "../../../tl2/stm.h"

#define DO_ABORT() TxAbort(STM_CUR_SELF)
#define POST_COMMIT(callback, context1, context2, context3) TxPostCommitHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))
#define ON_ABORT(callback, context1, context2, context3) TxAbortHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))
#endif
