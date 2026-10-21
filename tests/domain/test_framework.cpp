#include "test_framework.hpp"

#include <exception>
#include <iostream>

namespace lbt {

TestRegistry& TestRegistry::instance() {
  static TestRegistry reg;
  return reg;
}

void TestRegistry::add(std::string suite, std::string name, void (*fn)()) {
  entries_.push_back(Entry{std::move(suite), std::move(name), fn});
}
int TestRegistry::runAll() {
  std::string lastSuite;
  for (const Entry& e : entries_) {
    if (e.suite != lastSuite) {
      lastSuite = e.suite;
      std::cout << "\n[" << lastSuite << "]\n";
    }
    try {
      e.fn();
      ++passed_;
      std::cout << "  PASS " << e.name << "\n";
    } catch (const AssertionFailure& f) {
      ++failed_;
      std::cout << "  FAIL " << e.name << "\n        " << f.message << "\n";
    } catch (const std::exception& ex) {
      ++failed_;
      std::cout << "  FAIL " << e.name << "  exception: " << ex.what() << "\n";
    }
  }  std::cout << "\n----------------------------------------\n";
  std::cout << "tests " << (passed_ + failed_) << " | pass " << passed_ << " | fail " << failed_
            << "\n";
  return failed_ == 0 ? 0 : 1;
}

}  // namespace lbt