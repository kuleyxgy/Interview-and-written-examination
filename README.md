# Cross-MCU Sensor Framework

一个使用纯 C99 编写、面向裸机和 MCU 项目的静态内存 Sensor 框架。首个器件案例为 TMP75 兼容 12 位温度传感器，数据可以经过可组合滤波链，并分别送往 GUI、MQTT 和业务计算模块。

> 当前分支状态：v0.1 Alpha 初始实现。首轮代码先推送到 feature/sensor-framework-v0.1，完整测试、问题复现和稳定化修复将在后续提交中完成。请勿把当前 Alpha 直接用于生产设备。

## 设计目标

- 纯 C99，不依赖 C++。
- 默认不使用 malloc、calloc、realloc 或 free。
- HAL、协议设备和客户端使用 ops + ctx 实例句柄，可同时创建多个实例。
- 温度公共值使用 int32_t 毫摄氏度，不要求 MCU 具备 FPU。
- 滤波器直接引用 ops，可组合且不需要修改核心 switch。
- GUI、MQTT 和业务计算拥有独立有界事件队列。
- Sensor 核心采用单所有者协作式执行模型。
- 通过脚本和构建目标检查四层依赖。

## 四层架构

| 层级 | 目录 | 可以依赖 |
|---|---|---|
| 功能层 | src/func | 功能层、协议层、工具层 |
| 协议层 | src/proto | 协议层、接口层、工具层 |
| 接口层 | src/iface、src/ports | 接口层、工具层 |
| 工具层 | src/tool | 工具层 |

功能层不得直接包含 hal_、port_ 或平台 SDK 头文件。协议层负责把原始 HAL 能力解释成时钟、寄存器设备、温度器件、显示或 MQTT 等标准服务。

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

& $cmake -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmake --build build --config Debug
~~~

## 测试

Alpha 首次推送后的稳定化阶段将执行完整测试。已有测试可使用：

~~~powershell
$ctest = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'

& $ctest --test-dir build -C Debug --output-on-failure
python tools/check_layers.py
~~~

## 演示

构建后运行：

~~~powershell
.\build\Debug\host_demo.exe
~~~

host_demo 的目标行为是：

1. 在两条独立 host I2C 总线上创建相同地址的 TMP75；
2. 为两个 Sensor 配置不同滤波链；
3. 将事件复制到 GUI、MQTT 和业务队列；
4. 输出温度、质量、报警/统计、MQTT 报文和丢弃计数；
5. 以确定性的有限循环结束。

Alpha 首次推送不承诺所有演示路径已经完成稳定化；实际通过情况以后续 QA 提交和 CHANGELOG 为准。

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

- 当前为首次 Alpha 代码交付；
- 深度边界测试、错误注入、全量端到端验证和代码复核将在首次远程推送后进行；
- STM32F4 目录是 SDK 对接边界说明，不代表已经完成硬件在环验证；
- API 在 v0.1 稳定化前可能根据测试结果做兼容性调整。

详细设计见 doc/sensor_framework_design_20260809_182444.md。

下一版本建议见 doc/NEXT_VERSION_RECOMMENDATIONS.md。

