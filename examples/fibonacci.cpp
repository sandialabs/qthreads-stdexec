#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

using namespace std::chrono;

#define DEFAULT_DEPTH 6uz
#define DEFAULT_THRESHOLD 6uz

static std::size_t validation[] = {
  0uz,        // 0
  1uz,        // 1
  1uz,        // 2
  2uz,        // 3
  3uz,        // 4
  5uz,        // 5
  8uz,        // 6
  13uz,       // 7
  21uz,       // 8
  34uz,       // 9
  55uz,       // 10
  89uz,       // 11
  144uz,      // 12
  233uz,      // 13
  377uz,      // 14
  610uz,      // 15
  987uz,      // 16
  1597uz,     // 17
  2584uz,     // 18
  4181uz,     // 19
  6765uz,     // 20
  10946uz,    // 21
  17711uz,    // 22
  28657uz,    // 23
  46368uz,    // 24
  75025uz,    // 25
  121393uz,   // 26
  196418uz,   // 27
  317811uz,   // 28
  514229uz,   // 29
  832040uz,   // 30
  1346269uz,  // 31
  2178309uz,  // 32
  3524578uz,  // 33
  5702887uz,  // 34
  9227465uz,  // 35
  14930352uz, // 36
  24157817uz, // 37
  39088169uz  // 38
};

static_assert(DEFAULT_DEPTH <= 38uz, "Select valid DEPTH.");

std::size_t fib_serial(std::size_t n) {
  if (n < 2uz) { return n; }
  return fib_serial(n - 1uz) + fib_serial(n - 2uz);
}

#ifndef SERIAL
static size_t threshold = DEFAULT_THRESHOLD;
#endif

auto get_depth_and_set_threshold(int argc, char *argv[]) {
  std::size_t depth = DEFAULT_DEPTH;
  assert(argc <= 3);
  depth = argc > 1 ? atoi(argv[1]) : depth;
  assert(depth <= 38);
#ifndef SERIAL
  threshold = argc > 2 ? atoi(argv[2]) : threshold;
  assert(threshold >= 2uz);
#endif
  return depth;
}

#if (SERIAL)

auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);

  // timing this
  auto const start{steady_clock::now()};
  std::size_t r = fib_serial(depth);
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "serial" << "," << depth << "," << t.count() << "," << 1
            << std::endl;
}

#elif (QTHREADS)

static aligned_t fib(void *arg_) {
  auto n = *(std::size_t *)arg_;

  if (n < threshold) return fib_serial(n);

  aligned_t ret1 = 0uz;
  aligned_t ret2 = 0uz;
  std::size_t n1 = n - 1uz;
  std::size_t n2 = n - 2uz;

  qthread_fork(fib, &n1, &ret1);
  qthread_fork(fib, &n2, &ret2);

  qthread_readFF(NULL, &ret1);
  qthread_readFF(NULL, &ret2);

  return ret1 + ret2;
}

auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);

  qthread_initialize();

  // timing this
  auto const start{steady_clock::now()};
  aligned_t r = fib(static_cast<void *>(&depth));
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "qthreads" << "," << depth << "," << t.count() << ","
            << qthread_num_shepherds() * qthread_num_workers() << std::endl;

  qthread_finalize();
}

#elif (STDEXX_QTHREADS)

std::size_t fib(std::size_t n) {
  if (n < threshold) return fib_serial(n);
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
  auto depth = get_depth_and_set_threshold(argc, argv);

  stdexx::init();

  auto const start{steady_clock::now()};
  auto r = fib(depth);
  auto const end{steady_clock::now()};
  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexx" << "," << depth << "," << t.count() << ","
            << qthread_num_shepherds() * qthread_num_workers() << std::endl;

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

#ifndef NUM_THREADS
#define NUM_THREADS 8
#endif

static exec::static_thread_pool ctx{8};

unsigned int fib(size_t n) {
  if (n < threshold) return fib_serial(n);
  stdexec::scheduler auto sched = ctx.get_scheduler();
  stdexec::sender auto s1 =
    stdexec::starts_on(sched, stdexec::just(n - 1uz) | stdexec::then(&fib));
  stdexec::sender auto s2 =
    stdexec::starts_on(sched, stdexec::just(n - 2uz) | stdexec::then(&fib));
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}

auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);
  using namespace stdexec;

  exec::static_thread_pool ctx{8};

  auto const start{steady_clock::now()};
  auto r = fib(depth);
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};

  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexec" << "," << depth << "," << t.count() << ","
            << NUM_THREADS << std::endl;
}

#elif (OMP)
#include <omp.h>

std::size_t fib(std::size_t n) {
  std::size_t i, j;
  if (n < threshold) return fib_serial(n);

#pragma omp parallel
#pragma omp task shared(i)
  i = fib(n - 1);

#pragma omp task shared(j)
  j = fib(n - 2);

#pragma omp taskwait
  return i + j;
}

auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);

  // timing this
  auto const start{steady_clock::now()};

  auto r = fib(depth);
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "omp-task" << "," << depth << "," << t.count() << ","
            << omp_get_max_threads() << std::endl;
}
#else
error "Not implemented."
#endif
