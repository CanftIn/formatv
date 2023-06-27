#include <iostream>

#include "Format.h"
#include "FormatVariadic.h"

void test_format() {
  double myfloat = 12.3456789;
  char buffer[256];
  auto fmt = Formatv::format("%0.4f", myfloat);
  fmt.Print(buffer, sizeof(buffer));
  std::cout << buffer << '\n';
}

void test_formatv_parse() {
  const std::string format_string = "This is a test";
  auto replacements =
      Formatv::FormatvObjectBase::ParseFormatString(format_string);
  std::cout << replacements.size() << '\n';
  std::cout << replacements[0].spec << '\n';

  std::cout << Formatv::formatv("{0}", 1).str() << '\n';
  std::cout << Formatv::formatv("{0}", 'c').str() << '\n';
  std::cout << Formatv::formatv("{0}", -3).str() << '\n';
  std::cout << Formatv::formatv("{0}", "Test").str() << '\n';
  std::cout << Formatv::formatv("{0}", std::string("Test2")).str() << '\n';
  std::cout << Formatv::formatv("{0} {1}", 1234.412, "test").str() << '\n';

  std::cout << Formatv::formatv("{0:N}", 1234567890).str() << '\n';

  std::cout << Formatv::formatv("{0,=+5}", 123).str() << '\n';
}

auto main() -> int {
  test_format();
  test_formatv_parse();
  return 0;
}