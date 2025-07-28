#include <cstdio>
#include <stdexx.hpp>
#include <thread>
using namespace std::literals;

#if (STDEXX_QTHREADS)
// TODO
auto main() -> int {}
#elif (STDEXX_REFERENCE)
int main() {
  stdexec::run_loop loop;

  std::jthread worker([&](std::stop_token st) {
    std::stop_callback cb{st, [&] { loop.finish(); }};
    loop.run();
  });

  stdexec::sender auto hello = stdexec::just("hello world"s);
  stdexec::sender auto print = std::move(hello) | stdexec::then([](auto msg) {
                                 std::puts(msg.c_str());
                                 return 0;
                               });

  stdexec::scheduler auto io_thread = loop.get_scheduler();
  stdexec::sender auto work = stdexec::starts_on(io_thread, std::move(print));

  auto [result] = stdexec::sync_wait(std::move(work)).value();

  return result;
}

#else
error "Not implemented."
#endif

