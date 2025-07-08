#ifndef QTHREADS_STDEXEC_COMMON_HPP
#define QTHREADS_STDEXEC_COMMON_HPP

// General C++ utilities needed by the qthreads stdexec wrapper code.

namespace stdexx {

struct immovable {
  immovable() = default;
  immovable(immovable &&) = delete;
  immovable(immovable const &) = delete;
  immovable &operator=(immovable &&) = delete;
  immovable &operator=(immovable const &) = delete;
};

template <typename T>
struct is_index_sequence_impl {
  static constexpr bool val = false;
};

template <std::size_t... I>
struct is_index_sequence_impl<std::index_sequence<I...>> {
  static constexpr bool val = true;
};

template <typename T>
concept is_index_sequence = is_index_sequence_impl<T>::val;

// helper for apply_across
template <std::size_t I, typename Func, typename... Args>
decltype(auto) apply_at_index(Func &&func, Args &&...args) {
  return static_cast<Func &&>(func)(std::get<I>(static_cast<Args &&>(args))...);
}

// helper for apply_across
template <std::size_t... I, typename Func, typename... Args>
decltype(auto)
apply_across_impl(std::index_sequence<I...>, Func &&func, Args &&...args) {
  // deliberately decaying to (possibly const) lvalue references by
  // not forwarding since the function call potentially happens many times.
  return std::make_tuple(apply_at_index<I>(func, args...)...);
}

// apply_across is like std::apply, but applies an operation
// taking its arguments from each element of multiple tuples.
// TODO: better argument type checking to make sure we're actually
// getting tuple_like objects.
// TODO: forward noexcept specifier.
template <typename Func, typename... Args>
decltype(auto) apply_across(Func &&func, Args &&...args) {
  static_assert(sizeof...(Args),
                "Ambiguous return size for tuple. At least one argument to map "
                "onto is required.");
  constexpr std::size_t tuple_size = std::tuple_size_v<Args...[0]>;
  static_assert(((tuple_size == std::tuple_size_v<Args>) && ...),
                "Tuple-likes to be mapped onto must all be the same size.");
  return apply_across_impl(std::make_index_sequence<tuple_size>(),
                           static_cast<Func &&>(func),
                           static_cast<Args &&>(args)...);
}

// compile-time unit test for apply_across.
static_assert(
  std::is_same_v<decltype(apply_across(
                   [](size_t a, size_t b) { return a + b; },
                   std::declval<std::tuple<std::size_t, std::size_t>>(),
                   std::declval<std::tuple<std::size_t, std::size_t>>())),
                 std::tuple<std::size_t, std::size_t>>);

} // namespace stdexx

#endif // #ifndef QTHREADS_STDEXEC_COMMON_HPP
