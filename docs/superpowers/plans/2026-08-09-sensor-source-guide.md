# Sensor Framework Source Guide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 生成一份逐文件解释当前 Sensor 框架生产源码、结构体和函数语义的中文 Markdown 导读。

**Architecture:** 以当前工作区源码为唯一事实来源，按工具层、接口层、Port、协议层、功能层和 Host Demo 的依赖方向编排。每个文件记录职责、类型、函数和依赖边界，并用符号与文件清单检查防止遗漏或虚构。

**Tech Stack:** C11 源码、Markdown、Mermaid、PowerShell、ripgrep、Git。

## Global Constraints

- 最终文件固定为 `doc/sensor_framework_source_guide_20260809.md`。
- 覆盖 `src/` 下全部 `.c`、`.h` 和 `examples/host_demo/main.c`。
- 不逐文件讲解 `tests/`、CMake、Python 检查工具或构建脚本。
- 不修改生产源码、接口或运行行为。
- 使用中文解释并保留原始 C 标识符。
- 函数语义必须同时核对头文件契约和 `.c` 实现，不能根据名称推测。
- 区分公开 API、内部函数、当前实现和扩展建议。

---

### Task 1: 生成并验证逐文件源码导读

**Files:**
- Create: `doc/sensor_framework_source_guide_20260809.md`
- Read: `src/tool/*.{c,h}`
- Read: `src/iface/*.h`
- Read: `src/ports/host_sim/*.{c,h}`
- Read: `src/ports/template/*.c`
- Read: `src/proto/*.{c,h}`
- Read: `src/func/*.{c,h}`
- Read: `src/func/filter/*.{c,h}`
- Read: `src/func/app/*.{c,h}`
- Read: `examples/host_demo/main.c`

**Interfaces:**
- Consumes: 当前源码中的类型定义、公开函数声明、内部函数实现和四层依赖约束。
- Produces: 可供维护、移植和扩展时查阅的单文件中文源码说明。

- [ ] **Step 1: 固化源码文件清单**

运行：

```powershell
rg --files src examples/host_demo |
  Where-Object { $_ -match '\.(c|h)$' } |
  Sort-Object
```

把输出作为覆盖率基线。`src/ports/*/README.md` 不属于 C 源文件，不纳入逐文件函数说明。

- [ ] **Step 2: 提取实际类型和函数符号**

运行：

```powershell
rg -n "typedef|struct |enum |^[A-Za-z_][A-Za-z0-9_ *]+\(" src examples/host_demo
```

逐个打开匹配所在的头文件和实现文件；对 `static` 函数标为“内部函数”，对仅在头文件声明的接口标为“公开契约”。

- [ ] **Step 3: 编写文档导航与调用图**

文档开头必须包含：

1. 定点温度单位为 `mdeg_c`（毫摄氏度）。
2. 依赖方向为功能层 → 协议层 → 接口层，工具层可被所有层使用。
3. 采样主链：clock → sensor core → temperature driver → protocol device → HAL Port。
4. 发布主链：sensor event → 三个独立队列 → GUI、MQTT、business consumers。
5. Mermaid 图必须显示上述两条主链和层级边界。

- [ ] **Step 4: 逐文件编写工具层、接口层和 Port**

按下列文件组逐个建立小节，不合并或遗漏文件：

```text
src/tool/util_cfg.h
src/tool/util_status.h/.c
src/tool/util_byte.h/.c
src/tool/util_math.h/.c
src/tool/util_log.h/.c
src/tool/util_ringbuf.h/.c
src/iface/hal_cfg.h
src/iface/hal_gpio.h
src/iface/hal_i2c.h
src/iface/hal_net.h
src/iface/hal_spi.h
src/iface/hal_time.h
src/iface/hal_uart.h
src/ports/host_sim/host_i2c.h/.c
src/ports/host_sim/host_net.h/.c
src/ports/host_sim/host_time.h/.c
src/ports/template/hal_i2c.c
src/ports/template/hal_net.c
src/ports/template/hal_time.c
```

每个小节必须解释文件职责、类型字段、公开函数、重要内部函数和依赖边界；纯配置头及 HAL 声明头要说明没有运行逻辑的原因。

- [ ] **Step 5: 逐文件编写协议层**

按下列文件组逐个建立小节：

```text
src/proto/proto_cfg.h
src/proto/proto_clock.h/.c
src/proto/proto_display.h/.c
src/proto/proto_i2c_reg.h/.c
src/proto/proto_temp.h
src/proto/proto_temp_tmp75.h/.c
src/proto/proto_mqtt.h/.c
```

重点解释 HAL 前置声明的分层意义、I2C 寄存器操作、TMP75 定点解码，以及 MQTT 的连接状态、CONNACK 校验、静态发送队列、重连和 poll 预算。

- [ ] **Step 6: 逐文件编写功能层**

按下列文件组逐个建立小节：

```text
src/func/func_cfg.h
src/func/func_sensor_types.h
src/func/func_event_queue.h/.c
src/func/func_sensor.h/.c
src/func/func_temp.h/.c
src/func/filter/func_filter.h/.c
src/func/filter/func_filter_ma.h/.c
src/func/filter/func_filter_ema.h/.c
src/func/filter/func_filter_median.h/.c
src/func/filter/func_filter_limit.h/.c
src/func/app/func_app_gui.h/.c
src/func/app/func_app_mqtt.h/.c
src/func/app/func_app_biz.h/.c
src/func/app/func_app_runtime.h/.c
```

重点解释 caller-owned 静态对象、滤波 ops/chain、事件 fan-out、温度质量状态、每消费者独立队列、按 sensor ID 隔离的业务窗口，以及 runtime 的单 owner 调度。

- [ ] **Step 7: 解释 Host Demo 装配**

为 `examples/host_demo/main.c` 单独建立小节，按“fixture 和回调 → HAL/协议实例 → 两个 Sensor 和滤波链 → GUI/MQTT/biz 三队列 → 时间循环与故障注入 → 汇总输出”解释。列出重要内部辅助函数，但避免逐行翻译 514 行代码。

- [ ] **Step 8: 检查文件覆盖率和内容边界**

运行以下 PowerShell 检查；输出的 `$missing` 必须为空，目标文档不得包含占位文本：

```powershell
$guide = Get-Content -Raw -Encoding UTF8 doc/sensor_framework_source_guide_20260809.md
$files = rg --files src examples/host_demo |
  Where-Object { $_ -match '\.(c|h)$' }
$missing = $files | Where-Object {
  $normalized = $_ -replace '\\', '/'
  $guide -notmatch [regex]::Escape($normalized)
}
$missing
Select-String -Path doc/sensor_framework_source_guide_20260809.md `
  -Pattern (('T'+'BD'), ('T'+'ODO'), ('待'+'补充'), ('待'+'完善'))
```

预期：两条检查均无输出。

- [ ] **Step 9: 人工核对高风险契约**

将文档中的以下描述逐项与源码对照：

- `sns_status_t` 的全部错误码值；
- 环形缓冲区 empty/full 判断和容量语义；
- TMP75 负温度符号扩展；
- `func_filter_chain_apply` 的执行与错误传播；
- Sensor 的 `VALID → STALE → ERROR → VALID` 质量变化；
- MQTT 只有成功 CONNACK 后才连接、断线后从完整包头重发；
- business state 以 `sensor_id` 分离；
- Demo 只使用 GUI、MQTT、biz 三个事件队列。

- [ ] **Step 10: 检查并提交**

运行：

```powershell
git diff --check
git status --short
git add -- doc/sensor_framework_source_guide_20260809.md
git commit -m "docs: explain sensor framework source files"
```

预期：只提交目标导读文件，不包含 `.claude/` 或构建产物。
