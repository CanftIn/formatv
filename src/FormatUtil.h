#ifndef FORMATV_FORMAT_UTIL_H
#define FORMATV_FORMAT_UTIL_H

#include <array>
#include <cassert>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include "FormatAlign.h"

namespace Formatv {

class FormatUtil {
 public:
  static auto TranslateLocChar(char c) -> std::optional<AlignStyle> {
    switch (c) {
      case '-':
        return AlignStyle::Left;
      case '=':
        return AlignStyle::Center;
      case '+':
        return AlignStyle::Right;
      default:
        return std::nullopt;
    }
  }

  static auto ltrim(const std::string& str, const std::string& chars)
      -> std::string {
    const auto str_begin = str.find_first_not_of(chars);
    return (str_begin == std::string::npos) ? "" : str.substr(str_begin);
  }

  static auto rtrim(const std::string& str, const std::string& chars)
      -> std::string {
    const auto str_end = str.find_last_not_of(chars);
    return (str_end == std::string::npos) ? "" : str.substr(0, str_end + 1);
  }

  static auto trim(const std::string& str,
                   const std::string& chars = " \t\n\v\f\r") -> std::string {
    return rtrim(ltrim(str, chars), chars);
  }

  template <typename T>
  static auto ConsumeInteger(std::string& str, int radix, T& out_value)
      -> bool {
    std::size_t pos = 0;

    try {
      if constexpr (std::numeric_limits<T>::is_signed) {
        long long val = std::stoll(str, &pos, radix);

        if (val < std::numeric_limits<T>::min() ||
            val > std::numeric_limits<T>::max()) {
          return false;
        }

        out_value = static_cast<T>(val);
      } else {
        unsigned long long val = std::stoull(str, &pos, radix);

        if (val > std::numeric_limits<T>::max()) {
          return false;
        }

        out_value = static_cast<T>(val);
      }

      str = drop_front(str, pos);

      return pos == str.length();
    } catch (const std::invalid_argument& e) {
      return false;
    } catch (const std::out_of_range& e) {
      return false;
    }
  }

  static auto take_while(const std::string& str, std::function<bool(char)> f)
      -> std::string {
    auto end = std::find_if_not(str.begin(), str.end(), f);
    return std::string(str.begin(), end);
  }

  static auto take_front(const std::string& str, std::size_t n = 1)
      -> std::string {
    return str.substr(0, n);
  }

  static auto drop_front(const std::string& str, std::size_t n = 1)
      -> std::string {
    return str.substr(n);
  }

  static auto slice(const std::string& str, std::size_t start, std::size_t end)
      -> std::string {
    const std::size_t length = str.length();
    start = std::min(start, length);
    end = std::clamp(end, start, length);
    return str.substr(start, end - start);
  }

  static auto find(const std::string& str, char c, size_t from = 0) -> size_t {
    return std::string_view(str).find(c, from);
  }

  static auto find_first_of(const std::string& str, char c, size_t from = 0)
      -> size_t {
    return find(str, c, from);
  }

  static auto substr(const std::string& str, size_t start,
                     size_t n = std::string::npos) -> std::string {
    const std::size_t length = str.length();
    start = std::min(start, length);
    n = std::min(n, length - start);
    return str.substr(start, n);
  }
};

namespace Internal {

namespace AdlDetail {

using std::begin;

template <typename RangeT>
constexpr auto begin_impl(RangeT&& range)
    -> decltype(begin(std::forward<RangeT>(range))) {
  return begin(std::forward<RangeT>(range));
}

using std::end;

template <typename RangeT>
constexpr auto end_impl(RangeT&& range)
    -> decltype(end(std::forward<RangeT>(range))) {
  return end(std::forward<RangeT>(range));
}

using std::swap;

template <typename T>
constexpr void swap_impl(T&& lhs,
                         T&& rhs) noexcept(noexcept(swap(std::declval<T>(),
                                                         std::declval<T>()))) {
  swap(std::forward<T>(lhs), std::forward<T>(rhs));
}

using std::size;

template <typename RangeT>
constexpr auto size_impl(RangeT&& range)
    -> decltype(size(std::forward<RangeT>(range))) {
  return size(std::forward<RangeT>(range));
}

}  // namespace AdlDetail

template <typename RangeT>
constexpr auto adl_begin(RangeT&& range)
    -> decltype(AdlDetail::begin_impl(std::forward<RangeT>(range))) {
  return AdlDetail::begin_impl(std::forward<RangeT>(range));
}

template <typename RangeT>
constexpr auto adl_end(RangeT&& range)
    -> decltype(AdlDetail::end_impl(std::forward<RangeT>(range))) {
  return AdlDetail::end_impl(std::forward<RangeT>(range));
}

template <typename R, typename UnaryPredicate>
auto find_if(R&& range, UnaryPredicate p) {
  return std::find_if(adl_begin(range), adl_end(range), p);
}

template <typename R, typename UnaryPredicate>
auto find_if_not(R&& range, UnaryPredicate p) {
  return std::find_if_not(adl_begin(range), adl_end(range), p);
}

}  // namespace Internal

template <typename T>
class ArrayRef {
 public:
  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iteartor = std::reverse_iterator<const_iterator>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

 private:
  const T* Data = nullptr;

  size_type Length = 0;

  void debugCheckNullptrInvariant() const {
    assert((Data != nullptr) ||
           (Length == 0) &&
               "Created ArrayRef with nullptr and non-zero length.");
  }

 public:
  ArrayRef() = default;

  ArrayRef(std::nullopt_t /*unused*/) {}

  ArrayRef(const T& one_elt) : Data(&one_elt), Length(1) {}

  constexpr ArrayRef(const T* data, size_t length)
      : Data(data), Length(length) {
    debugCheckNullptrInvariant();
  }

  constexpr ArrayRef(const T* begin, const T* end)
      : Data(begin), Length(end - begin) {
    debugCheckNullptrInvariant();
  }

  template <
      typename Container,
      typename = std::enable_if_t<std::is_same_v<
          std::remove_const_t<decltype(std::declval<Container>().data())>, T*>>>
  ArrayRef(const Container& c) : Data(c.data()), Length(c.size()) {
    debugCheckNullptrInvariant();
  }

  template <typename A>
  ArrayRef(const std::vector<T, A>& vec)
      : Data(vec.data()), Length(vec.size()) {}

  template <size_t N>
  constexpr ArrayRef(const std::array<T, N>& arr)
      : Data(arr.data()), Length(N) {}

  template <size_t N>
  constexpr ArrayRef(const T (&arr)[N]) : Data(arr), Length(N) {}

  constexpr ArrayRef(const std::initializer_list<T>& vec)
      : Data(vec.begin() == vec.end() ? static_cast<T*>(nullptr) : vec.begin()),
        Length(vec.size()) {}

  template <typename U>
  ArrayRef(
      const ArrayRef<U*>& a,
      std::enable_if_t<std::is_convertible_v<U* const*, T const*>>* /*unused*/ =
          nullptr)
      : Data(a.data()), Length(a.size()) {}

  template <typename U, typename A>
  ArrayRef(
      const std::vector<U*, A>& vec,
      std::enable_if_t<std::is_convertible_v<U* const*, T const*>>* /*unused*/ =
          nullptr)
      : Data(vec.data()), Length(vec.size()) {}

  auto begin() const -> iterator { return Data; }
  auto end() const -> iterator { return Data + Length; }

  constexpr auto cbegin() const -> const_iterator { return Data; }
  constexpr auto cend() const -> const_iterator { return Data + Length; }

  auto rbegin() const -> reverse_iterator { return reverse_iterator(end()); }
  auto rend() const -> reverse_iterator { return reverse_iterator(begin()); }

  auto crbegin() const -> const_reverse_iteartor {
    return const_reverse_iteartor(cend());
  }
  auto crend() const -> const_reverse_iteartor {
    return const_reverse_iteartor(cbegin());
  }

  constexpr auto empty() const -> bool { return Length == 0; }
  constexpr auto data() const -> const T* { return Data; }
  constexpr auto size() const -> size_t { return Length; }

  auto front() const -> const T& {
    assert(!empty() && "Cannot get the front of an empty ArrayRef");
    return Data[0];
  }

  auto back() const -> const T& {
    assert(!empty() && "Cannot get the back of an empty ArrayRef");
    return Data[Length - 1];
  }

  constexpr auto equals(ArrayRef rhs) const -> bool {
    return Length == rhs.Length && std::equal(begin(), end(), rhs.begin());
  }

  auto slice(size_t n, size_t m) const -> ArrayRef<T> {
    assert(n + m <= size() && "Invalid slice range");
    return ArrayRef<T>(data() + n, m);
  }

  auto slice(size_t n) const -> ArrayRef<T> { return slice(n, size() - n); }

  auto drop_front(size_t n = 1) const -> ArrayRef<T> {
    return slice(n, size() - n);
  }

  auto drop_back(size_t n = 1) const -> ArrayRef<T> {
    return slice(0, size() - n);
  }

  template <class PredicateT>
  auto drop_while(PredicateT pred) const -> ArrayRef<T> {
    return ArrayRef<T>(Internal::find_if_not(*this, pred), end());
  }

  template <class PredicateT>
  auto drop_until(PredicateT pred) const -> ArrayRef<T> {
    return ArrayRef<T>(Internal::find_if(*this, pred), end());
  }

  auto take_front(size_t n = 1) const -> ArrayRef<T> {
    if (n >= size()) {
      return *this;
    }
    return drop_back(size() - n);
  }

  auto take_back(size_t n = 1) const -> ArrayRef<T> {
    if (n >= size()) {
      return *this;
    }
    return drop_front(size() - n);
  }

  template <class PredicateT>
  auto take_while(PredicateT pred) const -> ArrayRef<T> {
    return ArrayRef<T>(begin(), Internal::find_if_not(*this, pred));
  }

  template <class PredicateT>
  auto take_until(PredicateT pred) const -> ArrayRef<T> {
    return ArrayRef<T>(begin(), Internal::find_if(*this, pred));
  }

  constexpr auto operator[](size_t index) const -> const T& {
    assert(index < Length && "Invalid index!");
    return Data[index];
  }

  template <typename U>
  auto operator=(U&& temporary)
      -> std::enable_if_t<std::is_same_v<U, T>, ArrayRef<T>>& = delete;

  template <typename U>
  auto operator=(std::initializer_list<U>)
      -> std::enable_if_t<std::is_same_v<U, T>, ArrayRef<T>>& = delete;

  auto vec() const -> std::vector<T> {
    return std::vector<T>(Data, Data + Length);
  }

  operator std::vector<T>() const { return vec(); }
};

}  // namespace Formatv

#endif  // FORMATV_FORMAT_UTIL_H