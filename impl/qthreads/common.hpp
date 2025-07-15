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

template <typename T>
struct decay_rvalue_impl {
  using type = T;
};

template <typename T>
struct decay_rvalue_impl<T&&> {
  using type = T;
};

template <typename T>
using decay_rvalue = decay_rvalue_impl<T>::type;

// flatten_tuples takes a pack of tuple types and concatenates them
// into a single tuple type.
// Related, flatten_tuples_starts and
// flatten_tuples_stops provide index sequences that
// mark the beginning and (non-inclusive) end of the indices in
// the final tuple provided by each tuple type of the input (Like CSR).
// TODO: verify that the types are actually tuples.
template <typename... Ts>
struct flatten_tuples_impl;

// Trivial input case
template <>
struct flatten_tuples_impl<std::index_sequence<>,
                           std::index_sequence<>,
                           std::tuple<>,
                           std::tuple<>> {
  using type = std::tuple<>;
  using starts = std::index_sequence<>;
  using stops = std::index_sequence<>;
};

// Recursion base case
template <std::size_t... Starts,
          std::size_t... Ends,
          typename... Processed,
          typename... Next>
struct flatten_tuples_impl<std::index_sequence<Starts...>,
                           std::index_sequence<Ends...>,
                           std::tuple<Processed...>,
                           std::tuple<Next...>> {
  using type = std::tuple<Processed..., Next...>;
  using starts = std::index_sequence<Starts..., sizeof...(Processed)>;
  using stops =
    std::index_sequence<Ends..., sizeof...(Processed) + sizeof...(Next)>;
};

// General recursive case
template <std::size_t... Starts,
          std::size_t... Ends,
          typename... Processed,
          typename... Next,
          typename... Others>
struct flatten_tuples_impl<std::index_sequence<Starts...>,
                           std::index_sequence<Ends...>,
                           std::tuple<Processed...>,
                           std::tuple<Next...>,
                           Others...> {
  using recurse = flatten_tuples_impl<
    std::index_sequence<Starts..., sizeof...(Processed)>,
    std::index_sequence<Ends..., sizeof...(Processed) + sizeof...(Next)>,
    std::tuple<Processed..., Next...>,
    Others...>;
  using type = recurse::type;
  using starts = recurse::starts;
  using stops = recurse::stops;
};

template <typename... Ts>
using flatten_tuples = flatten_tuples_impl<std::index_sequence<>,
                                           std::index_sequence<>,
                                           std::tuple<>,
                                           Ts...>::type;

template <typename... Ts>
using flatten_tuples_starts = flatten_tuples_impl<std::index_sequence<>,
                                                  std::index_sequence<>,
                                                  std::tuple<>,
                                                  Ts...>::starts;

template <typename... Ts>
using flatten_tuples_stops = flatten_tuples_impl<std::index_sequence<>,
                                                 std::index_sequence<>,
                                                 std::tuple<>,
                                                 Ts...>::stops;

// compile-time unit test for flatten_tuples functionality.
struct test_flatten_tuples {
  using t0 = std::tuple<>;
  using t1 = std::tuple<int, int, int>;
  using t2 = std::tuple<>;
  using t3 = std::tuple<float>;
  using flattened = flatten_tuples<t0, t1, t2, t3>;
  using starts = flatten_tuples_starts<t0, t1, t2, t3>;
  using stops = flatten_tuples_stops<t0, t1, t2, t3>;
  static_assert(std::is_same_v<flattened, std::tuple<int, int, int, float>>);
  static_assert(
    std::is_same_v<starts, std::index_sequence<0uz, 0uz, 3uz, 3uz>>);
  static_assert(std::is_same_v<stops, std::index_sequence<0uz, 3uz, 3uz, 4uz>>);
};

template <typename T>
struct flatten_nested_tuples_impl;

template <typename... Ts>
struct flatten_nested_tuples_impl<std::tuple<Ts...>> {
  using type = flatten_tuples<std::decay_t<Ts>...>;
  using starts = flatten_tuples_starts<std::decay_t<Ts>...>;
  using stops = flatten_tuples_stops<std::decay_t<Ts>...>;
};

template <typename T>
using flatten_nested_tuples = flatten_nested_tuples_impl<T>::type;
template <typename T>
using flatten_nested_tuples_starts = flatten_nested_tuples_impl<T>::starts;
template <typename T>
using flatten_nested_tuples_stops = flatten_nested_tuples_impl<T>::stops;

// check_matches_range: a utility function for checking
// that a pack of types match a range of types in a tuple.
template <typename T, std::size_t start, std::size_t end, typename... Args>
struct check_matches_range;

template <typename... T, std::size_t start>
struct check_matches_range<std::tuple<T...>, start, start> {};

template <typename... T,
          std::size_t start,
          std::size_t end,
          typename Arg,
          typename... Args>
struct check_matches_range<std::tuple<T...>, start, end, Arg, Args...> {
  static_assert(end - start == sizeof...(Args) + 1uz);
  static_assert(std::is_same_v<T...[start], Arg>);
  using recurse =
    check_matches_range<std::tuple<T...>, start + 1uz, end, Args...>;
};

using test_check_matches_range =
  check_matches_range<std::tuple<double, int, double, int>,
                      1uz,
                      4uz,
                      int,
                      double,
                      int>;

// assign_to_range assigns a pack of values to a
// range of values within a given tuple.
template <std::size_t Offset, typename Indices>
struct assign_to_range_impl;

template <std::size_t Offset, std::size_t... Ix>
struct assign_to_range_impl<Offset, std::index_sequence<Ix...>> {
  template <typename... Ts, typename... As>
  void operator()(std::tuple<Ts...> &t, As &&...as) const noexcept {
    static_assert(sizeof...(Ix) == sizeof...(As));
    static_assert(Offset + sizeof...(Ix) <= sizeof...(Ts));
    // Fold expression on comma operator
    // to evaluate expression for each index in the pack.
    ((std::get<Offset + Ix>(t) = std::forward<As...[Ix]>(as...[Ix])), ...);
  }
};

template <std::size_t start, std::size_t stop>
assign_to_range_impl<start, std::make_index_sequence<stop - start>>
  assign_to_range;

// apply_at_indices:
// build a tuple type by applying a template
// only at given indices of a parameter pack of types.
template <template <typename> typename Op, typename Indices, typename... Ts>
struct apply_at_indices_impl;

template <template <typename> typename Op, std::size_t... Ix, typename... Ts>
struct apply_at_indices_impl<Op, std::index_sequence<Ix...>, Ts...> {
  // TODO: assert the max of the index sequence is less than sizeof...(Ts)
  using type = std::tuple<Op<Ts...[Ix]>...>;
};

template <template <typename> typename Op, typename Indices, typename... Ts>
using apply_at_indices = apply_at_indices_impl<Op, Indices, Ts...>::type;

template <typename T, std::size_t Ix>
struct get_at_index_impl;

template <std::size_t Ix, std::size_t... Ixs>
struct get_at_index_impl<std::index_sequence<Ixs...>, Ix> {
  static constexpr std::size_t value = Ixs...[Ix];
};

template <typename T, std::size_t Ix>
static constexpr std::size_t get_at_index = get_at_index_impl<T, Ix>::value;

// consuming_apply: an equivalent of std::apply that moves from
// each of the tuple elements.
template <typename F, typename T, std::size_t... Ix>
decltype(auto)
consuming_apply_impl(F &&f, T &&t, std::index_sequence<Ix...>) noexcept {
  return static_cast<F &&>(f)(std::move(std::get<Ix>(t))...);
}

template <typename F, typename T>
decltype(auto) consuming_apply(F &&f, T &&t) noexcept {
  return consuming_apply_impl(
    static_cast<F &&>(f),
    std::move(t),
    std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>());
}

} // namespace stdexx

#endif // #ifndef QTHREADS_STDEXEC_COMMON_HPP
