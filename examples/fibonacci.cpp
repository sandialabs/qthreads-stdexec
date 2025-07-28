#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

aligned_t fib(size_t n) {
  if (n < 2uz) return n;
  stdexec::sender auto s1 =
    stdexx::qthreads_just_sender(n - 1uz) | stdexec::then(&fib);
  stdexec::sender auto s2 =
    stdexx::qthreads_just_sender(n - 2uz) | stdexec::then(&fib);
  auto [r1, r2] =
    stdexec::sync_wait(stdexec::when_all(std::move(s1), std::move(s2))).value();
  return r1 + r2;
}

auto main() -> int {
  stdexx::init();
  auto [r] =
    stdexec::sync_wait(stdexx::qthreads_just_sender(10uz) | stdexec::then(&fib))
      .value();
  std::cout << r << std::endl;
  stdexx::finalize();
}

#elif (STDEXX_REFERENCE)
auto main() -> int {}
#else
error "Not implemented."
#endif