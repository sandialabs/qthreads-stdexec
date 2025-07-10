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
