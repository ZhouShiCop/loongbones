#include "tests/domain/test_framework.hpp"

#include <cstdio>
#include <iostream>

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::cout << "loongbones Task 4 B1 atlas importer tests\n";
  return lbt::TestRegistry::instance().runAll();
}
