// 极简测试框架。零第三方依赖（规范 8.3: 新增依赖前须论证）。
// 需求只有断言 + 统计 + 分组, 故自行实现而不引入 Catch2/GoogleTest。
// 用法见 Loongbones/tools/core-tests/README.md。

#ifndef LOONGBONES_TESTS_FRAMEWORK_HPP
#define LOONGBONES_TESTS_FRAMEWORK_HPP

#include <string>
#include <vector>

namespace lbt {

struct AssertionFailure {
  std::string message;
};
class TestRegistry {
 public:
  static TestRegistry& instance();
  void add(std::string suite, std::string name, void (*fn)());
  int runAll();
  int passed() const { return passed_; }
  int failed() const { return failed_; }

 private:
  struct Entry { std::string suite; std::string name; void (*fn)(); };
  std::vector<Entry> entries_;
  int passed_ = 0;
  int failed_ = 0;
};

struct Registrar {
  Registrar(const char* suite, const char* name, void (*fn)()) {
    TestRegistry::instance().add(suite, name, fn);
  }
};

}  // namespace lbt

#define LBT_CONCAT_(a, b) a##b
#define LBT_CONCAT(a, b) LBT_CONCAT_(a, b)
/// 定义一个测试用例
#define TEST(suite, name)                                               \
  static void LBT_CONCAT(lbt_fn_, __LINE__)();                         \
  static ::lbt::Registrar LBT_CONCAT(lbt_reg_, __LINE__)(              \
      #suite, #name, &LBT_CONCAT(lbt_fn_, __LINE__));                 \
  static void LBT_CONCAT(lbt_fn_, __LINE__)()

#define LBT_FAIL(msg)                                                  \
  throw ::lbt::AssertionFailure{std::string(__FILE__) + ":" +          \
                                std::to_string(__LINE__) + ": " + (msg)}
#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) LBT_FAIL(std::string("CHECK failed: ") + #cond);       \
  } while (false)

#define CHECK_EQ(a, b)                                                 \
  do {                                                                 \
    if (!((a) == (b))) {                                               \
      LBT_FAIL(std::string("CHECK_EQ failed: ") + #a + " == " + #b);  \
    }                                                                  \
  } while (false)

#endif  // LOONGBONES_TESTS_FRAMEWORK_HPP