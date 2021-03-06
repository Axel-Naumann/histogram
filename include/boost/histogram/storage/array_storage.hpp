// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef _BOOST_HISTOGRAM_STORAGE_ARRAY_HPP_
#define _BOOST_HISTOGRAM_STORAGE_ARRAY_HPP_

#include <algorithm>
#include <boost/histogram/detail/meta.hpp>
#include <cstddef>
#include <memory>
#include <type_traits>

// forward declaration for serialization
namespace boost {
namespace serialization {
class access;
}
} // namespace boost

namespace boost {
namespace histogram {

namespace detail {
template <typename T>
struct counter_traits {
  using value_type = T;
  static value_type value(const T& t) noexcept { return t; }
  static void increase_by_count(T& lhs, const T& n) noexcept { lhs += n; }
  static void increase_by_weight(T& lhs, const T& w) noexcept { lhs += w; }
};
} // namespace detail

template <typename T> class array_storage {
public:
  using value_type = typename detail::counter_traits<T>::value_type;

  explicit array_storage(std::size_t s) { init(s); }

  array_storage() = default;
  array_storage(const array_storage &other) {
    reset(other.size());
    std::copy(other.array_.get(), other.array_.get() + size_, array_.get());
  }
  array_storage &operator=(const array_storage &other) {
    if (this != &other) {
      reset(other.size());
      std::copy(other.array_.get(), other.array_.get() + size_, array_.get());
    }
    return *this;
  }
  array_storage(array_storage &&other) {
    std::swap(size_, other.size_);
    std::swap(array_, other.array_);
  }
  array_storage &operator=(array_storage &&other) {
    if (this != &other) {
      std::swap(size_, other.size_);
      std::swap(array_, other.array_);
    }
    return *this;
  }

  template <typename S>
  explicit array_storage(const S &other) {
    reset(other.size());
    for (decltype(size_) i = 0u; i < size_; ++i) {
      array_[i] = other.value(i);
    }
  }

  template <typename S> array_storage &operator=(const S &other) {
    reset(other.size());
    for (decltype(size_) i = 0u; i < size_; ++i) {
      array_[i] = other.value(i);
    }
    return *this;
  }

  std::size_t size() const noexcept { return size_; }

  void increase(std::size_t i) noexcept { ++array_[i]; }

  void increase_by_weight(std::size_t i, const value_type w) noexcept {
    detail::counter_traits<T>::increase_by_weight(array_[i], w);
  }

  void add(std::size_t i, const value_type &n) noexcept {
    detail::counter_traits<T>::increase_by_count(array_[i], n);
  }

  template <typename U=T, typename = decltype(U(0, 0))>
  void add(std::size_t i, const value_type& value, const value_type& variance) noexcept {
    array_[i] += T(value, variance);
  }

  value_type value(std::size_t i) const noexcept {
    return detail::counter_traits<T>::value(array_[i]);
  }

  template <typename U=T, typename = decltype(std::declval<U &>().variance())>
  value_type variance(std::size_t i) const noexcept {
    return array_[i].variance();
  }

  template <typename U>
  array_storage &operator+=(const array_storage<U> &rhs) noexcept {
    for (auto i = 0ul; i < size_; ++i)
      array_[i] += rhs.array_[i];
    return *this;
  }

  array_storage &operator*=(const value_type x) noexcept {
    for (auto i = 0ul; i < size_; ++i)
      array_[i] *= x;
    return *this;
  }

private:
  std::size_t size_ = 0;
  std::unique_ptr<T[]> array_;

  void reset(std::size_t size) {
    size_ = size;
    array_.reset(new T[size]);
  }
  void init(std::size_t size) {
    reset(size);
    std::fill(array_.get(), array_.get() + size, T(0));
  }

  template <typename U> friend class array_storage;

  friend class ::boost::serialization::access;
  template <typename Archive> void serialize(Archive &, unsigned);
};

} // namespace histogram
} // namespace boost

#endif
