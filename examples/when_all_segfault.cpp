#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)
// This was a reproducing example for a memory corruption bug
// that was happening earlier. We've worked around it now, but
// the fix isn't particularly enlightening. I'm leaving this
// here as a test that can be used for further investigation.

int main() {
  stdexx::init();
  auto [r] = stdexec::sync_wait(
               stdexec::when_all(stdexx::qthreads_just_sender(1uz) |
                                   stdexec::then([](auto a) { return a; }),
                                 stdexx::qthreads_just_sender(2uz)) |
               stdexec::then([](auto a, auto b) { return a + b; }))
               .value();
  std::cout << r << std::endl;
  stdexx::finalize();
}
#elif (STDEXX_REFERENCE)
auto main() -> int {}
#else
error "Not implemented."
#endif
