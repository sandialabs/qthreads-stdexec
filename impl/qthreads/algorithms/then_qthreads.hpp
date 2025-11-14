// Copyright 2025 Sandia National Laboratories

#define once

#include <iostream>
#include <memory>
#include <stdexec/execution.hpp>
#include <stdio.h>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

namespace stdexx {

// We do not need this as the user should
// call runtime initialization and finalization
// in ULT/stdexec scenarios.

int context_init() { return qthread_initialize(); }

void context_finalize() { qthread_finalize(); }

struct qthreads_context {
  struct qthreads {};

  std::shared_ptr<qthreads> ctx = std::make_shared<qthreads>();

  qthreads_context() { context_init(); }

  ~qthreads_context() {
    if (ctx.use_count() == 1) context_finalize();
  }

  qthreads_context(qthreads_context const &other): ctx(other.ctx) {}

  qthreads_context &operator=(qthreads_context const &&other) { return *this; }
};

template <stdexec::receiver Receiver, stdexec::sender Work>
struct on_qthreads_receiver : Receiver {
  Receiver receiver_;
  Work work_;
  using receiver_concept = stdexec::receiver_t;

  void set_value(int i) noexcept {
    std::cout << "work_and_done" << std::endl;

    stdexec::sender auto and_done = stdexec::then(work_, [](aligned_t *feb) {
      qthread_readFF(NULL, feb);
      std::cout << "FEB:" << feb << std::endl;
    });
    auto op = stdexec::connect(and_done, std::move(receiver_));
    stdexec::start(op);
  }

  void set_error(std::exception_ptr e) noexcept {
    stdexec::set_error(std::move(receiver_), e);
  }

  void set_stopped() noexcept { stdexec::set_stopped(std::move(receiver_)); }
};

template <stdexec::sender Previous, stdexec::sender Work>
struct on_qthreads_sender {
  Previous previous_;
  Work work_;
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t *),
                                   stdexec::set_error_t(std::exception_ptr),
                                   stdexec::set_stopped_t()>;

  stdexec::env<> get_env() const noexcept { return {}; }

  template <stdexec::receiver Receiver>
  auto connect(Receiver receiver)
    && -> stdexec::connect_result_t<Previous,
                                    on_qthreads_receiver<Receiver, Work>> {
    return stdexec::connect(previous_, on_qthreads_receiver{receiver, work_});
  }
};

// This is motivated by the stdexec::then algorithm but takes two
// senders instead of invocable so we can use a custom qthreads sender
template <stdexec::sender Previous, stdexec::sender Work>
auto then(Previous prev, Work work) -> on_qthreads_sender<Previous, Work> {
  return {prev, work};
}

} // namespace stdexx
