// Task 4 B6: 内置插件的注册入口。
//
// 定义在**插件侧**而不是宿主侧，这样依赖方向始终是「插件 → 宿主」，
// 宿主库不必知道有哪些插件实现。
//
// 为什么不用「静态初始化器自动注册」：静态库里只有初始化器的目标文件
// 不会被链接器拉进来，注册会**静默丢失**。故由宿主显式调用本函数。

#include "loongbones/importers/host/import_host.hpp"

#include <memory>

#include "loongbones/importers/spine32/spine32_import_plugin.hpp"

namespace loongbones::importers::host {

void registerBuiltinPlugins(ImportRegistry& registry) {
  registry.registerPlugin(std::make_unique<spine32::Spine32ImportPlugin>());
}

}  // namespace loongbones::importers::host
