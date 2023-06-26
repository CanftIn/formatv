#include <iostream>

#include "Format.h"

auto main() -> int {
  double myfloat = 12.3456789;
  char buffer[256];
  auto fmt = Formatv::format("%0.4f", myfloat);
  fmt.Print(buffer, sizeof(buffer));
  std::cout << buffer << '\n';

  return 0;
}