#define once

#if (STDEXX_QTHREADS)
// ULT backend
#include <qthreads/algorithms.hpp>
#include <qthreads/stdexec.hpp>
#elif (STDEXX_REFERENCE)
// stdexec backend
#include <reference/algorithms.hpp>
#include <stdexec/execution.hpp>
#elif (QTHREADS)
#include <qthread/qthread.h>
#elif (SERIAL)
#elif (STDTHREAD)
#elif (OMP)
#else
error "Not implemented."
#endif
