// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef _BOOST_HISTOGRAM_HISTOGRAM_IMPL_STATIC_HPP_
#define _BOOST_HISTOGRAM_HISTOGRAM_IMPL_STATIC_HPP_

#include <boost/fusion/adapted/mpl.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/container/vector/convert.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/as_vector.hpp>
#include <boost/fusion/include/comparison.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/sequence/comparison.hpp>
#include <boost/histogram/axis/axis.hpp>
#include <boost/histogram/detail/axis_visitor.hpp>
#include <boost/histogram/detail/meta.hpp>
#include <boost/histogram/detail/utility.hpp>
#include <boost/histogram/histogram_fwd.hpp>
#include <boost/histogram/storage/operators.hpp>
#include <boost/histogram/value_iterator.hpp>
#include <boost/mpl/count.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/vector.hpp>
#include <type_traits>

// forward declaration for serialization
namespace boost {
namespace serialization {
class access;
}
} // namespace boost

namespace boost {
namespace histogram {

template <typename Axes, typename Storage> class static_histogram {
  static_assert(!mpl::empty<Axes>::value, "at least one axis required");
  using axes_size = typename fusion::result_of::size<Axes>::type;

public:
  using axes_type = typename fusion::result_of::as_vector<Axes>::type;
  using value_type = typename Storage::value_type;
  using value_iterator = value_iterator_over<Storage>;

  static_histogram() = default;
  static_histogram(const static_histogram &rhs) = default;
  static_histogram(static_histogram &&rhs) = default;
  static_histogram &operator=(const static_histogram &rhs) = default;
  static_histogram &operator=(static_histogram &&rhs) = default;

  template <typename... Axis>
  explicit static_histogram(const Axis &... axis) : axes_(axis...) {
    storage_ = Storage(bincount_from_axes());
  }

  explicit static_histogram(axes_type &&axes) : axes_(std::move(axes)) {
    storage_ = Storage(bincount_from_axes());
  }

  template <typename S>
  explicit static_histogram(const static_histogram<Axes, S> &rhs)
      : storage_(rhs.storage_) {
    detail::axes_assign(axes_, rhs.axes_);
  }

  template <typename A, typename S>
  explicit static_histogram(const dynamic_histogram<A, S> &rhs)
      : storage_(rhs.storage_) {
    detail::axes_assign(axes_, rhs.axes_);
  }

  template <typename S>
  static_histogram &operator=(const static_histogram<Axes, S> &rhs) {
    if (static_cast<const void *>(this) != static_cast<const void *>(&rhs)) {
      detail::axes_assign(axes_, rhs.axes_);
      storage_ = rhs.storage_;
    }
    return *this;
  }

  template <typename A, typename S>
  static_histogram &operator=(const dynamic_histogram<A, S> &rhs) {
    if (static_cast<const void *>(this) != static_cast<const void *>(&rhs)) {
      detail::axes_assign(axes_, rhs.axes_);
      storage_ = rhs.storage_;
    }
    return *this;
  }

  template <typename A, typename S>
  bool operator==(const static_histogram<A, S> &rhs) const noexcept {
    return false;
  }

  template <typename S>
  bool operator==(const static_histogram<Axes, S> &rhs) const noexcept {
    return detail::axes_equal(axes_, rhs.axes_) && storage_ == rhs.storage_;
  }

  template <typename A, typename S>
  bool operator==(const dynamic_histogram<A, S> &rhs) const noexcept {
    return detail::axes_equal(axes_, rhs.axes_) && storage_ == rhs.storage_;
  }

  template <typename A, typename S>
  bool operator!=(const static_histogram<A, S> &rhs) const noexcept {
    return !operator==(rhs);
  }

  template <typename A, typename S>
  bool operator!=(const dynamic_histogram<A, S> &rhs) const noexcept {
    return !operator==(rhs);
  }

  template <typename S>
  static_histogram &operator+=(const static_histogram<Axes, S> &rhs) {
    if (!detail::axes_equal(axes_, rhs.axes_))
      throw std::logic_error("axes of histograms differ");
    storage_ += rhs.storage_;
    return *this;
  }

  template <typename A, typename S>
  static_histogram &operator+=(const dynamic_histogram<A, S> &rhs) {
    if (!detail::axes_equal(axes_, rhs.axes_))
      throw std::logic_error("axes of histograms differ");
    storage_ += rhs.storage_;
    return *this;
  }

  static_histogram &operator*=(const value_type rhs) {
    storage_ *= rhs;
    return *this;
  }

  static_histogram &operator/=(const value_type rhs) {
    storage_ *= 1.0 / rhs;
    return *this;
  }

  template <typename... Args> void fill(Args &&... args) {
    using n_count = typename mpl::count<mpl::vector<Args...>, count>;
    using n_weight = typename mpl::count<mpl::vector<Args...>, weight>;
    static_assert(
        (n_count::value + n_weight::value) <= 1,
        "arguments may contain at most one instance of type count or weight");
    static_assert(sizeof...(args) ==
                      (axes_size::value + n_count::value + n_weight::value),
                  "number of arguments does not match histogram dimension");
    fill_impl(mpl::int_<(n_count::value + 2 * n_weight::value)>(),
              std::forward<Args>(args)...);
  }

  template <typename... Indices> value_type value(Indices &&... indices) const {
    static_assert(sizeof...(indices) == axes_size::value,
                  "number of arguments does not match histogram dimension");
    std::size_t idx = 0, stride = 1;
    lin<0>(idx, stride, std::forward<Indices>(indices)...);
    if (stride == 0) {
      throw std::out_of_range("invalid index");
    }
    return storage_.value(idx);
  }

  template <typename S = Storage, typename... Indices>
  detail::requires_variance_support<S> variance(Indices &&... indices) const {
    static_assert(sizeof...(indices) == axes_size::value,
                  "number of arguments does not match histogram dimension");
    std::size_t idx = 0, stride = 1;
    lin<0>(idx, stride, std::forward<Indices>(indices)...);
    if (stride == 0) {
      throw std::out_of_range("invalid index");
    }
    return storage_.variance(idx);
  }

  /// Number of axes (dimensions) of histogram
  constexpr unsigned dim() const noexcept { return axes_size::value; }

  /// Total number of bins in the histogram (including underflow/overflow)
  std::size_t bincount() const noexcept { return storage_.size(); }

  /// Sum of all counts in the histogram
  double sum() const noexcept {
    double result = 0.0;
    for (std::size_t i = 0, n = storage_.size(); i < n; ++i)
      result += storage_.value(i);
    return result;
  }

  /// Reset bin counters to zero
  void reset() { storage_ = Storage(bincount_from_axes()); }

  /// Get N-th axis (const version)
  template <int N>
  typename std::add_const<
      typename fusion::result_of::value_at_c<axes_type, N>::type>::type &
  axis(mpl::int_<N>) const {
    static_assert(N < axes_size::value, "axis index out of range");
    return fusion::at_c<N>(axes_);
  }

  /// Get N-th axis
  template <int N>
  typename fusion::result_of::value_at_c<axes_type, N>::type &
  axis(mpl::int_<N>) {
    static_assert(N < axes_size::value, "axis index out of range");
    return fusion::at_c<N>(axes_);
  }

  // Get first axis (convenience for 1-d histograms, const version)
  constexpr typename std::add_const<
      typename fusion::result_of::value_at_c<axes_type, 0>::type>::type &
  axis() const {
    return fusion::at_c<0>(axes_);
  }

  // Get first axis (convenience for 1-d histograms)
  typename fusion::result_of::value_at_c<axes_type, 0>::type &axis() {
    return fusion::at_c<0>(axes_);
  }

  /// Apply unary functor/function to each axis
  template <typename Unary> void for_each_axis(Unary &&unary) const {
    fusion::for_each(axes_, unary);
  }

  /// Returns a lower-dimensional histogram
  template <int N, typename... Rest>
  auto reduce_to(mpl::int_<N>, Rest...) const -> static_histogram<
      detail::axes_select<Axes, mpl::vector<mpl::int_<N>, Rest...>>, Storage> {
    using HR = static_histogram<
        detail::axes_select<Axes, mpl::vector<mpl::int_<N>, Rest...>>, Storage>;
    typename HR::axes_type axes;
    detail::axes_assign_subset<mpl::vector<mpl::int_<N>, Rest...>>(axes, axes_);
    auto hr = HR(std::move(axes));
    const auto b =
        detail::bool_mask<mpl::vector<mpl::int_<N>, Rest...>>(dim(), true);
    reduce_impl(hr, b);
    return hr;
  }

  value_iterator begin() const noexcept {
    return value_iterator(*this, storage_);
  }

  value_iterator end() const noexcept {
    return value_iterator(storage_);
  }

private:
  axes_type axes_;
  Storage storage_;

  std::size_t bincount_from_axes() const noexcept {
    detail::field_count_visitor fc;
    fusion::for_each(axes_, fc);
    return fc.value;
  }

  template <typename... Args>
  inline void fill_impl(mpl::int_<0>, Args &&... args) {
    std::size_t idx = 0, stride = 1;
    xlin<0>(idx, stride, std::forward<Args>(args)...);
    if (stride)
      storage_.increase(idx);
  }

  template <typename... Args>
  inline void fill_impl(mpl::int_<1>, Args &&... args) {
    std::size_t idx = 0, stride = 1;
    unsigned n = 0;
    xlin_n<0>(idx, stride, n, std::forward<Args>(args)...);
    if (stride)
      storage_.add(idx, n);
  }

  template <typename... Args>
  inline void fill_impl(mpl::int_<2>, Args &&... args) {
    std::size_t idx = 0, stride = 1;
    double w = 0.0;
    xlin_w<0>(idx, stride, w, std::forward<Args>(args)...);
    if (stride)
      storage_.increase_by_weight(idx, w);
  }

  template <unsigned D> inline void lin(std::size_t &, std::size_t &) const {}

  template <unsigned D, typename First, typename... Rest>
  inline void lin(std::size_t &idx, std::size_t &stride, First &&x,
                  Rest &&... rest) const {
    detail::lin(idx, stride, fusion::at_c<D>(axes_), std::forward<First>(x));
    return lin<D + 1>(idx, stride, std::forward<Rest>(rest)...);
  }

  template <unsigned D> inline void xlin(std::size_t &, std::size_t &) const {}

  template <unsigned D, typename First, typename... Rest>
  inline void xlin(std::size_t &idx, std::size_t &stride, First &&first,
                   Rest &&... rest) const {
    detail::xlin(idx, stride, fusion::at_c<D>(axes_),
                 std::forward<First>(first));
    return xlin<D + 1>(idx, stride, std::forward<Rest>(rest)...);
  }

  template <unsigned D>
  inline void xlin_w(std::size_t &, std::size_t &, double &) const {}

  template <unsigned D, typename First, typename... Rest>
  inline typename disable_if<is_same<First, weight>>::type
  xlin_w(std::size_t &idx, std::size_t &stride, double &x, First &&first,
         Rest &&... rest) const {
    detail::xlin(idx, stride, fusion::at_c<D>(axes_),
                 std::forward<First>(first));
    return xlin_w<D + 1>(idx, stride, x, std::forward<Rest>(rest)...);
  }

  template <unsigned D, typename First, typename... Rest>
  inline typename enable_if<is_same<First, weight>>::type
  xlin_w(std::size_t &idx, std::size_t &stride, double &x, First &&first,
         Rest &&... rest) const {
    x = first.value;
    return xlin_w<D>(idx, stride, x, std::forward<Rest>(rest)...);
  }

  template <unsigned D>
  inline void xlin_n(std::size_t &, std::size_t &, unsigned &) const {}

  template <unsigned D, typename First, typename... Rest>
  inline typename disable_if<is_same<First, count>>::type
  xlin_n(std::size_t &idx, std::size_t &stride, unsigned &x, First &&first,
         Rest &&... rest) const {
    detail::xlin(idx, stride, fusion::at_c<D>(axes_),
                 std::forward<First>(first));
    return xlin_n<D + 1>(idx, stride, x, std::forward<Rest>(rest)...);
  }

  template <unsigned D, typename First, typename... Rest>
  inline typename enable_if<is_same<First, count>>::type
  xlin_n(std::size_t &idx, std::size_t &stride, unsigned &x, First &&first,
         Rest &&... rest) const {
    x = first.value;
    return xlin_n<D>(idx, stride, x, std::forward<Rest>(rest)...);
  }

  struct shape_assign_visitor {
    mutable std::vector<unsigned>::iterator ni;
    template <typename Axis> void operator()(const Axis &a) const {
      *ni = a.shape();
      ++ni;
    }
  };

  template <typename H>
  void reduce_impl(H &h, const std::vector<bool> &b) const {
    std::vector<unsigned> n(dim());
    for_each_axis(shape_assign_visitor{n.begin()});
    detail::index_mapper m(n, b);
    do {
      detail::storage_add(h.storage_, storage_, m.second, m.first);
    } while (m.next());
  }

  template <typename A, typename S> friend class static_histogram;
  template <typename A, typename S> friend class dynamic_histogram;
  friend class ::boost::serialization::access;
  template <typename Archive> void serialize(Archive &, unsigned);
};

/// default static type factory
template <typename... Axis>
inline static_histogram<mpl::vector<Axis...>>
make_static_histogram(Axis &&... axis) {
  using h = static_histogram<mpl::vector<Axis...>>;
  auto axes = typename h::axes_type(std::forward<Axis>(axis)...);
  return h(std::move(axes));
}

/// static type factory with variable storage type
template <typename Storage, typename... Axis>
inline static_histogram<mpl::vector<Axis...>, Storage>
make_static_histogram_with(Axis &&... axis) {
  using h = static_histogram<mpl::vector<Axis...>, Storage>;
  auto axes = typename h::axes_type(std::forward<Axis>(axis)...);
  return h(std::move(axes));
}

} // namespace histogram
} // namespace boost

#endif
