#include <iostream>
#include <cstdlib>

#include "vec.h"

template<typename T>
void test(const char* const op_desc, T result, T expected) {
  if (result != expected) {
    std::cerr << op_desc << ":\n";
    std::cerr << "       got: " << result << '\n';
    std::cerr << "  expected: " << expected << std::endl;
  }
}

template<typename T, size_t N>
std::ostream& operator<<(std::ostream& os, Vec<T, N> v) {
  os << '<';
  bool first = true;
  for (int x : v) {
    if (!first) os << ", ";
    os << x;
    first = false;
  }
  return os << '>';
}

#define TEST(op, expect) test(#op, op, expect)

int main() {
  TEST(Vec(1, 1) + Vec(1.0, 1.0), Vec(2.0, 2.0));
  TEST(Vec(1, 1) - Vec(1.0, 1.0), Vec(0.0, 0.0));
  TEST(Vec(1, 1, 1) - Vec(1, 1, 1), Vec(0, 0, 0));
  TEST(Vec(1, 2, 3) * 2, Vec(2, 4, 6));
  TEST(Vec(1, 2, 3) / 2, Vec(0, 1, 1));
  TEST(2 / Vec(1, 2, 3), Vec(2, 1, 0));
  TEST(magnitude_squared(Vec(3,4)), 5 * 5);
  TEST(magnitude(Vec(3,4)), 5);
}
