# Windows 一键构建脚本设计

## 目标

在仓库根目录提供一个 PowerShell 主脚本和一个可双击的 CMD 入口，使 Windows 用户无需手工输入 CMake、CTest 和演示命令即可构建本项目。

## 交付文件

- `build.ps1`：唯一的构建逻辑实现，支持命令行参数并返回可靠的进程退出码。
- `build.cmd`：调用同目录的 `build.ps1`、透传参数、显示最终退出码并暂停窗口，方便双击使用。
- `tests/tool/test_build_script.ps1`：验证参数契约、错误退出和实际构建流程。

## `build.ps1` 接口

~~~powershell
.\build.ps1 [-Configuration Debug|Release]
              [-BuildDir <relative-path>]
              [-Port host|none]
              [-Clean]
              [-Demo]
~~~

默认值：

- `Configuration=Debug`
- `BuildDir=build`
- `Port=host`
- 默认执行配置、编译和全部测试；默认不运行演示。

行为：

1. 先从 PATH 查找 `cmake`，找不到时使用本机已验证的 Visual Studio Build Tools 18 CMake 路径。
2. `Port=host` 时启用测试和 host demo，使用 `Visual Studio 18 2026`、x64 生成器。
3. `Port=none` 时关闭测试和示例，只构建可移植的 tool/proto/func 库；此模式拒绝 `-Demo`。
4. CMake 配置成功后执行所选配置的构建。
5. host 模式构建完成后运行 CTest，并在任一测试失败时返回非零退出码。
6. 指定 `-Demo` 时，在测试通过后运行 `host_demo.exe`。
7. 每个阶段打印清晰的中文标题和最终成功/失败信息。

## 清理安全

`-Clean` 只允许删除仓库根目录下由 `BuildDir` 精确解析出的目录。脚本必须拒绝空路径、仓库根目录、绝对路径、父目录逃逸和仓库外目标。未指定 `-Clean` 时不删除任何内容。

## `build.cmd` 行为

双击 `build.cmd` 等价于运行默认的 `build.ps1`：配置、编译和测试。CMD 入口使用 `-NoProfile -ExecutionPolicy Bypass`，把 `%*` 原样传给 PowerShell，保存退出码，显示结果并执行 `pause`，最后用同一退出码退出。

高级使用者直接调用 `build.ps1`，避免 CMD 的暂停行为。

## 错误处理

- 不自动安装工具、不访问网络、不执行 Git 操作。
- CMake、编译、CTest 或 demo 任一步非零退出时立即停止并返回该退出码。
- 找不到 CMake/CTest 或 demo 时给出可操作的错误信息。
- 构建目录和可执行文件路径始终相对脚本所在的仓库根目录解析，不依赖调用者当前目录。

## 测试与验收

严格采用 RED→GREEN：

1. 先创建脚本契约测试；在 `build.ps1`/`build.cmd` 不存在时确认测试因缺少交付文件而失败。
2. 实现最小脚本后，运行契约测试并确认参数与危险清理路径检查通过。
3. 在仓库外的调用工作目录执行 `build.ps1 -BuildDir build-script-test`，确认仍能正确定位源码。
4. 验证默认 host 构建和 12/12 CTest 通过。
5. 验证 `-Demo` 正常退出并输出 `[SUMMARY]`。
6. 验证 `-Port none -Configuration Release` 构建通过且不生成 host demo。
7. 验证非法 `-Port none -Demo` 和仓库外 `-Clean` 返回非零，且仓库外文件保持不变。

## 非目标

- 不封装 MCU 厂商 SDK 或烧录流程。
- 不下载 CMake、编译器或依赖。
- 不提供 Linux shell 脚本。
- 不修改现有 CMake target 或框架运行时代码。
