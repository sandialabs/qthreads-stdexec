#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

using namespace std::chrono;

#define DEPTH 10uz

static unsigned int validation[] = {
  0,        // 0
  1,        // 1
  1,        // 2
  2,        // 3
  3,        // 4
  5,        // 5
  8,        // 6
  13,       // 7
  21,       // 8
  34,       // 9
  55,       // 10
  89,       // 11
  144,      // 12
  233,      // 13
  377,      // 14
  610,      // 15
  987,      // 16
  1597,     // 17
  2584,     // 18
  4181,     // 19
  6765,     // 20
  10946,    // 21
  17711,    // 22
  28657,    // 23
  46368,    // 24
  75025,    // 25
  121393,   // 26
  196418,   // 27
  317811,   // 28
  514229,   // 29
  832040,   // 30
  1346269,  // 31
  2178309,  // 32
  3524578,  // 33
  5702887,  // 34
  9227465,  // 35
  14930352, // 36
  24157817, // 37
  39088169  // 38
};

static_assert(DEPTH <= 38 && DEPTH >= 0, "Select valid DEPTH.");

#if (SERIAL)
unsigned int fib(unsigned int n) {
  if (n < 2) { return n; }
  return fib(n - 1) + fib(n - 2);
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  // timing this
  auto const start{steady_clock::now()};
  unsigned int r = fib(depth);
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "serial" << "," << depth << "," << t.count() << "," << 1 << std::endl;
}

#elif (QTHREADS)

static aligned_t fib(void *arg_) {
  unsigned int n = *(unsigned int *)arg_;

  if (n < 2) { return n; }

  aligned_t ret1 = 0;
  aligned_t ret2 = 0;
  unsigned int n1 = n - 1;
  unsigned int n2 = n - 2;

  qthread_fork(fib, &n1, &ret1);
  qthread_fork(fib, &n2, &ret2);

  qthread_readFF(NULL, &ret1);
  qthread_readFF(NULL, &ret2);

  return ret1 + ret2;
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  qthread_initialize();

  // timing this
  auto const start{steady_clock::now()};
  unsigned int r = fib(static_cast<void *>(&depth));
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "qthreads" << "," << depth << "," << t.count()  << "," << qthread_num_shepherds() * qthread_num_workers() << std::endl;

  qthread_finalize();
}

#elif (STDEXX_QTHREADS)

#define CUT_OFF  0

aligned_t fib(size_t n) {
  if (n < 2uz) return n;

  if(n < CUT_OFF)
    return fib(n - 1) + fib(n - 2);
  else
  {
  stdexec::sender auto s1 =
    stdexx::qthreads_just_sender(n - 1uz) | stdexec::then(&fib);
  stdexec::sender auto s2 =
    stdexx::qthreads_just_sender(n - 2uz) | stdexec::then(&fib);
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
  }
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  stdexx::init();

  auto const start{steady_clock::now()};
  auto [r] = stdexec::sync_wait(stdexx::qthreads_just_sender(depth) |
                                stdexec::then(&fib))
               .value();
  auto const end{steady_clock::now()};
  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexx" << "," << depth << "," << t.count() << "," << qthread_num_shepherds() * qthread_num_workers() << std::endl;

  stdexx::finalize();
}

#elif (STDEXX_REFERENCE)

#if 0
unsigned int fib(size_t n) {
  if (n < 2uz) return n;
  stdexec::sender auto s1 = stdexec::just(n - 1uz) | stdexec::then(&fib);
  stdexec::sender auto s2 = stdexec::just(n - 2uz) | stdexec::then(&fib);
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  auto const start{steady_clock::now()};

  auto [r] =
    stdexec::sync_wait(stdexec::just(depth) | stdexec::then(&fib)).value();
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};

  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexec" << "," << depth << "," << t.count() << std::endl;
}

#endif

#include "exec/static_thread_pool.hpp"

unsigned int fib(size_t n) {
  if (n < 2uz) return n;
  stdexec::sender auto s1 = stdexec::just(n - 1uz) | stdexec::then(&fib);
  stdexec::sender auto s2 = stdexec::just(n - 2uz) | stdexec::then(&fib);
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}

auto main(int argc, char *argv[]) -> int {
  using namespace stdexec;
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  unsigned int THREADS = 1;

  exec::static_thread_pool ctx{THREADS};
  scheduler auto sched = ctx.get_scheduler();

  auto const start{steady_clock::now()};
  auto algorithm =
    stdexec::starts_on(sched, stdexec::just(depth) | stdexec::then(fib));

  auto [r] = sync_wait(algorithm).value();
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};

  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexec" << "," << depth << "," << t.count() << "," << THREADS << std::endl;
}

#elif (OMP)
#include <omp.h>

unsigned int fib(unsigned int n) {
  int i, j;
  if (n < 2) { return n; }

#pragma omp task shared(i)
  i = fib(n - 1);

#pragma omp task shared(j)
  j = fib(n - 2);

#pragma omp taskwait
  return i + j;
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  unsigned int r;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  // timing this
  auto const start{steady_clock::now()};
#pragma omp parallel shared(r)
#pragma omp single
  r = fib(depth);
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "omp-task" << "," << depth << "," << t.count() << "," << omp_get_max_threads() << std::endl;
}
#else
error "Not implemented."
#endif
