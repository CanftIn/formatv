#ifndef FORMATV_FORMAT_VARIADIC_H
#define FORMATV_FORMAT_VARIADIC_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "FormatAlign.h"
#include "FormatUtil.h"
#include "FormatVariadicDetails.h"

namespace Formatv {

enum class ReplacementType : uint8_t {
  Empty,
  Format,
  Literal,
};

struct ReplacementItem {
  ReplacementItem() = default;
  explicit ReplacementItem(std::string literal)
      : type(ReplacementType::Literal), spec(std::move(literal)) {}
  ReplacementItem(std::string spec, size_t index, size_t align,
                  AlignStyle where, char pad, std::string options)
      : type(ReplacementType::Format),
        spec(std::move(spec)),
        index(index),
        align(align),
        where(where),
        pad(pad),
        options(std::move(options)) {}

  ReplacementType type = ReplacementType::Empty;
  std::string spec;
  size_t index = 0;
  size_t align = 0;
  AlignStyle where = AlignStyle::Right;
  char pad = 0;
  std::string options;
};

class FormatvObjectBase;

auto operator<<(std::ostream& os, const FormatvObjectBase& obj)
    -> std::ostream&;

class FormatvObjectBase {
 public:
  FormatvObjectBase(const FormatvObjectBase&) = delete;
  auto operator=(const FormatvObjectBase&) -> FormatvObjectBase& = delete;

  void format(std::ostream& os) const {
    for (auto& r : ParseFormatString(fmt_)) {
      switch (r.type) {
        case ReplacementType::Empty:
          continue;
        case ReplacementType::Literal:
          os << r.spec;
          continue;
        case ReplacementType::Format: {
          if (r.index >= adapters_.size()) {
            os << r.spec;
            continue;
          }

          auto* w = adapters_[r.index];
          FormatAlign align(*w, r.where, r.align, r.pad);
          align.format(os, r.options);
        }
        default:
          continue;
      }
    }
  }

  static auto ParseFormatString(std::string fmt)
      -> std::vector<ReplacementItem> {
    std::vector<ReplacementItem> replacements;
    ReplacementItem i;
    while (!fmt.empty()) {
      std::tie(i, fmt) = SplitLiteralAndReplacement(fmt);
      if (i.type != ReplacementType::Empty) {
        replacements.push_back(i);
      }
    }
    return replacements;
  }

  static auto ParseReplacementItem(std::string spec)
      -> std::optional<ReplacementItem> {
    std::string rep_string = FormatUtil::trim(spec, "{}");

    char pad = ' ';
    std::size_t align = 0;
    AlignStyle where = AlignStyle::Right;
    std::string options;
    size_t index = 0;

    rep_string = FormatUtil::trim(rep_string);
    if (FormatUtil::ConsumeInteger(rep_string, 0, index)) {
      assert(false && "Invalid replacement sequence index!");
      return ReplacementItem{};
    }

    rep_string = FormatUtil::trim(rep_string);
    if (!rep_string.empty() && rep_string.front() == ',') {
      rep_string = FormatUtil::drop_front(rep_string, 1);
      if (!ConsumeFieldLayout(rep_string, where, align, pad)) {
        assert(false && "Invalid replacement field layout specification!");
      }
    }

    rep_string = FormatUtil::trim(rep_string);
    if (!rep_string.empty() && rep_string.front() == ':') {
      rep_string = FormatUtil::drop_front(rep_string, 1);
      options = FormatUtil::trim(rep_string);
      rep_string = "";
    }

    rep_string = FormatUtil::trim(rep_string);
    if (!rep_string.empty()) {
      assert(false && "Unexpected characters found in replacement string!");
    }

    return ReplacementItem{spec, index, align, where, pad, options};
  }

  auto str() const -> std::string {
    std::ostringstream stream;
    stream << *this;
    std::string result = stream.str();
    stream.flush();
    return result;
  }

  operator std::string() const { return str(); }

 protected:
  FormatvObjectBase(std::string fmt,
                    ArrayRef<Internal::FormatAdapter*> adapters)
      : fmt_(std::move(fmt)), adapters_(adapters.begin(), adapters.end()) {}

  FormatvObjectBase(FormatvObjectBase&&) = default;

  static auto ConsumeFieldLayout(std::string& spec, AlignStyle& where,
                                 size_t& align, char& pad) -> bool {
    where = AlignStyle::Right;
    align = 0;
    pad = ' ';
    if (spec.empty()) {
      return true;
    }

    if (spec.size() > 1) {
      if (auto loc = FormatUtil::TranslateLocChar(spec[1])) {
        pad = spec[0];
        where = *loc;
        spec = FormatUtil::drop_front(spec, 2);
      } else if (auto loc = FormatUtil::TranslateLocChar(spec[0])) {
        where = *loc;
        spec = FormatUtil::drop_front(spec, 1);
      }
    }

    bool failed = FormatUtil::ConsumeInteger(spec, 0, align);
    return !failed;
  }

  static auto SplitLiteralAndReplacement(std::string fmt)
      -> std::pair<ReplacementItem, std::string> {
    while (!fmt.empty()) {
      if (fmt.front() != '{') {
        std::size_t bo = FormatUtil::find_first_of(fmt, '{');
        return std::make_pair(ReplacementItem{FormatUtil::substr(fmt, 0, bo)},
                              FormatUtil::substr(fmt, bo));
      }

      std::string braces =
          FormatUtil::take_while(fmt, [](char c) { return c == '{'; });
      if (braces.size() > 1) {
        size_t num_excaped_braces = braces.size() / 2;
        std::string middle = FormatUtil::take_front(fmt, num_excaped_braces);
        std::string right = FormatUtil::drop_front(fmt, num_excaped_braces * 2);
        return std::make_pair(ReplacementItem{middle}, right);
      }

      std::size_t bc = FormatUtil::find_first_of(fmt, '}');
      if (bc == std::string::npos) {
        assert(false &&
               "Unterminated brace sequence.  Escape with {{ for a literal "
               "brace.");
        return std::make_pair(ReplacementItem{fmt}, std::string());
      }

      std::size_t bo2 = FormatUtil::find_first_of(fmt, '{', 1);
      if (bo2 < bc) {
        return std::make_pair(ReplacementItem{FormatUtil::substr(fmt, 0, bo2)},
                              FormatUtil::substr(fmt, bo2));
      }

      std::string spec = FormatUtil::slice(fmt, 1, bc);
      std::string right = FormatUtil::substr(fmt, bc + 1);

      auto ri = ParseReplacementItem(spec);
      if (ri) {
        return std::make_pair(*ri, right);
      }

      fmt = FormatUtil::drop_front(fmt, bc + 1);
    }
    return std::make_pair(ReplacementItem{fmt}, std::string());
  }

  std::string fmt_;

  ArrayRef<Internal::FormatAdapter*> adapters_;
};

inline auto operator<<(std::ostream& os, const FormatvObjectBase& obj)
    -> std::ostream& {
  obj.format(os);
  return os;
}

template <typename Tuple>
class FormatvObject : public FormatvObjectBase {
 public:
  FormatvObject(std::string fmt, Tuple&& params)
      : FormatvObjectBase(fmt, parameter_pointers_),
        parameters_(std::move(params)),
        parameter_pointers_(std::apply(CreateAdapters(), parameters_)) {}

  FormatvObject(const FormatvObject& rhs) = delete;

  FormatvObject(FormatvObject&& rhs)
      : FormatvObjectBase(std::move(rhs)),
        parameters_(std::move(rhs.parameters_)) {
    parameter_pointers_ = std::apply(CreateAdapters(), parameters_);
    adapters_ = parameter_pointers_;
  }

 private:
  struct CreateAdapters {
    template <typename... Ts>
    auto operator()(Ts&... items)
        -> std::array<Internal::FormatAdapter*, std::tuple_size<Tuple>::value> {
      return {{&items...}};
    }
  };

  Tuple parameters_;

  std::array<Internal::FormatAdapter*, std::tuple_size<Tuple>::value>
      parameter_pointers_;
};

///   // Convert to std::string.
///   std::string S = formatv("{0} {1}", 1234.412, "test").str();
///
///   OS << formatv("{0} {1}", 1234.412, "test");
template <typename... Ts>
inline auto formatv(const char* fmt, Ts&&... vals)
    -> FormatvObject<decltype(std::make_tuple(
        Internal::build_format_adapter(std::forward<Ts>(vals))...))> {
  using ParamTuple = decltype(std::make_tuple(
      Internal::build_format_adapter(std::forward<Ts>(vals))...));
  return FormatvObject<ParamTuple>(
      fmt, std::make_tuple(
               Internal::build_format_adapter(std::forward<Ts>(vals))...));
}

}  // namespace Formatv

#endif  // FORMATV_FORMAT_VARIADIC_H