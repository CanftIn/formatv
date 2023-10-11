#ifndef FORMATV_FORMAT_VARIADIC_DETAILS_H
#define FORMATV_FORMAT_VARIADIC_DETAILS_H

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

namespace Formatv {

// 这是一个模板结构，充当自定义的格式提供者。
// 用户应该为特定的类型特化这个模板以提供格式化功能。
template <typename T, typename Enable = void>
struct FormatProvider {};

namespace Internal {

// 它是一个抽象基类，定义了一个纯虚函数format。所有适配器类都需要继承这个基类并实现这个函数。
class FormatAdapter {
 public:
  virtual void format(std::ostream& os, std::string options) = 0;

 protected:
  virtual ~FormatAdapter() = default;

 private:
  virtual void anchor() {}
};

// ProviderFormatAdapter 和 StreamOperatorFormatAdapter 类: 
// 这两个类都是FormatAdapter的具体子类。
//
// ProviderFormatAdapter使用FormatProvider为特定类型进行格式化，
// 而StreamOperatorFormatAdapter则使用流插入运算符(<<)为类型进行格式化。
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

// HasFormatProvider 和 HasStreamOperator 类:
// 这两个模板结构用于检查一个给定的类型是否有与FormatProvider或流插入运算符相关的格式化功能。
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

// Uses* 结构:
// 这些结构根据上面的检查，决定哪种适配器应该用于给定的类型。
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

// build_format_adapter 函数模板:
// 这是一个函数模板的重载集合，根据对象的类型选择合适的格式适配器，
// 并将对象传递给适配器进行格式化。它使用SFINAE来选择适当的重载版本，
// 根据对象是否满足不同的格式化要求。
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