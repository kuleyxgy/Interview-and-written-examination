# Cross-MCU Sensor Framework

一个使用纯 C99 编写、面向裸机和 MCU 项目的静态内存 Sensor 框架。首个器件案例为 TMP75 兼容 12 位温度传感器，数据可以经过可组合滤波链，并分别送往 GUI、MQTT 和业务计算模块。

> 当前状态：v0.1.0 首个稳定化版本。Host 模拟、严格编译、分层检查和端到端演示均已通过；真实 MCU Port、硬件在环和 TLS 不在本版本验收范围内。

## 设计目标

- 纯 C99，不依赖 C++。
- 默认不使用 malloc、calloc、realloc 或 free。
- HAL、协议设备和客户端使用 ops + ctx 实例句柄，可同时创建多个实例。
- 温度公共值使用 int32_t 毫摄氏度，不要求 MCU 具备 FPU。
- 滤波器直接引用 ops，可组合且不需要修改核心 switch。
- GUI、MQTT 和业务计算拥有独立有界事件队列。
- Sensor 核心采用单所有者协作式执行模型。
- Sensor 核心是显式 `func_sensor_core_t` 实例，不使用隐式全局注册表。
- 通过脚本和构建目标检查四层依赖。

## 四层架构

| 层级 | 目录 | 可以依赖 |
|---|---|---|
| 功能层 | src/func | 功能层、协议层、工具层 |
| 协议层 | src/proto | 协议层、接口层、工具层 |
| 接口层 | src/iface、src/ports | 接口层、工具层 |
| 工具层 | src/tool | 工具层 |

功能层不得直接包含 hal_、port_ 或平台 SDK 头文件。协议层负责把原始 HAL 能力解释成时钟、寄存器设备、温度器件、显示或 MQTT 等标准服务。

`examples/host_demo/main.c` 是唯一的 composition root，用来实例化并连接各层对象；它不属于任何运行时模块，也不会放宽 `src/func` 的依赖规则。

## 目录

~~~text
src/
├─ tool/                 状态码、字节序、定点数学、环形缓冲、日志
├─ iface/                时间、I2C、网络、UART、SPI、GPIO 接口
├─ proto/                时钟、I2C 寄存器、TMP75、显示、MQTT
├─ func/
│  ├─ filter/            MA、EMA、中值、限幅和滤波链
│  └─ app/               GUI、MQTT、业务和协作式运行时
└─ ports/
   ├─ host_sim/          PC 上的确定性模拟实例
   ├─ template/          新 MCU Port 模板
   └─ stm32f4xx/         STM32F4 对接说明
tests/                   单元与集成测试
examples/host_demo/      双 TMP75 端到端演示
tools/check_layers.py    分层依赖检查
doc/                     设计、变更建议和移植信息
~~~

## 环境要求

- CMake 3.20 或更新版本；
- 支持 C99 的 C 编译器；
- Windows 已验证工具环境：Visual Studio Build Tools 18、MSVC 19.51、CMake 4.3.1；
- Python 3.10 或更新版本，用于分层检查。

## Windows 构建

~~~powershell
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

& $cmake -S . -B build -G 'Visual Studio 18 2026' -A x64 -DSENSOR_PORT=host
& $cmake --build build --config Debug
~~~

主要构建 target 与层级一一对应：

| Target | 所属内容 |
|---|---|
| `sensor_tool` | 工具层 |
| `sensor_iface` | 标准 HAL 接口 |
| `sensor_port_host` | Host 接口层 Port |
| `sensor_proto` | 协议层 |
| `sensor_func` | 功能层 |
| `sensor_framework` | 由装配方选择 Port 的聚合接口 target |

MCU 工程可使用 `-DSENSOR_PORT=none -DBUILD_TESTING=OFF -DSENSOR_BUILD_EXAMPLES=OFF`，只构建可移植的 tool/proto/func 库，再链接自己的接口层 Port。模板可单独编译检查：

~~~powershell
& $cmake --build build --config Debug --target check_port_template
~~~

## 测试

配置时默认启用测试。当前共有 40 个 C 测试函数，并注册 12 个 CTest 目标：tool、iface、proto、filters、sensor、apps、mqtt、e2e、integration、host_demo_e2e、layers 和 layers_negative。

~~~powershell
$ctest = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'

& $ctest --test-dir build -C Debug --output-on-failure
& $cmake --build build --config Debug --target check_layers
~~~

已验证环境的结果为 12/12 通过，MSVC `/W4 /WX` 为 0 警告、0 错误。

## 演示

构建后运行：

~~~powershell
.\build\Debug\host_demo.exe
~~~

host_demo 已实现：

1. 在两条独立 host I2C 总线上创建相同地址的 TMP75；
2. 为两个 Sensor 配置不同滤波链；
3. 将事件复制到 GUI、MQTT 和业务队列；
4. 注入 I2C IO/TIMEOUT，使质量经历 VALID → STALE → ERROR → VALID；
5. 完成 MQTT CONNECT/CONNACK/QoS 0 PUBLISH，输出报文十六进制；
6. 注入 MQTT 断线、离线排队、重连和队列溢出；
7. 输出 GUI、业务窗口/迟滞报警和 dropped 统计后确定性退出。

成功运行的摘要行类似：`[SUMMARY] GUI_records=11 MQTT_pending=0 MQTT_dropped=1`。

## 最小装配顺序

以下代码展示核心 API 的连接方式；对象、队列存储和滤波状态均由调用方静态持有：

~~~c
func_sensor_core_t core;
func_temp_t temperature;
func_sensor_registration_t registration = {
    1U, "board-temp", &func_temp_driver_ops, &temperature
};

func_sensor_core_init(&core);
func_temp_configure(&temperature, &temperature_cfg);
func_event_queue_init(&gui_queue, gui_storage, GUI_CAPACITY,
                      FUNC_QUEUE_DROP_NEWEST);
func_sensor_register(&core, &registration);
func_sensor_subscribe(&core, 1U, &gui_queue);

/* 在唯一 owner 的主循环或 Sensor 任务中调用。 */
func_sensor_poll_all(&core, now_ms);
func_app_gui_poll(&gui_app, now_ms);
~~~

完整可运行装配见 `examples/host_demo/main.c`。

## 核心约定

### 数值

- 温度单位：毫摄氏度；
- 25000 表示 25.000℃；
- TMP75 的 0.0625℃ LSB 转换到整数毫摄氏度时，半毫度远离零舍入；
- 滤波内部乘加优先使用 int64_t 并在输出时饱和到 int32_t。

### 内存

- 队列、滤波状态、协议工作区和驱动上下文由调用方静态提供；
- 每个带缓冲区的 API 同时接收容量；
- 对象的 ctx 生命周期必须覆盖其句柄和所有使用者；
- 不允许隐式的“当前总线”或“当前 MQTT 客户端”。

### 线程模型

- Sensor 注册、轮询、发布和应用消费由一个主循环或一个 RTOS Sensor 任务拥有；
- 首版核心不创建任务和互斥锁；
- 多任务项目应把操作投递给唯一 Sensor 任务；
- 首版事件队列不承诺跨线程安全。

## 添加 MCU Port

1. 复制 src/ports/template；
2. 为启用的 HAL 创建平台 ctx、ops 和实例句柄；
3. 将厂商 SDK 类型限制在 Port 的 .c 文件中；
4. 在协议层装配总线、器件地址和协议设备；
5. 运行分层检查；
6. 在目标板验证超时、负温度、断线和时间回绕。

接口层只处理原始硬件事务。TMP75 寄存器格式、MQTT 报文或 GUI 语义不能出现在 Port 中。

## 添加温度协议

实现 proto_temp_device_t 的 init 和 read_mdeg_c ops，把器件私有上下文放在协议层，然后把实例传给 func_temp。功能层不应知道器件使用 I2C、SPI、ADC 还是远程总线。

## 添加滤波器

1. 在 src/func/filter 新增独立 .h/.c；
2. 定义配置和调用方拥有的状态类型；
3. 实现 init、reset、process；
4. 导出 const func_filter_ops_t；
5. 在目标 Sensor 的静态滤波链中直接引用该 ops。

不需要修改中央枚举、注册表或 switch。

## MQTT 首版范围

- MQTT 3.1.1；
- QoS 0 publish；
- 调用方提供的固定容量发送队列；
- 断线排队和有上限的重连退避；
- keepalive；
- 不包含 TLS、QoS 1/2、subscribe 和持久会话。

TLS 后续应通过安全的 hal_net Port 接入，而不是耦合进 MQTT 报文核心。

## 状态与限制

- 当前 Host 参考实现已经过单元、集成、错误注入、端到端测试和独立代码复核；
- STM32F4 目录是 SDK 对接边界说明，不代表已经完成硬件在环验证；
- MQTT 首版只处理本客户端所需的 CONNACK 和 PINGRESP 入站控制包，不提供订阅；
- `budget_ms` 通过每次 poll 最多一个非阻塞 HAL 工作单元，并将 connect timeout 限制到预算内；具体 Port 必须遵守非阻塞 send/recv 契约；
- 本版本不是功能安全认证组件，投产前仍需目标 MCU、编译器、网络栈和硬件在环验证。

详细设计见 doc/sensor_framework_design_20260809_182444.md。

下一版本建议见 doc/NEXT_VERSION_RECOMMENDATIONS.md。
