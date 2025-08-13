#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

using namespace std::chrono;

#define DEFAULT_DEPTH 6uz
#define DEFAULT_THRESHOLD 2uz

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

unsigned int sleep_1sec(size_t n) {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 1uz;
}

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

#elif (STDTHREAD)
//*********************
//*********************
//*********************

#include <future>
#include <thread>

void fib(unsigned int n, std::promise<int> &&p) {
  if (n < 2) {
    p.set_value(n);
    return;
  }

  std::promise<int> p1;
  std::promise<int> p2;
  auto ret1 = p1.get_future();
  auto ret2 = p2.get_future();
  unsigned int n1 = n - 1, n2 = n - 2;

  std::thread t1(fib, n1, std::move(p1));
  t1.join();
  std::thread t2(fib, n2, std::move(p2));
  t2.join();

  p.set_value(ret1.get() + ret2.get());
}

auto main(int argc, char *argv[]) -> int {
  unsigned int depth = DEPTH;
  depth = argc == 2 ? atoi(argv[1]) : depth;
  assert(depth <= 38);

  // timing this
  auto const start{steady_clock::now()};
  std::promise<int> p;
  auto f = p.get_future();
  std::thread thr(fib, depth, std::move(p));
  thr.join();
  unsigned int r = f.get();
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};
  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdthread" << "," << depth << "," << t.count() << "," << 1
            << std::endl;
}

#elif (QTHREADS)
//*********************
//*********************
//*********************

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
//*********************
//*********************
//*********************

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

auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);

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
  std::cout << "stdexec(qthreads)" << "," << depth << "," << t.count() << ","
            << qthread_num_shepherds() * qthread_num_workers() << std::endl;

  stdexx::finalize();
}

#elif (STDEXX_REFERENCE)
//*********************
//*********************
//*********************

#define USE_SCHED_ON_EACH_LEVEL

#include "exec/static_thread_pool.hpp"
constexpr int THREADS = 1;

#if !defined(USE_CTX_ON_EACH_LEVEL)

unsigned int fib_per_thread(size_t n) {
  if (n < threshold) return fib_serial(n);
  if (n < 2uz) return n;
  stdexec::sender auto s1 =
    stdexec::just(n - 1uz) | stdexec::then(&fib_per_thread);
  stdexec::sender auto s2 =
    stdexec::just(n - 2uz) | stdexec::then(&fib_per_thread);
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}

static exec::static_thread_pool ctx{THREADS};

unsigned int fib(size_t n) {
  if (n < threshold) return fib_serial(n);
  stdexec::scheduler auto sched = ctx.get_scheduler();
  // Note: using start_on recursively results in a deadlock
  stdexec::sender auto s1 = stdexec::starts_on(
    sched, stdexec::just(n - 1uz) | stdexec::then(&fib_per_thread));
  stdexec::sender auto s2 = stdexec::starts_on(
    sched, stdexec::just(n - 2uz) | stdexec::then(&fib_per_thread));
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}
#else
// **** using static_thread_pool on each nesting level *****
unsigned int fib(size_t n) {
  exec::static_thread_pool ctx{THREADS};
  using namespace stdexec;
  scheduler auto sched = ctx.get_scheduler();
  if (n < 2uz) return n;
  sender auto s1 = starts_on(sched, just(n - 1uz) | then(fib));
  sender auto s2 = starts_on(sched, just(n - 2uz) | then(fib));
  /*auto [r1, r2] =
    sync_wait(when_all(std::move(s1), std::move(s2))).value();*/
  auto [r1] = sync_wait(when_all(std::move(s1))).value();
  auto [r2] = sync_wait(when_all(std::move(s2))).value();
  return r1 + r2;
}
#endif
auto main(int argc, char *argv[]) -> int {
  auto depth = get_depth_and_set_threshold(argc, argv);
  using namespace stdexec;

  auto const start{steady_clock::now()};
#if !defined(USE_SCHED_ON_EACH_LEVEL)
  auto algorithm = stdexec::just(depth) | stdexec::then(fib));
  auto [r] = sync_wait(algorithm).value();
#else
  std::size_t r = fib(depth);
#endif
  auto const end{steady_clock::now()};

  std::chrono::duration<double> const t{end - start};

  if (r != validation[depth]) {
    std::cout << "Failed." << std::endl;
    exit(1);
  }
  std::cout << "stdexec(reference)" << "," << depth << "," << t.count() << ","
            << THREADS << std::endl;
}

#elif (OMP)
//*********************
//*********************
//*********************
#include <omp.h>

std::size_t fib(std::size_t n) {
  std::size_t i, j;
  if (n < threshold) return fib_serial(n);

#pragma omp task shared(i)
  i = fib(n - 1);

#pragma omp task shared(j)
  j = fib(n - 2);

#pragma omp taskwait
  return i + j;
}

auto main(int argc, char *argv[]) -> int {
  std::size_t r;
  auto depth = get_depth_and_set_threshold(argc, argv);

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
  std::cout << "omp-task" << "," << depth << "," << t.count() << ","
            << omp_get_max_threads() << std::endl;
}
#else
error "Not implemented."
#endif
