#ifndef FORMATV_FORMAT_H
#define FORMATV_FORMAT_H

#include <cassert>
#include <cstdio>
#include <tuple>
#include <utility>

namespace Formatv {

class FormatObjectBase {
 public:
  FormatObjectBase(const char* fmt) : fmt_(fmt) {}

  auto Print(char* buffer, unsigned buffer_size) const -> unsigned {
    assert(buffer_size && "Invalid buffer size!");
    int n = snprint(buffer, buffer_size);
    // If the buffer is too small, double the size.
    if (n < 0) {
      return buffer_size * 2;
    }
    if (unsigned(n) >= buffer_size) {
      return n + 1;
    }
    return n;
  }

 protected:
  FormatObjectBase(const FormatObjectBase&) = default;
  ~FormatObjectBase() = default;
  //virtual auto home() -> void;

  /// Call `snprintf()` with the given buffer and buffer size.
  virtual auto snprint(char* buffer, unsigned buffer_size) const
      -> int = 0;

  const char* fmt_;
};

namespace Internal {

template <typename... Args>
struct ValidateFormatParameters;
template <typename Arg, typename... Args>
struct ValidateFormatParameters<Arg, Args...> {
  static_assert(std::is_scalar_v<Arg>,
                "format can't be used with non fundamental / non pointer type");
  ValidateFormatParameters() { ValidateFormatParameters<Args...>(); }
};
template <>
struct ValidateFormatParameters<> {};

}  // namespace Internal

template <typename... Ts>
class FormatObject final : public FormatObjectBase {
 public:
  FormatObject(const char* fmt, const Ts&... args)
      : FormatObjectBase(fmt), args_(args...) {
    Internal::ValidateFormatParameters<Ts...>();
  }

  auto snprint(char* buffer, unsigned buffer_size) const -> int override {
    return snprint_tuple(buffer, buffer_size, std::index_sequence_for<Ts...>());
  }

 private:
  template <std::size_t... Is>
  auto snprint_tuple(char* buffer, unsigned buffer_size,
                     std::index_sequence<Is...> /*unused*/) const -> int {
    return std::snprintf(buffer, buffer_size, fmt_, std::get<Is>(args_)...);
  }

  std::tuple<Ts...> args_;
};

template <typename... Ts>
inline auto format(const char* fmt, const Ts&... args) -> FormatObject<Ts...> {
  return FormatObject<Ts...>(fmt, args...);
}

}  // namespace Formatv

#endif  // FORMATV_FORMAT_H