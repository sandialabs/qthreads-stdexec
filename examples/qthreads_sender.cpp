#include <cstdio>
#include <stdexx.hpp>

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

auto main() -> int {
  /*Explicit use of custom senders*/
  stdexx::qthreads_context qthreads_context;
  stdexec::sender auto s1 =
    stdexx::qthreads_just_sender(42) | stdexec::then(task1);
  stdexec::sync_wait(std::move(s1));
  stdexec::sender auto s2 =
    stdexx::qthreads_just_sender(42) | stdexec::then(task2);
  stdexec::sync_wait(std::move(s2));
  // stdexec::sync_wait(stdexec::when_all(s1, s2)).value();

  stdexec::sender auto s5 = stdexx::qthreads_basic_func_sender(&task3);
  stdexec::sync_wait(std::move(s5));

  return 0;
}

#elif (STDEXX_REFERENCE)

auto main() -> int {}

#else
error "Not implemented."
#endif
