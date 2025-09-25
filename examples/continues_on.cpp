#include <iostream>

#include <stdexx.hpp>

int main() {
  ///*
  auto [n] =
    stdexec::sync_wait(stdexec::just(0uz) |
                       stdexec::then([](auto val) { return val + 1uz; }) |
                       stdexec::continues_on(stdexx::qthreads_scheduler{}) |
                       stdexec::then([](auto val) { return val + 2uz; }))
      .value();
  //*/
  /*
  auto [n] =
    stdexec::sync_wait(stdexx::qthreads_just_sender(0uz) |
                       stdexec::then([](auto val) { return val + 1uz; }) |
                       stdexec::continues_on(stdexx::qthreads_scheduler{}) |
                       stdexec::then([](auto val) { return val + 2uz; }))
      .value();
  */
}
