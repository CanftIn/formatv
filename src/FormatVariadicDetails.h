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
  virtual void anchor();
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

}  // namespace Internal

}  // namespace Formatv

#endif  // FORMATV_FORMAT_VARIADIC_DETAILS_H