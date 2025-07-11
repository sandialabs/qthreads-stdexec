#define once

#include <stdio.h>

#include <concepts>
#include <iostream>
#include <type_traits>

#include <stdexec/execution.hpp>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

#include "common.hpp"

namespace stdexx {

int init() { return qthread_initialize(); }

void finalize() { qthread_finalize(); }

struct qthreads_domain;
struct qthreads_scheduler;
struct qthreads_env;

struct qthreads_sender_tag : stdexec::sender_t {};

template <typename S>
concept is_qthreads_sender =
  std::derived_from<typename S::sender_concept, qthreads_sender_tag>;

template <typename Der>
struct qthreads_base_sender;
struct qthreads_sender;
template <typename Val>
struct qthreads_just_sender;
template <typename Func>
struct qthreads_basic_func_sender;
template <typename Func, typename Arg>
struct qthreads_func_sender;

template <class Tag, class... Env>
struct transform_sender_for;

template <class Tag>
struct apply_sender_for;

struct qthreads_domain {
  // The example we're following for transform_sender uses
  // stdexec::sender_expr as the concept for Sender here,
  // but that seems to require some internal or undocumented
  // stuff right now, so trying to get that concept to match
  // a qthreads_sender causes the compiler to word-vomit
  // a mountain of unhelpful concept mismatch errors.
  // For now just skip the concept check here.
  template <stdexec::sender_expr Sender, class... Env>
  auto transform_sender(Sender &&sndr, Env const &...env) const {
    using Tag = stdexec::tag_of_t<Sender>;
    // The stream domain example in nvexec uses some internal
    // idioms to unpack the info from inside stdexec::then.
    // For now we're following that same pattern rather than chase
    // down the info manually.
    // TODO: is there even an established method in the spec for getting the
    // invocable back out of a sender adaptor?
    return stdexec::__sexpr_apply(static_cast<Sender &&>(sndr),
                                  transform_sender_for<Tag, Env...>{env...});
  }

  // transform_sender gets called recursively until one of the senders
  // returns something of its own type. We use this separate overload
  // to cover that base case.
  template <typename Sn, typename... Env>
    requires is_qthreads_sender<Sn>
  auto &&transform_sender(Sn &&sndr, Env const &...env) const noexcept;

  // The domain's apply_sender routine is used to implement
  // customization of sync_wait. Here we're just forwarding
  // to a separate template we'll fully define much later.
  template <class Tag, stdexec::sender Sender, class... Args>
    requires stdexec::__callable<apply_sender_for<Tag>, Sender, Args...>
  static auto apply_sender(Tag, Sender &&sndr, Args &&...args) {
    return apply_sender_for<Tag>{}(static_cast<Sender &&>(sndr),
                                   static_cast<Args &&>(args)...);
  }
};

// Scheduler type usable with stdexec APIs.
// In our case it's mostly trivial since the qthreads scheduler
// is a static thing that (of necessity) has to be initialized/deinitialized
// elsewhere.
struct qthreads_scheduler {
  constexpr qthreads_scheduler() = default;

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_scheduler const &) noexcept;

  bool operator==(qthreads_scheduler const &rhs) const noexcept { return true; }

  bool operator!=(qthreads_scheduler const &rhs) const noexcept {
    return !(*this == rhs);
  }

  // Called by stdexec::schedule to get a qthreads_sender that can
  // start a chain of tasks on this scheduler.
  qthreads_sender schedule() const noexcept;
};

// Associated env for all qthreads sender types.
// TODO: why did they design the env to be distinct from the domain and
// scheduler?
struct qthreads_env {
  qthreads_scheduler get_completion_scheduler() const noexcept { return {}; }

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_env const &) noexcept;
};

// CRTP type used by the various operation states.
// This implements the qthread_fork call and stores the FEB.
// The associated qthread_readFF call happens during sync_wait
// whenever waiting on an associated qthreads sender.
// The types that subclass from this one provide a static
// function that gets passed th qthread_fork as well as any
// additional init/deinit they may need.
template <typename Derived_Op_State, typename Receiver>
struct qt_os_base : immovable {
  aligned_t feb;
  Receiver receiver;

  template <typename Receiver_>
  qt_os_base(Receiver_ &&r): feb(0u), receiver(std::forward<Receiver_>(r)) {}

  inline void start() noexcept {
    auto st = stdexec::get_stop_token(stdexec::get_env(receiver));
    if (st.stop_requested()) {
      stdexec::set_stopped(std::move(receiver));
      return;
    }
    int r = qthread_fork(&Derived_Op_State::task, this, &feb);

    if (r != QTHREAD_SUCCESS) {
      stdexec::set_error(std::move(this->receiver), r);
    }
  }

  inline void wait() noexcept { qthread_readFF(NULL, &feb); }
};

// Operation state for the case where we're just returning a sender
// from stdexec::schedule(qthreads_scheduler()) that can then be used
// to run other stuff inside the associated qthread using stdexec::then.
// Note: stdexec algs like "then" do their work inside set_value,
// so this is all that's needed.
template <typename Receiver>
struct operation_state : qt_os_base<operation_state<Receiver>, Receiver> {
  static aligned_t task(void *arg) noexcept {
    auto *os = static_cast<operation_state *>(arg);
    stdexec::set_value(std::move(os->receiver));
    return 0u;
  }
};

// Operation state for a qthreads sender type that behaves similarly
// to stdexec::just. It contains a value and passes it to the
// associated set_value call. It can be used to start a chain
// of stdexec::then calls.
template <typename Val, typename Receiver>
struct just_operation_state :
  qt_os_base<just_operation_state<Val, Receiver>, Receiver> {
  Val val;

  template <typename Val_, typename Receiver_>
  just_operation_state(Val_ &&v, Receiver_ &&receiver):
    qt_os_base<just_operation_state<Val, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    val(std::forward<Val_>(v)) {}

  static aligned_t task(void *os_void) noexcept {
    just_operation_state *os =
      reinterpret_cast<just_operation_state *>(os_void);
    stdexec::set_value(std::move(os->receiver), std::move(os->val));
    return 0u;
  }
};

// Operation state for a qthreads sender type that
// encapsulates an invocable with an associated single argument.
// TODO: multiple arguments? Thus far I got hung up on
// some kind of issue preventing expanding parameter packs
// as struct members.
template <typename Func, typename Arg, typename Receiver>
struct func_operation_state :
  qt_os_base<func_operation_state<Func, Arg, Receiver>, Receiver> {
  Func func;
  Arg arg;

  template <typename Func_, typename Arg_, typename Receiver_>
  func_operation_state(Func_ &&f, Arg_ &&a, Receiver_ &&receiver):
    qt_os_base<func_operation_state<Func, Arg, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    func(std::forward<Func_>(f)), arg(std::forward<Arg_>(a)) {}

  static aligned_t task(void *os_void) noexcept {
    // TODO: we can probably forward C++ exceptions out of here. Figure out
    // how.
    func_operation_state *os =
      reinterpret_cast<func_operation_state *>(os_void);
    aligned_t ret = (os->func)(os->arg);
    stdexec::set_value(std::move(os->receiver), ret);
    return ret;
  }
};

// Operation state for a simpler version of the previous
// where there's no associated argument passed, just a bare function call.
template <typename Func, typename Receiver>
struct basic_func_operation_state :
  qt_os_base<basic_func_operation_state<Func, Receiver>, Receiver> {
  Func func;

  template <typename Func_, typename Receiver_>
  basic_func_operation_state(Func_ &&f, Receiver_ &&receiver):
    qt_os_base<basic_func_operation_state<Func, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    func(std::forward<Func_>(f)) {}

  static aligned_t task(void *os_void) noexcept {
    // TODO: we can probably forward C++ exceptions out of here. Figure out
    // how.
    basic_func_operation_state *os =
      reinterpret_cast<basic_func_operation_state *>(os_void);
    aligned_t ret = (os->func)();
    stdexec::set_value(std::move(os->receiver), ret);
    return ret;
  }
};

template <typename Receiver, typename... Senders>
struct when_all_op_state;

template <typename outer_op_state_t, std::size_t Index>
struct when_all_item_receiver {
  outer_op_state_t *op;

  qthreads_env get_env() const noexcept { return {}; }

  template <typename... V>
  __attribute__((always_inline)) void set_value(V &&...vals) && noexcept {
    op->template _set_value<Index>(static_cast<V &&>(vals)...);
  }

  template <typename E>
  __attribute__((always_inline)) void set_error(E &&e) && noexcept {
    op->template _set_error<Index>(static_cast<E &&>(e));
  }

  // Currently just disable set_stopped.
  //__attribute__((always_inline)) void set_stopped() && noexcept {
  //  op->set_stopped();
  //}
};

template <typename Receiver, typename... Senders>
struct when_all_op_state : immovable {
  template <std::size_t I>
  using item_receiver =
    when_all_item_receiver<when_all_op_state<Receiver, Senders...>, I>;
  template <typename T>
    requires is_index_sequence<T>
  struct infer_tuple_types;

  template <std::size_t... I>
  struct infer_tuple_types<std::index_sequence<I...>> {
    using os_tuple_type_impl = std::tuple<decltype(stdexec::connect(
      std::declval<Senders...[I]>(), std::declval<item_receiver<I>>()))...>;
  };

  using internal_op_state_tuple_type = infer_tuple_types<
    std::make_index_sequence<sizeof...(Senders)>>::os_tuple_type_impl;

  // A simplified API for getting the return type associated with each sender.
  // stdexec::when_all has already confirmed that all the senders are from
  // the same domain, so we can assume that all the senders are some kind
  // of qthreads sender which means we know:
  // - they don't return multiple types, so variants aren't needed
  // - they return a single value in their call to set_value
  // - they use the qthreads_env
  // TODO: currently these are fine limitations,
  //   but what are the cases where they might not be true
  //   and how should they be handled here?
  // This still wraps the type in a tuple so that it'll work
  // for senders that don't actually return a value either.
  template <typename S>
  using ret_tuple_of_qthreads_sender =
    stdexec::value_types_of_t<S, qthreads_env, std::tuple, std::variant>;

  // Get the wrapped return type for something that's known to
  // return something other than void.
  template <typename S>
  using ret_type_of_qthreads_sender =
    std::tuple_element_t<0, ret_tuple_of_qthreads_sender<S>>;

  template <typename S>
  struct does_not_return_void {
    bool value = !std::is_same_v<ret_tuple_of_qthreads_sender<S>, std::tuple<>>;
  };

  // Indices mapping non-void output values to input senders.
  using non_void_value_indices =
    indices_from_condition<does_not_return_void, Senders...>;
  // Indices mapping input senders to non-void output values.
  using ret_value_reverse_indices =
    reverse_indices_from_condition<does_not_return_void, Senders...>;

  using ret_tuple_type = apply_at_indices<ret_type_of_qthreads_sender,
                                          non_void_value_indices,
                                          Senders...>;

  // Trick to initialize immovable operation states in-place inside
  // our tuple of inner operation states.
  // Relies on C++17 guaranteed copy elision.
  // See https://stackoverflow.com/a/75594252
  template <typename S, typename R>
  struct op_state_converter {
    std::tuple<S, R> args;

    op_state_converter(S &&s, R &&r) noexcept:
      args{static_cast<S &&>(s), static_cast<R &&>(r)} {}

    using op_state =
      decltype(stdexec::connect(std::declval<S>(), std::declval<R>()));

    // Guaranteed copy elision here allows constructing
    // the operation state in-place when initializing a tuple
    // of operation states.
    operator op_state() && {
      return stdexec::connect(std::move(std::get<0>(args)),
                              std::move(std::get<1>(args)));
    }
  };

  // Need to associate indices with the sender types
  // to connect them with the appropriate receivers
  // when building the tuple of internal operation states.
  // This tuple type exists so that we can do that pairing of indices.
  // Unfortunately it adds a syntactic layer of indirection for
  // accessing the tuple of operation states.
  template <typename>
  struct op_states_t;

  // Helper function used immediately below in the op_state tuple init.
  template <std::size_t I>
  decltype(auto) internal_receiver_gen(Senders &&...senders) noexcept {
    return op_state_converter{when_all_item_receiver<decltype(*this), I>{this},
                              std::move(senders...[I])};
  }

  template <std::size_t... Ix>
  struct op_states_t<std::index_sequence<Ix...>> {
    internal_op_state_tuple_type tup;

    op_states_t(Senders &&...senders) noexcept:
      tup{op_state_converter{when_all_item_receiver<decltype(*this), Ix>{this},
                             std::move(senders...[Ix])}...} {}

    template <std::size_t I>
    decltype(auto) get() noexcept {
      return std::get<I>(tup);
    }
  };

  // TODO: should we try to make sure the return values and/or the
  // atomic counter are on separate cache lines?
  // Note: currently we require the ret_tuple values to all be
  // trivially initializable and movable. Its not clear if there
  // are ways to relax those constraints much.
  ret_tuple_type ret_tuple;
  std::atomic<std::size_t> completion_counter;
  std::atomic<std::size_t> error_counter;
  Receiver receiver;
  // Need an extra layer of indirection here to be able
  // to construct these things in-place.
  op_states_t<std::make_index_sequence<sizeof...(Senders)>> internal_op_states;

  when_all_op_state(Receiver &&r, Senders &&...senders) noexcept:
    internal_op_states{static_cast<Senders &&>(senders)...},
    receiver{std::move(r)} {
    completion_counter.store(sizeof...(Senders), std::memory_order_relaxed);
    error_counter.store(0uz, std::memory_order_relaxed);
  }

  // Called by the wrapped receivers' set_value
  // No existing qthreads senders send more than a single value
  // so we don't currently need to handle the
  // multi-return-value case here.
  // TODO: what even is the expected behavior for when_all in that case?
  template <std::size_t Index, typename V>
  void _set_value(V &&val) noexcept {
    static constexpr std::size_t output_index =
      get_at_index<ret_value_reverse_indices, Index>;
    static_assert(output_index != std::numeric_limits<std::size_t>::max());
    using expected_ret_t =
      std::tuple_element_t<0uz,
                           ret_tuple_of_qthreads_sender<Senders...[Index]>>;
    static_assert(
      std::is_same_v<V, expected_ret_t>,
      "Mismatch between the return type passed to set_value and the type "
      "specified by the corresponding completion signatures.");
    std::get<output_index>(ret_tuple) = std::move(val);
    if (!completion_counter.fetch_sub(1uz, std::memory_order_relaxed)) {
      std::apply([this](auto... args) { this->receiver.set_value(args...); },
                 ret_tuple);
    }
  }

  template <std::size_t Index, typename E>
  void _set_error(E &&err) noexcept {
    // We don't currently support cancelling qthreads tasks mid-way
    // through execution anyway, so the only concern here is which
    // error to report.
    // Unfortunately, the standard behavior for when_all is just to
    // report one of the errors and discard the others instead of
    // doing any kind of container exception type for forwarding
    // information from multiple exceptions. We'll follow that
    // convention for now, but it's reasonably within our power
    // to do something better in our specialization someday later.
    // TODO: gather all the exceptions into some kind of
    // combined exception type instead of just forwarding
    // the first one of them.
    // Note: the default when_all behavior also seems to
    // be to wait for everything to cancel or finish before
    // forwarding the error too. Since qthreads are much cheaper
    // than OS threads, it doesn't make sense to do that in our case.
    // We can just call the outer receiver's set_error here
    // and let whatever other work needs to happen start afterward.
    // Note: we're not doing anything to completion_counter because
    // we don't actually want some other completing thread to
    // call set_value anymore.
    if (!error_counter.fetch_add(1uz, std::memory_order_relaxed)) {
      this->receiver.set_error(static_cast<E &&>(err));
    }
  }

  // Rough outline of what needs to happen:
  //   - At init this inits all the wrapped operation states.
  //   - At connect, it connects an internal receiver to each
  //     wrapped sender.
  //   - At start this starts all the wrapped operation states.
  //   - When set_value is called on one of the internal receivers it
  //     just stores the value in its entry in the tuple.
  //     Whichever one ends last should call (or somehow trigger the call to)
  //     set_value for the outer receiver that expects the whole tuple
  //     of values.
  //     Note, given that there's no a-priori way to determine which
  //     operation will complete first, there needs to be an atomic
  //     counter to determine which set_value call happens last
  //     so that the outer set_value call can happen.
  //   - On wait, it waits all the operation states one after the other.
  // TODO: a tuple of the operation state types from each sender
  // TODO: a tuple of the return value from each sender.
  //   WHAT ON EARTH happens if one of the wrapped senders has multiple possible
  //   return values? Pretty sure that's a case we can (hopefully) ignore for
  //   qthreads-based senders. Maybe? Note: the cuda example seems to want to
  //   ALLOCATE the tuple for the return values!?!? It will likely be fine to
  //   just store that tuple as a value in this struct instead? Note: The
  //   existing when_all seems to return a... tuple of tuples of returns?
  // TODO: a set of forwarding receivers that forward what they get out of
  //   set_value into the final tuple returned by when_all.
  //   Note: Since the tuple has to be indexed by compile-time indices,
  //   the index needs to be a template parameter there too.
  //   TODO: each of these things needs to have some kind of reference
  //   to the outer receiver as well as the associated tuple of
  //   return values and the atomic counter counting when to
  //   call the outer set_value. What's the right idiom for this?
  // TODO: Add a layer of indirection inside each tuple element
  //   that ensures each entry has its own cache line.
  //   This is likely kind-of complicated so do this later.
  //   On the other hand, a whole cache line seems like total overkill
  //   for like... a couple ints or something. In theory we could end up
  //   in a situation where the compiler optimizes out the whole
  //   extra buffer of stuff. On the other hand, it could not
  //   be optimizing out a bunch of this template machinery too
  //   which would also be harmful. I don't know how to
  //   get it to optimize away the right parts here.
  //   We should probably test how much this even matters for
  //   the existing qthreads APIs before trying to guarantee separate
  //   cache lines here.

  template <typename Receiver_>
  when_all_op_state(Receiver_ &&r): receiver(std::forward<Receiver_>(r)) {
    // TODO: connect all the wrapped senders to the inner receivers here.
  }

  inline void start() noexcept {
    // TODO: start all the wrapped operation states here.
  }

  inline void wait() noexcept {
    // TODO: readFF everything here.
  }
};

// CRTP base class for various qthreads sender types.
// This takes care of marking it as satisfying the
// is_qthreads_sender concept (and the ordinary sender concept too).
// It's also used to avoid having to deal with writing separate
// customizations for then, sync_wait, get_env, get_domain, etc.
// for each qthreads sender type.
template <typename derived_qthreads_sender>
struct qthreads_base_sender {
  using sender_concept = qthreads_sender_tag;

  qthreads_env get_env() const noexcept { return {}; }

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_sender const &) noexcept;
};

// Qthreads sender type returned by stdexec::schedule in order to
// start a chain of tasks on the qthreads_scheduler.
struct qthreads_sender : qthreads_base_sender<qthreads_sender> {
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  operation_state<Receiver> connect(Receiver &&receiver) && {
    return {std::forward<Receiver>(receiver)};
  }
};

// Qthreads ender type wrapping a single value.
// Note: the value is moved into this struct and will be moved
// again into the operation state and then into the set_value call.
template <typename Val>
struct qthreads_just_sender : qthreads_base_sender<qthreads_just_sender<Val>> {
  Val val;

  qthreads_just_sender(Val &&v) noexcept: val(v) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(Val &&),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static just_operation_state<Val, Receiver> connect(qthreads_just_sender &&s,
                                                     Receiver &&receiver) {
    return {std::move(s.val), std::forward<Receiver>(receiver)};
  }
};

// Qthreads sender type wrapping a bare function call.
// Note: the func is moved into this struct and will be moved
// again into the operation state.
template <typename Func>
struct qthreads_basic_func_sender :
  qthreads_base_sender<qthreads_basic_func_sender<Func>> {
  Func func;

  qthreads_basic_func_sender(Func &&f) noexcept: func(f) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static basic_func_operation_state<Func, Receiver>
  connect(qthreads_basic_func_sender &&s, Receiver &&receiver) {
    return {std::move(s.func), std::forward<Receiver>(receiver)};
  }
};

// Qthreads sender type wrapping a call to a function with an argument.
// The func and arg are moved into this struct and will be moved into the
// operation state.
template <typename Func, typename Arg>
struct qthreads_func_sender :
  qthreads_base_sender<qthreads_func_sender<Func, Arg>> {
  Func func;
  Arg arg;

  qthreads_func_sender(Func f, Arg a) noexcept: func(f), arg(a) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static func_operation_state<Func, Arg, Receiver>
  connect(qthreads_func_sender &&s, Receiver &&receiver) {
    return {
      std::move(s.func), std::move(s.arg), std::forward<Receiver>(receiver)};
  }
};

// This provides the scheduler's customization for stdexec::schedule.
// It just needs to be defined down here for order of definition reasons.
qthreads_sender qthreads_scheduler::schedule() const noexcept { return {}; }

// A helper type for our implementation of stdexec::then.
// The example implementation of then in the stdexec repo
// doesn't actually handle void return types correctly
// in its templating idioms.
// On the other hand, the actual implementation for stdexec::then
// has a bunch of internal interface calls instead of just
// using specified ones. This struct type exists as a templating
// tool so we can compile the associated set_value call
// after we've determined whether the invocable passed to
// then returns void.
template <bool returns_void>
struct set_value_impl;

template <>
struct set_value_impl<true> {
  template <typename Rec, typename Func, typename... Args>
  static decltype(auto) impl(Rec &&rec, Func &&func, Args &&...args) {
    std::invoke(std::move(func), static_cast<Args &&>(args)...);
    stdexec::set_value(std::move(rec));
  }
};

template <>
struct set_value_impl<false> {
  template <typename Rec, typename Func, typename... Args>
  static decltype(auto) impl(Rec &&rec, Func &&func, Args &&...args) {
    return stdexec::set_value(
      std::move(rec),
      std::invoke(static_cast<Func &&>(func), static_cast<Args &&>(args)...));
  }
};

// Another helper type to generate the completion signatures
// depending on whether the invocable passed to our "then" customization
// returns void or not.
template <bool returns_void>
struct then_completions;

template <>
struct then_completions<true> {
  template <typename ret_t, typename... Args>
  using completions =
    stdexec::completion_signatures<stdexec::set_value_t(),
                                   stdexec::set_error_t(std::exception_ptr)>;
};

template <>
struct then_completions<false> {
  template <typename ret_t, typename... Args>
  using completions =
    stdexec::completion_signatures<stdexec::set_value_t(ret_t),
                                   stdexec::set_error_t(std::exception_ptr)>;
};

// Sender and receiver types for our customization of stdexec::then.
// Adapted from the then implementation in their examples directory.
template <class R, class F>
class qthreads_then_receiver :
  public stdexec::receiver_adaptor<qthreads_then_receiver<R, F>, R> {
  template <class... As>
  using ret_t =
    std::invoke_result_t<decltype(std::move(std::declval<F>())), As...>;
  template <typename... As>
  using _completions = then_completions<std::is_same_v<ret_t<As...>, void>>::
    template completions<ret_t<As...>, As...>;
public:
  qthreads_then_receiver(R r, F f_):
    stdexec::receiver_adaptor<qthreads_then_receiver, R>{std::move(r)},
    f(std::move(f_)) {}

  // Customize set_value by invoking the callable and passing the result to the
  // inner receiver
  template <class... As>
    requires stdexec::receiver_of<R, _completions<As...>>
  void set_value(As &&...as) && noexcept {
    try {
      set_value_impl<std::is_same_v<ret_t<As...>, void>>::impl(
        std::move(*this).base(), std::move(f), static_cast<As &&>(as)...);
    } catch (...) {
      stdexec::set_error(std::move(*this).base(), std::current_exception());
    }
  }
private:
  F f;
};

template <stdexec::sender S, typename F>
struct qthreads_then_sender : qthreads_base_sender<qthreads_then_sender<S, F>> {
  S s;
  F f;

  template <typename... Args>
  using ret_t = std::invoke_result_t<F, Args...>;

  template <typename... As>
  using set_value_t = then_completions<std::is_same_v<ret_t<As...>, void>>::
    template completions<ret_t<As...>, As...>;

  template <class Env>
  using completions_t = stdexec::transform_completion_signatures_of<
    S,
    Env,
    stdexec::completion_signatures<>,
    set_value_t>;

  template <class Env>
  auto get_completion_signatures(Env &&) && -> completions_t<Env> {
    return {};
  }

  template <stdexec::receiver R>
    requires stdexec::sender_to<S, qthreads_then_receiver<R, F>>
  auto connect(R r) && {
    // No additional data needed in the operation state, so just
    // connect the wrapped sender to the qthreads_then_receiver which
    // actually wraps the provided function.
    return stdexec::connect(
      std::move(s),
      qthreads_then_receiver<R, F>{static_cast<R &&>(r), static_cast<F &&>(f)});
  }
};

// Roughly adapted from the nvexec when_all customization.
// This does a bunch of work to derive the completion signatures for
// when_all based on the completion signatures of the various
// senders passed to it.
namespace when_all {
template <class Sender, class... Env>
concept valid_child_sender = stdexec::sender_in<Sender, Env...> && requires {
  requires(
    stdexec::__v<stdexec::__count_of<stdexec::set_value_t, Sender, Env...>> <=
    1);
};

template <class Sender, class... Env>
concept too_many_completions_sender =
  stdexec::sender_in<Sender, Env...> && requires {
    requires(
      stdexec::__v<stdexec::__count_of<stdexec::set_value_t, Sender, Env...>> >
      1);
  };

template <class Env, class... Senders>
struct completions {};

template <class... Env, class... Senders>
  requires(too_many_completions_sender<Senders, Env...> || ...)
struct completions<stdexec::__types<Env...>, Senders...> {
  static constexpr auto position_of() noexcept -> std::size_t {
    constexpr bool which[] = {too_many_completions_sender<Senders, Env...>...};
    return stdexec::__pos_of(which, which + sizeof...(Senders));
  }

  using InvalidArg = stdexec::__m_at_c<position_of(), Senders...>;
  using __t =
    stdexec::__when_all::__too_many_value_completions_error<InvalidArg, Env...>;
};

template <class... Env, class... Senders>
  requires(valid_child_sender<Senders, Env...> && ...)
struct completions<stdexec::__types<Env...>, Senders...> {
  using non_values = stdexec::__meval<
    stdexec::__concat_completion_signatures,
    stdexec::completion_signatures<>,
    stdexec::transform_completion_signatures<
      stdexec::__completion_signatures_of_t<Senders, Env...>,
      stdexec::completion_signatures<>,
      stdexec::__mconst<stdexec::completion_signatures<>>::__f>...>;
  using values =
    stdexec::__minvoke<stdexec::__mconcat<stdexec::__qf<stdexec::set_value_t>>,
                       stdexec::__value_types_t<
                         stdexec::__completion_signatures_of_t<Senders, Env...>,
                         stdexec::__q<stdexec::__types>,
                         stdexec::__msingle_or<stdexec::__types<>>>...>;
  using __t = stdexec::__if_c<
    (stdexec::__sends<stdexec::set_value_t, Senders, Env...> && ...),
    stdexec::__minvoke<
      stdexec::__mpush_back<stdexec::__q<stdexec::completion_signatures>>,
      non_values,
      values>,
    non_values>;
};
} // namespace when_all

template <is_qthreads_sender... S>
struct qthreads_when_all_sender :
  qthreads_base_sender<qthreads_when_all_sender<S...>> {
  //;
};

// Our transform_sender override calls into this for implementing stdexec::then.
template <>
struct transform_sender_for<stdexec::then_t> {
  template <class Fn, class Sender>
    requires is_qthreads_sender<Sender>
  auto operator()(stdexec::__ignore, Fn fun, Sender &&sndr) const {
    // fun is already the invocable we want to wrap.
    // It's already been extracted from inside the default "then".
    // All we need to do here is construct the associated sender from it.
    return qthreads_then_sender<Sender, Fn>{
      {}, static_cast<Sender &&>(sndr), static_cast<Fn &&>(fun)};
  }
};

// Customization for stdexec::when_all
template <>
struct transform_sender_for<stdexec::when_all_t> {
  template <typename... Senders>
  auto
  operator()(stdexec::__ignore, stdexec::__ignore, Senders &&...sndrs) const {
    static_assert(false, "made it to correct customization.");
    /*using __sender_t =
      __t<when_all_sender_t<stdexec::when_all_t, stream_scheduler,
    __id<__decay_t<Senders>>...>>; return __sender_t{ context_state_t{nullptr,
    nullptr, nullptr, nullptr}, static_cast<Senders&&>(sndrs)...
    };*/
  }
};

// TODO: the local_state and result receiver include some useless
// details left over from the default sync_wait. We should remove them.
// For now, they don't interfere with anything so it's not critical.

template <>
struct apply_sender_for<stdexec::sync_wait_t> {
  template <typename S>
  auto operator()(S &&sn);

  // Our customization of stdexec::sync_wait calls into this.
  // This is where most of the work for that happens.
  template <typename Sn>
    requires is_qthreads_sender<Sn>
  auto operator()(Sn &&sn) {
    // We're relying on some internal stuff from stdexec::sync_wait here.
    stdexec::__sync_wait::__state local_state{};
    std::optional<stdexec::__sync_wait::__sync_wait_result_t<Sn>> result{};

    // Launch the sender with a continuation that will fill in the __result
    // optional or set the exception_ptr in __local_state.
    [[maybe_unused]]
    auto op = stdexec::connect(
      std::move(sn),
      stdexec::__sync_wait::__receiver_t<Sn>{&local_state, &result});
    stdexec::start(op);

    // Wait on the FEB associated with the qthread.
    // TODO: make a function call to get its address out of the
    // operation state instead of accessing it directly.
    // Currently this works for our override of stdexec::then,
    // but there may be other stuff that requires the extra indirection.
    op.wait();
    return result;
  }
};

// Base case for transform_sender.
template <typename Sn, typename... Env>
  requires is_qthreads_sender<Sn>
auto &&qthreads_domain::transform_sender(Sn &&sndr,
                                         Env const &...env) const noexcept {
  return std::move(sndr);
}

// Associate our various types with the qthreads_domain.
// For some reason get_domain can currently only be specialized this way.
// TODO: fix and/or report this upstream.
// In theory adding get_domain as a method or using the query interface
// should work, but neither actually do.
qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_scheduler const &) noexcept {
  return {};
}

qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_env const &) noexcept {
  return {};
}

template <typename Der>
qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_base_sender<Der> const &) noexcept {
  return {};
}

} // namespace stdexx

