#include <cstdio>
#include <stdexx.hpp>

/* One qthread per func with sender-receiver implementing a fork-join */

#if (STDEXX_QTHREADS)
static auto task(void *) -> aligned_t {
  std::cout << "Hello from qthread task!" << std::endl;
  return 42;
}

/*
static auto task_inline(aligned_t *) -> aligned_t {
  std::cout << "Hello from task inline to caller!" << std::endl;
  return (aligned_t)42;
}*/

// Calls qthread_fork()
struct sender {
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
struct receiver {
  using receiver_concept = stdexec::receiver_t;

  void set_value(aligned_t *feb) noexcept { qthread_readFF(NULL, feb); }

  void set_error(std::exception_ptr) noexcept {}

  void set_stopped() noexcept {}
};

// Synchronously fork and join a qthread
struct sender_forkjoin {
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
      stdexec::set_value(std::move(rcv), *feb);
    }
  };

  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), &feb, func};
  }

  stdexec::env<> get_env() const noexcept { return {}; }
};

auto main() -> int {
  stdexx::init();

  // Example 1
  stdexec::sender auto my_qthreads_sender = sender{task, 0};
  stdexec::receiver auto my_qthreads_sender_receiver = receiver{};
  auto op1 = stdexec::connect(my_qthreads_sender, my_qthreads_sender_receiver);
  stdexec::start(op1);

  // Example 2
  auto sender_fj = sender_forkjoin{task};
  auto op = stdexec::connect(sender_fj,
                             empty_recv::expect_value_receiver{(aligned_t)42});
  stdexec::start(op);

  // Example 3
  // Custom then to implement sync
  stdexec::sender auto s1 = stdexx::then(stdexec::just(42), sender{task, 0});
  stdexec::sender auto s2 = stdexx::then(stdexec::just(42), sender{task, 0});
  auto [v1, v2] = stdexec::sync_wait(stdexec::when_all(s1, s2)).value();
  std::cout << *v1 + *v2 << std::endl;

  // Example 4 //Should this work?
  // Using custom sender and receiver to fork-off work and joining threads via
  // my_qthreads_sender_receiver
  /*stdexec::sender auto work = stdexec::then(my_qthreads_sender, task_inline);
  auto op2 = stdexec::connect(work, my_qthreads_sender_receiver);
  stdexec::start(op2);
  */

  stdexx::finalize();
  return 0;
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
