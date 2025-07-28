#define once

#if (STDEXX_QTHREADS)
// ULT backend
#include <qthreads/algorithms.hpp>
#include <qthreads/stdexec.hpp>
#elif (STDEXX_REFERENCE)
// stdexec backend
#include <reference/algorithms.hpp>
#include <stdexec/execution.hpp>
#else
error "Not implemented."
#endif
