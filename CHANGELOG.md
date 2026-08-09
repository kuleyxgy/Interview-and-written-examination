# Changelog

## v0.1.0

- 将 Sensor 注册表改为显式 `func_sensor_core_t` 实例，支持多个独立框架对象。
- 完成 MQTT 3.1.1 CONNECT/CONNACK 状态机、分片/粘包处理、拒绝处理、完整报文重发、配置原子性和 poll 预算约束。
- GUI、业务和 MQTT runtime 支持独立裁剪；业务统计绑定 Sensor ID，避免多 Sensor 窗口混合。
- 温度校准使用 `int64_t` 中间值并饱和到 `int32_t`。
- 完成双 host I2C、同地址 TMP75、不同滤波链、三类消费者、错误注入和 MQTT 断线重连端到端演示。
- CMake 拆分为 tool、iface/port、proto、func targets，并支持 `SENSOR_PORT=none` 和 Port 模板编译检查。
- 增加 40 个 C 测试函数及分层检查负例；MSVC `/W4 /WX` 构建 0 警告、0 错误，12/12 CTest 通过。

## v0.1.0-alpha

初始代码交付计划包含：

- 四层 C99 工程结构；
- 静态工具层；
- 实例化 HAL 与 host_sim；
- TMP75 温度协议；
- 可插拔定点滤波链；
- Sensor 核心和独立应用队列；
- GUI、MQTT 和业务消费者；
- host_demo 与分层检查；
- 构建、演示和移植 README。

此条目只表示 Alpha 初始实现已推送。完整 QA、缺陷修复、最终测试数量和稳定化结论将在后续 v0.1.0 条目中记录。
