#include <iostream>

#include <stdexx.hpp>

int main() {
  stdexx::init();
  // TODO: there's a memory corruption bug here. Fix it.
  auto [r] = stdexec::sync_wait(
               stdexec::when_all(stdexx::qthreads_just_sender(1uz) |
                                   stdexec::then([](auto a) { return a; }),
                                 stdexx::qthreads_just_sender(2uz)) |
               stdexec::then([](auto a, auto b) { return a + b; }))
               .value();
  std::cout << r << std::endl;
  stdexx::finalize();
}
