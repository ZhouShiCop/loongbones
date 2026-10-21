// 测试入口。运行全部用例并输出通过/失败/跳过数量（规范 8.4: 须报告数量）。

#include "test_framework.hpp"
#include <cstdio>
#include <iostream>

int main() {
  // 无缓冲输出：进程崩溃时缓冲内容会丢失，导致无法定位到具体用例。
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::cout << "loongbones core model tests (domain, Qt-free)\n";
  return lbt::TestRegistry::instance().runAll();
}