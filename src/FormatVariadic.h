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

// 格式化字符串中的替换操作类型。
enum class ReplacementType : uint8_t {
  Empty, // 表示没有替换。
  Format, // 表示应该格式化和替换的项目。
  Literal, // 表示应原样插入的字符串字面量。
};

// 保存每个替换项的格式说明详情。
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

  // 替换的类型。
  ReplacementType type = ReplacementType::Empty;
  // 来自格式的原始字符串。
  std::string spec;
  // 要替换的值的索引。
  size_t index = 0;
  // align, where, pad: 对齐的规格说明。
  size_t align = 0; // 对齐大小。
  AlignStyle where = AlignStyle::Right; // 对齐样式。
  char pad = 0; // 填充字符。
  // 替换项的其他格式选项。
  std::string options;
};

class FormatvObjectBase;

auto operator<<(std::ostream& os, const FormatvObjectBase& obj)
    -> std::ostream&;

// 用于格式化对象的基类。
class FormatvObjectBase {
 public:
  FormatvObjectBase(const FormatvObjectBase&) = delete;
  auto operator=(const FormatvObjectBase&) -> FormatvObjectBase& = delete;

  // 根据替换项格式化字符串并将其写入给定的ostream。
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

  // 解析格式字符串以获取替换项列表。
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

  // 将单个替换规格解析为ReplacementItem。
  static auto ParseReplacementItem(std::string spec)
      -> std::optional<ReplacementItem> {
    // 移除 spec 字符串的 { 和 }。
    std::string rep_string = FormatUtil::trim(spec, "{}");

    char pad = ' ';
    std::size_t align = 0;
    AlignStyle where = AlignStyle::Right;
    std::string options;
    size_t index = 0;

    // 移除 rep_string 的前后空白字符。
    rep_string = FormatUtil::trim(rep_string);
    // 尝试从 rep_string 开始的位置解析一个整数，并将其赋值给 index。
    if (FormatUtil::ConsumeInteger(rep_string, 0, index)) {
      assert(false && "Invalid replacement sequence index!");
      return ReplacementItem{};
    }

    rep_string = FormatUtil::trim(rep_string);
    // 第一个字符为 `,`，它将尝试解析字段布局。
    // `,`: 通常是用于字段布局或对齐的指示符。
    // 例如，`{0,10}`，其中 0 是要替换的参数索引，
    // 10 是指示字段宽度或对齐的数字。`,` 符号在此处用作分隔符。
    if (!rep_string.empty() && rep_string.front() == ',') {
      rep_string = FormatUtil::drop_front(rep_string, 1);
      if (!ConsumeFieldLayout(rep_string, where, align, pad)) {
        assert(false && "Invalid replacement field layout specification!");
      }
    }

    rep_string = FormatUtil::trim(rep_string);
    // 第一个字符是否为 `:`，它会从 rep_string 中提取选项字符串。
    // `:`: 这通常是用于格式选项的指示符。
    // 例如， `{0:0.00}`，其中 0 是要替换的参数索引，0.00 是指示
    // 如何格式化数字的选项（例如，始终显示两位小数）。
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

  // 返回格式化的字符串。
  auto str() const -> std::string {
    std::ostringstream stream;
    stream << *this;
    std::string result = stream.str();
    stream.flush();
    return result;
  }

  // 将对象转换为字符串。
  operator std::string() const { return str(); }

 protected:
  FormatvObjectBase(std::string fmt,
                    ArrayRef<Internal::FormatAdapter*> adapters)
      : fmt_(std::move(fmt)), adapters_(adapters.begin(), adapters.end()) {}

  FormatvObjectBase(FormatvObjectBase&&) = default;

  // 解析对齐、填充和宽度规格。
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

  // 从输入的 fmt 字符串中分离字面量和替换项。
  // 即它寻找 `{...}` 结构中的替换项，并将其与其前面的字面量一起返回。
  // 如果找到一个连续的 `{` 或者 `{{`，它将按照适当的逻辑对其进行处理。
  static auto SplitLiteralAndReplacement(std::string fmt)
      -> std::pair<ReplacementItem, std::string> {
    while (!fmt.empty()) {
      // 处理没有 { 开头的字符串。
      if (fmt.front() != '{') {
        std::size_t bo = FormatUtil::find_first_of(fmt, '{');
        return std::make_pair(ReplacementItem{FormatUtil::substr(fmt, 0, bo)},
                              FormatUtil::substr(fmt, bo));
      }

      // 处理连续的 { 字符。
      // 如果找到一个或多个 {，它会尝试获取连续的 { 个数，并将其保存在 braces 中。
      std::string braces =
          FormatUtil::take_while(fmt, [](char c) { return c == '{'; });
      // 如果连续的 `{` 个数大于1（即 `{{`），它将其解释为转义字符，
      // 并只保留其中一半作为字面量返回。剩下的部分被视为后续的字符串。
      if (braces.size() > 1) {
        size_t num_excaped_braces = braces.size() / 2;
        std::string middle = FormatUtil::take_front(fmt, num_excaped_braces);
        std::string right = FormatUtil::drop_front(fmt, num_excaped_braces * 2);
        return std::make_pair(ReplacementItem{middle}, right);
      }

      // 查找匹配的 }。
      std::size_t bc = FormatUtil::find_first_of(fmt, '}');
      if (bc == std::string::npos) {
        assert(false &&
               "Unterminated brace sequence.  Escape with {{ for a literal "
               "brace.");
        return std::make_pair(ReplacementItem{fmt}, std::string());
      }

      // 查找嵌套的 {。
      std::size_t bo2 = FormatUtil::find_first_of(fmt, '{', 1);
      if (bo2 < bc) {
        return std::make_pair(ReplacementItem{FormatUtil::substr(fmt, 0, bo2)},
                              FormatUtil::substr(fmt, bo2));
      }

      // 处理格式说明符。
      // 在 { 和 } 之间的字符串被视为替换项的格式说明符。
      std::string spec = FormatUtil::slice(fmt, 1, bc);
      std::string right = FormatUtil::substr(fmt, bc + 1);
      // 调用 ParseReplacementItem 函数来解析这个说明符。
      auto ri = ParseReplacementItem(spec);
      // 解析成功，它返回解析得到的 ReplacementItem 和 } 之后的字符串。
      if (ri) {
        return std::make_pair(*ri, right);
      }

      // 上述所有情况都没有返回结果，函数将删除 fmt 中到 bc 位置之前的所有字符，并继续循环。
      fmt = FormatUtil::drop_front(fmt, bc + 1);
    }
    // 遍历完整个 fmt 字符串仍然没有找到任何替换项，它将返回整个字符串作为字面量。
    return std::make_pair(ReplacementItem{fmt}, std::string());
  }

  std::string fmt_;

  ArrayRef<Internal::FormatAdapter*> adapters_;
};

// 允许直接将格式化的结果流式传输到输出流。
inline auto operator<<(std::ostream& os, const FormatvObjectBase& obj)
    -> std::ostream& {
  obj.format(os);
  return os;
}

// 表示具有特定参数的格式化操作的模板类。
// 它捕获格式字符串和作为元组的格式化值。
// 保存每个参数的格式适配器的指针，这些适配器知道如何格式化该特定类型。
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
  // 创建格式适配器。
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

///   // 用户创建格式化字符串的主要接口。
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