// Copyright 2025 Sandia National Laboratories

#include <cstdio>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)
struct sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(int)>;

  int value_{0};

  /*
    The anatomy of op state: Op state is templated on receiver (callback), has
    start and calls set_value on (rcv, value) synchronously. It uses compatible
    completion signature to provide value to rcv. Calling set_value marks
    completion of sender (the recv is the callback).
  */
  template <class Receiver>
  struct op {
    Receiver rcv;
    int val;

    void start() & noexcept {
      std::printf("success!\n");
      stdexec::set_value(std::move(rcv), val);
    }
  };

  /*
    The anatomy of connect: It connects sender and receiver and returns op
    state templated on the reiver type The initialization of op sets the rcv
    (callback) and the callback is called on completion of op::start.
  */
  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), value_};
  }
};

auto main() -> int {
  /*Sync_wait*/
  auto my_sender = sender{42};
  auto [a] = stdexec::sync_wait(std::move(my_sender)).value();

  /*Via connect*/
  auto op = stdexec::connect(my_sender, empty_recv::expect_value_receiver{42});
  stdexec::start(op);

  return (a == 42) ? 1 : 0;
}

#elif (STDEXX_REFERENCE)
auto main() -> int {}
#elif (QTHREADS)
auto main() -> int {}
#elif (OMP)
auto main() -> int {}
#elif (SERIAL)
auto main() -> int {}
#else
error "Not implemented."
#endif
