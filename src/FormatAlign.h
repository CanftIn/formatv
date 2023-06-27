#ifndef FORMATV_FORMAT_ALIGN_H
#define FORMATV_FORMAT_ALIGN_H

#include <sstream>

#include "FormatVariadicDetails.h"

namespace Formatv {

enum class AlignStyle : uint8_t {
  Left,    // "-"
  Center,  // "="
  Right,   // "+"
};

struct FormatAlign {
  Internal::FormatAdapter& adapter_;
  AlignStyle where_;
  size_t amount_;
  char fill_;

  FormatAlign(Internal::FormatAdapter& adapter, AlignStyle where, size_t amount,
              char fill = ' ')
      : adapter_(adapter), where_(where), amount_(amount), fill_(fill) {}

  void format(std::ostream& os, std::string options) {
    if (amount_ == 0) {
      adapter_.format(os, options);
      return;
    }

    std::ostringstream stream;

    adapter_.format(stream, options);

    std::string item = stream.str();
    if (amount_ <= item.size()) {
      os << item;
      return;
    }

    size_t pad_amount = amount_ - item.size();
    switch (where_) {
      case AlignStyle::Left:
        os << item;
        fill(os, pad_amount);
        break;
      case AlignStyle::Center: {
        size_t x = pad_amount / 2;
        fill(os, x);
        os << item;
        fill(os, pad_amount - x);
        break;
      }
      default:
        fill(os, pad_amount);
        os << item;
        break;
    }
  }

 private:
  void fill(std::ostream& os, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
      os << fill_;
    }
  }
};

}  // namespace Formatv

#endif  // FORMATV_FORMAT_ALIGN_H