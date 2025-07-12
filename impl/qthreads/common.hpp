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

// flatten_tuples takes a pack of tuple types and concatenates them
// into a single tuple type.
// Related, flatten_tuples_starts and
// flatten_tuples_ends provide index sequences that
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
  using ends = std::index_sequence<>;
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
  using ends =
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
  using ends = recurse::ends;
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
using flatten_tuples_ends = flatten_tuples_impl<std::index_sequence<>,
                                                std::index_sequence<>,
                                                std::tuple<>,
                                                Ts...>::ends;

// compile-time unit test for flatten_tuples functionality.
struct test_flatten_tuples {
  using t0 = std::tuple<>;
  using t1 = std::tuple<int, int, int>;
  using t2 = std::tuple<>;
  using t3 = std::tuple<float>;
  using flattened = flatten_tuples<t0, t1, t2, t3>;
  using starts = flatten_tuples_starts<t0, t1, t2, t3>;
  using ends = flatten_tuples_ends<t0, t1, t2, t3>;
  static_assert(std::is_same_v<flattened, std::tuple<int, int, int, float>>);
  static_assert(
    std::is_same_v<starts, std::index_sequence<0uz, 0uz, 3uz, 3uz>>);
  static_assert(std::is_same_v<ends, std::index_sequence<0uz, 3uz, 3uz, 4uz>>);
};

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

// indices_from_condition takes a condition and a pack of types and
// generates an index sequence of the indices in the pack of types
// that satisfy the condition.

// TODO: once we get reliable compiler support for
// variable template template parameters we can get rid of
// the condition::value redirection.

template <typename T, template <typename> typename condition>
concept satisfies_condition = condition<T>::value;

template <template <typename> typename condition,
          std::size_t current_index,
          typename indices,
          typename reverse_indices,
          typename... Ts>
struct indices_from_condition_impl;

// empty list of types case
template <template <typename> typename condition>
struct indices_from_condition_impl<condition,
                                   0uz,
                                   std::index_sequence<>,
                                   std::index_sequence<>> {
  using indices = std::index_sequence<>;
  using reverse_indices = std::index_sequence<>;
};

// Recursion base case
template <template <typename> typename condition,
          std::size_t current_index,
          std::size_t... Ix,
          std::size_t... RIx,
          typename T>
struct indices_from_condition_impl<condition,
                                   current_index,
                                   std::index_sequence<Ix...>,
                                   std::index_sequence<RIx...>,
                                   T> {
  using indices = std::conditional_t<condition<T>::value,
                                     std::index_sequence<Ix..., current_index>,
                                     std::index_sequence<Ix...>>;
  using reverse_indices = std::conditional_t<
    condition<T>::value,
    std::index_sequence<RIx..., sizeof...(Ix)>,
    std::index_sequence<RIx..., std::numeric_limits<std::size_t>::max()>>;
};

// Actual recursive case for true condition branch.
// Do the condition test in a requires clause to avoid
// instantiating both branches.
template <template <typename> typename condition,
          std::size_t current_index,
          std::size_t... Ix,
          std::size_t... RIx,
          typename T,
          typename... Ts>
  requires satisfies_condition<T, condition>
struct indices_from_condition_impl<condition,
                                   current_index,
                                   std::index_sequence<Ix...>,
                                   std::index_sequence<RIx...>,
                                   T,
                                   Ts...> {
  static_assert(sizeof...(Ts),
                "Empty parameter pack case should be handled by a different "
                "template candidate.");
  using recurse =
    indices_from_condition_impl<condition,
                                current_index + 1uz,
                                std::index_sequence<Ix..., current_index>,
                                std::index_sequence<RIx..., sizeof...(Ix)>,
                                Ts...>;
  using indices = recurse::indices;
  using reverse_indices = recurse::reverse_indices;
};

// Actual recursive case for the false condition branch.
template <template <typename> typename condition,
          std::size_t current_index,
          std::size_t... Ix,
          std::size_t... RIx,
          typename T,
          typename... Ts>
struct indices_from_condition_impl<condition,
                                   current_index,
                                   std::index_sequence<Ix...>,
                                   std::index_sequence<RIx...>,
                                   T,
                                   Ts...> {
  static_assert(sizeof...(Ts),
                "Empty parameter pack case should be handled by a different "
                "template candidate.");
  static_assert(
    !condition<T>::value,
    "True case should be handled by a different template candidate.");
  using recurse = indices_from_condition_impl<
    condition,
    current_index + 1uz,
    std::index_sequence<Ix...>,
    std::index_sequence<RIx..., std::numeric_limits<std::size_t>::max()>,
    Ts...>;
  using indices = recurse::indices;
  using reverse_indices = recurse::reverse_indices;
};

// Assert that this case isn't reached since recursion should have already hit
// the base case.
template <template <typename> typename condition,
          std::size_t current_index,
          std::size_t... Ix,
          std::size_t... RIx>
struct indices_from_condition_impl<condition,
                                   current_index,
                                   std::index_sequence<Ix...>,
                                   std::index_sequence<RIx...>> {
  static_assert(false, "Recursion logic error in indices_from_condition_impl");
};

template <template <typename> typename condition, typename... Ts>
using indices_from_condition =
  indices_from_condition_impl<condition,
                              0uz,
                              std::index_sequence<>,
                              std::index_sequence<>,
                              Ts...>::indices;

template <template <typename> typename condition, typename... Ts>
using reverse_indices_from_condition =
  indices_from_condition_impl<condition,
                              0uz,
                              std::index_sequence<>,
                              std::index_sequence<>,
                              Ts...>::reverse_indices;

// Unit test for indices_from_condition
struct test_indices_from_condition {
  template <typename T>
  using is_int = std::is_same<T, int>;
  using indices =
    indices_from_condition<is_int, int, float, float, int, double, int>;
  using reverse_indices =
    reverse_indices_from_condition<is_int, int, float, float, int, double, int>;
  static constexpr std::size_t mx = std::numeric_limits<std::size_t>::max();
  static_assert(std::is_same_v<indices, std::index_sequence<0uz, 3uz, 5uz>>);
  static_assert(std::is_same_v<reverse_indices,
                               std::index_sequence<0uz, mx, mx, 1uz, mx, 2uz>>);
};

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

} // namespace stdexx

#endif // #ifndef QTHREADS_STDEXEC_COMMON_HPP
