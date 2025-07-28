#include <exec/static_thread_pool.hpp>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)
// TODO
auto main() -> int {}
#elif (STDEXX_REFERENCE)
int main() {
  // Declare a pool of 3 worker threads:
  exec::static_thread_pool pool(3);

  auto sched = pool.get_scheduler();

  auto fun = [](int i) { return i * i; };
  auto work = stdexec::when_all(
    stdexec::starts_on(sched, stdexec::just(0) | stdexec::then(fun)),
    stdexec::starts_on(sched, stdexec::just(1) | stdexec::then(fun)),
    stdexec::starts_on(sched, stdexec::just(2) | stdexec::then(fun)));

  // Launch the work and wait for the result
  auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

  // Print the results:
  std::printf("%d %d %d\n", i, j, k);
}
#else
error "Not implemented."
#endif
