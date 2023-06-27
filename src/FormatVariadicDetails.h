#ifndef FORMATV_FORMAT_VARIADIC_DETAILS_H
#define FORMATV_FORMAT_VARIADIC_DETAILS_H

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

namespace Formatv {

template <typename T, typename Enable = void>
struct FormatProvider {};

namespace Internal {

class FormatAdapter {
 public:
  virtual void format(std::ostream& os, std::string options) = 0;

 protected:
  virtual ~FormatAdapter() = default;

 private:
  virtual void anchor() {}
};

template <typename T>
class ProviderFormatAdapter : public FormatAdapter {
 public:
  explicit ProviderFormatAdapter(T&& item) : item_(std::forward<T>(item)) {}

  void format(std::ostream& os, std::string options) override {
    FormatProvider<std::decay_t<T>>::format(item_, os, options);
  }

 private:
  T item_;
};

template <typename T>
class StreamOperatorFormatAdapter : public FormatAdapter {
 public:
  explicit StreamOperatorFormatAdapter(T&& item)
      : item_(std::forward<T>(item)) {}

  void format(std::ostream& os, std::string /*options*/) override {
    os << item_;
  }

 private:
  T item_;
};

template <typename T>
class MissingFormatAdapter;

template <typename T, T>
struct SameType;

// FormatProvider should have the signature:
//   static void format(const T&, raw_stream &, StringRef);
template <class T>
class HasFormatProvider {
 public:
  using Decayed = std::decay_t<T>;
  using SignatureFormat = void (*)(const Decayed&, std::ostream&, std::string);

  template <typename U>
  static auto test(SameType<SignatureFormat, &U::format>*) -> char;

  template <typename U>
  static auto test(...) -> double;

  static constexpr bool const Value =
      (sizeof(test<FormatProvider<Decayed>>(nullptr)) == 1);
};

template <class T>
class HasStreamOperator {
 public:
  using ConstRefT = const std::decay_t<T>&;

  template <typename U>
  static auto test(
      std::enable_if_t<std::is_same_v<decltype(std::declval<std::ostream&>()
                                               << std::declval<U>()),
                                      std::ostream&>,
                       int*>) -> char;

  template <typename U>
  static auto test(...) -> double;

  static constexpr bool const Value = (sizeof(test<ConstRefT>(nullptr)) == 1);
};

template <typename T>
struct UsesFormatMember
    : public std::integral_constant<
          bool, std::is_base_of_v<FormatAdapter, std::remove_reference_t<T>>> {
};

template <typename T>
struct UsesFormatProvider
    : public std::integral_constant<bool, !UsesFormatMember<T>::value &&
                                              HasFormatProvider<T>::Value> {};

template <typename T>
struct UsesStreamOperator
    : public std::integral_constant<bool, !UsesFormatMember<T>::value &&
                                              !UsesFormatProvider<T>::value &&
                                              HasStreamOperator<T>::Value> {};

template <typename T>
struct UsesMissingProvider
    : public std::integral_constant<bool, !UsesFormatMember<T>::value &&
                                              !UsesFormatProvider<T>::value &&
                                              !HasStreamOperator<T>::Value> {};

template <typename T>
auto build_format_adapter(T&& item)
    -> std::enable_if_t<UsesFormatMember<T>::value, T> {
  return std::forward<T>(item);
}

template <typename T>
auto build_format_adapter(T&& item)
    -> std::enable_if_t<UsesFormatProvider<T>::value,
                        ProviderFormatAdapter<T>> {
  return ProviderFormatAdapter<T>(std::forward<T>(item));
}

template <typename T>
auto build_format_adapter(T&& item)
    -> std::enable_if_t<UsesStreamOperator<T>::value,
                        StreamOperatorFormatAdapter<T>> {
  return StreamOperatorFormatAdapter<T>(std::forward<T>(item));
}

template <typename T>
auto build_format_adapter(T&& item)
    -> std::enable_if_t<UsesMissingProvider<T>::value,
                        MissingFormatAdapter<T>> {
  return MissingFormatAdapter<T>(std::forward<T>(item));
}

}  // namespace Internal

}  // namespace Formatv

#endif  // FORMATV_FORMAT_VARIADIC_DETAILS_H