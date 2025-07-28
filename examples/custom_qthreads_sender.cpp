#include <cstdio>
#include <stdexx.hpp>

/* One qthread per func with sender-receiver implementing a fork-join */

#if (STDEXX_QTHREADS)
static inline auto task1(int) -> aligned_t {
  std::cout << "Hello from qthread task 1!" << std::endl;
  return 42 + 0;
}

static inline auto task2(int) -> aligned_t {
  std::cout << "Hello from qthread task 2!" << std::endl;
  return 42 + 1;
}

static inline auto task3() -> aligned_t {
  std::cout << "Hello from qthread task 3!" << std::endl;
  return 42 + 2;
}

// Calls qthread_fork()
struct sender_wrapper {
  using sender_concept = stdexec::sender_t;
  qthread_f func;
  aligned_t feb;
  int input;

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t *),
                                   stdexec::set_error_t(std::exception_ptr),
                                   stdexec::set_stopped_t()>;

  template <class Receiver>
  struct op {
    Receiver rcv;
    aligned_t *feb;
    qthread_f func;
    int input;

    void start() & noexcept {
      int r = qthread_fork(func, &input, feb);
      if (r) stdexec::set_error(std::move(rcv), std::exception_ptr());
      stdexec::set_value(std::move(rcv), feb);
    }
  };

  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), &feb, func, input};
  }

  stdexec::env<> get_env() const noexcept { return {}; }
};

// Calls read FF (join)
struct sender_wrapper_receiver {
  using receiver_concept = stdexec::receiver_t;

  void set_value(aligned_t *feb) noexcept { qthread_readFF(NULL, feb); }

  void set_error(std::exception_ptr) noexcept {}

  void set_stopped() noexcept {}
};

// Synchronously fork and join a qthread
struct sender_wrapper_sync {
  using sender_concept = stdexec::sender_t;
  qthread_f func;
  aligned_t feb;

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t *),
                                   stdexec::set_error_t(std::exception_ptr),
                                   stdexec::set_stopped_t()>;

  template <class Receiver>
  struct op {
    Receiver rcv;
    aligned_t *feb;
    qthread_f func;

    void start() & noexcept {
      int r = qthread_fork(func, NULL /*no args*/, feb);
      qthread_readFF(NULL, feb);
      if (r) stdexec::set_error(std::move(rcv), std::exception_ptr());
      stdexec::set_value(std::move(rcv), feb);
    }
  };

  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), &feb, func};
  }

  stdexec::env<> get_env() const noexcept { return {}; }
};

auto main() -> int {
  stexx::init();
  /*Explicit use of custom senders*/
  stdexec::sender auto s1 = then(stdexec::just(42), sender_wrapper{task1, 0});
  stdexec::sender auto s2 = then(stdexec::just(42), sender_wrapper{task2, 0});
  auto val = stdexec::sync_wait(stdexec::when_all(s1, s2)).value();
  // std::cout << std::get<0>(val) << std::endl;

  // Using sender_wrapper and sender_wrapper_receiver
  stdexec::sender auto s5 = sender_wrapper{task3};
  auto op1 = stdexec::connect(s5, sender_wrapper_receiver{});
  stdexec::start(op1);

  // Just run sender_wrapper_sync
  auto s3 = sender_wrapper_sync{task3};
  auto op2 =
    stdexec::connect(s3, empty_recv::expect_value_receiver{(aligned_t)44});
  stdexec::start(op2);
  stexx::finalize();
  return 0;
}

#elif (STDEXX_REFERENCE)
auto main() -> int {}
#elif (QTHREADS)
auto main() -> int {}
#else
error "Not implemented."
#endif
