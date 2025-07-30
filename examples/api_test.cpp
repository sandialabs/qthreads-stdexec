#include <cstdlib>
#include <iostream>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

struct example_1 {
  example_1() {
    stdexec::sender auto my_sender =
      stdexec::schedule(stdexx::qthreads_scheduler{});
    stdexec::sync_wait(std::move(my_sender));
  }
};

struct example_2 {
  example_2() {
    stdexec::sender auto my_sender =
      stdexec::schedule(stdexx::qthreads_scheduler{});
    stdexec::sender auto algorithm = stdexec::then(std::move(my_sender), []() {
      std::puts("Hello from user-level thread in then-lambda");
      return 1;
    });
    auto val = stdexec::sync_wait(std::move(algorithm)).value();
    std::cout << std::get<0>(val) << std::endl;
  }
};

struct example_3 {
  example_3() {
    stdexec::sender auto begin =
      stdexec::schedule(stdexx::qthreads_scheduler{});
    stdexec::sender auto hi_again = stdexec::then(std::move(begin), []() {
      std::cout << "Hello world! Have an int.\n";
      return 13;
    });
    auto val = stdexec::sync_wait(std::move(hi_again)).value();
    std::cout << std::get<0>(val) << std::endl;
  }
};

struct example_4 {
  example_4() {
    stdexec::sender auto then =
      stdexec::schedule(stdexx::qthreads_scheduler{}) | stdexec::then([]() {
        std::cout << "hello from then returning a value" << std::endl;
        return (aligned_t)1;
      });
    auto val = stdexec::sync_wait(std::move(then)).value();
    std::cout << "value returned from then: " << std::get<0>(val) << std::endl;
  }
};

struct example_5 {
  static aligned_t test_func(aligned_t val) noexcept {
    std::cout << "hello, passed value is: " << val << std::endl;
    return val;
  }

  example_5() {
    auto val1 =
      stdexec::sync_wait(
        stdexx::qthreads_func_sender<decltype(&example_5::test_func),
                                     aligned_t>(&example_5::test_func, 4ull))
        .value();
    stdexx::qthreads_func_sender<decltype(&example_5::test_func), aligned_t>
      snd2(&example_5::test_func, 4ull);
    auto val2 = stdexec::sync_wait(std::move(snd2)).value();
    std::cout << "values from qthreads func sender: " << std::get<0>(val1)
              << ", " << std::get<0>(val2) << std::endl;
  }
};

struct example_6 {
  example_6() {
    stdexec::sync_wait(
      stdexx::qthreads_func_sender([](int val) { return val; }, 42) |
      stdexec::then([](auto val) {
        std::cout << "then after func sender: " << val << std::endl;
      }));
  }
};

struct example_7 {
  example_7() {
    stdexec::sync_wait(
      stdexx::qthreads_just_sender(42) | stdexec::then([](auto val) {
        std::cout << "then after just sender: " << val << std::endl;
      }));
  }
};

struct example_8 {
  example_8() {
    stdexec::sync_wait(stdexx::qthreads_basic_func_sender([]() { return 42; }) |
                       stdexec::then([](auto val) {
                         std::cout << "then after basic func sender: " << val
                                   << std::endl;
                       }));
  }
};

/*
struct example_9 {
  example_9() {
    auto val =
      stdexec::sync_wait(stdexec::when_all(stdexx::qthreads_just_sender(1),
                                           stdexx::qthreads_just_sender(2)))
        .value();
    std::cout << "values from inside when_all: " << std::get<0>(val) << " "
              << std::get<1>(val) << std::endl;
  }
};*/

/*struct example_10 {
  example_10() {
    stdexec::sender auto s =
      stdexec::schedule(stdexx::qthreads_scheduler{}) |
      stdexec::then([](int arg) {
        std::puts("Hello from user-level thread in then-lambda");
      }) |
      stdexec::bulk(stdexec::par, 20, [](int i) {
        std::cout << "Hello from user-level thread in bulk!(i=" << i << ")"
                  << std::endl;
      });
    stdexec::sync_wait(std::move(s));
  }
};*/

/*
struct example_11 {
  example_11() {
    auto snd = stdexec::schedule(thread_pool.scheduler()) |
               stdexec::then([] { return 123; }) |
               stdexec::continues_on(stdexx::qthreads_scheduler{}) |
               stdexec::then([](int i) { return 123 * 5; }) |
               stdexec::continues_on(thread_pool.scheduler()) |
               stdexec::then([](int i) { return i - 5; });
    auto [result] = stdexec::sync_wait(snd).value();
  } // result == 610
};*/

auto main() -> int {
  stdexx::init();
  using types = std::variant<example_1, example_2
                             // example_3,
                             // example_4,
                             // example_5,
                             // example_6,
                             // example_7,
                             // example_8,
                             // example_9,
                             // example_10,
                             // example_11,
                             >;
  std::vector<types> v = {{}, {}, {}, {}, {}};
  stdexx::finalize();
  return 0;
}

#elif (STDEXX_REFERENCE)

#include <exec/static_thread_pool.hpp>

auto main() -> int {
  using namespace stdexec;
  exec::static_thread_pool ctx{8};
  scheduler auto sch = ctx.get_scheduler();

  sender auto begin = schedule(sch);
  sender auto hi_again = then(begin, [] {
    std::cout << "Hello world! Have an int.\n";
    return 13;
  });

  sender auto add_42 = then(hi_again, [](int arg) { return arg + 42; });
  auto [i] = sync_wait(std::move(add_42)).value();
  std::cout << "Result: " << i << std::endl;

  // Sync_wait provides a run_loop scheduler
  std::tuple<run_loop::__scheduler> t = sync_wait(get_scheduler()).value();
  (void)t;

  auto y = let_value(get_scheduler(), [](auto sched) {
    return starts_on(sched, then(just(), [] {
                       std::cout << "from run_loop\n";
                       return 42;
                     }));
  });
  sync_wait(std::move(y));
  sync_wait(when_all(just(42), get_scheduler(), get_stop_token()));
  return 0;
}
#elif (QTHREADS)
auto main() -> int {}
#elif (SERIAL)
auto main() -> int {}
#elif (OMP)
auto main() -> int {}
#else
error "Not implemented."
#endif
