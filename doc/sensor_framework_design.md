# 跨 MCU 平台 Sensor 框架 — 设计与实施计划

> 版本: v0.1 (待评审) · 日期: 2026-08-09
> 状态: **设计稿，等待评审后再进入编码**

---

## 1. 目标与范围

用 **纯 C（C99）** 实现一个**跨 MCU 平台**的传感器采集框架，以**温度传感器**为第一个落地案例，满足三点核心诉求：

1. **多滤波方式灵活可插拔** —— 滤波器以"注册表 + 算子表"方式扩展，新滤波器不改核心代码。
2. **应用层完全解耦** —— GUI 显示、MQTT 上报、业务计算三者在框架层彼此隔离，通过"事件订阅"接入，互不感知、可独立增删。
3. **四层架构 + 严格依赖规则** —— 功能层 / 协议层 / 接口层 / 工具层，每模块归属唯一层级，接口标准化，跨 MCU 只改接口层实现。

设计原则：
- **平台无关**：框架代码不出现任何寄存器、外设寄存器、编译器私有特性；对硬件的一切访问都经接口层 HAL。
- **静态优先**：默认静态内存分配，不依赖 `malloc`；提供可选的动态分配宏开关。
- **可测试**：提供 PC 端 `host_sim` 移植（模拟 I2C 从机 / ADC / 网络），支撑单测与端到端 demo。
- **可裁剪**：所有能力由编译期配置宏控制（`sensor_cfg.h`），按需裁剪。

---

## 2. 总体架构

```
┌────────────────────────────── 功能层（含应用模块） ──────────────────────────────┐
│    [func_app_gui]   [func_app_mqtt]   [func_app_biz]                              │
│         │ 订阅             │ 订阅             │ 订阅                              │
│         ▼                 ▼                 ▼                                    │
│    ┌───────────────────────────────────────────────┐                             │
│    │   func_sensor   传感器注册表 + 事件总线         │  ◄── 功能层核心              │
│    └───────────────────────────────────────────────┘                             │
│         ▲ 采样+滤波后 publish                       │ 依赖(同层/协议层)            │
│    [func_temp][func_ntc] ──►  [func_filter 可插拔滤波框架+内置滤波器]               │
└─────────────────┬───────────────────────────────────────────────────────────────┘
                  │ 调用
┌─────────────────▼───────────────────────────────────────────────────────────────┐
│  协议层:  [proto_i2c] [proto_adc] [proto_temp] [proto_mqtt] [proto_display]       │
└─────────────────┬───────────────────────────────────────────────────────────────┘
                  │ 调用
┌─────────────────▼───────────────────────────────────────────────────────────────┐
│  接口层:  [hal_i2c][hal_spi][hal_uart][hal_adc][hal_gpio][hal_net][hal_time]     │
│           [plat_os]                                                              │
│     实现:  ports/host_sim（PC 模拟） · ports/stm32f4xx（示例 MCU，预留）          │
└─────────────────┬───────────────────────────────────────────────────────────────┘
                  │ 调用
┌─────────────────▼───────────────────────────────────────────────────────────────┐
│  工具层:  [util_result][util_log][util_list][util_ringbuf][util_byte]            │
│           [util_math][util_crc]                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 分层与依赖规则

| 层级 | 职责 | 允许依赖 |
|---|---|---|
| **功能层** | 业务能力：传感器生命周期/事件总线、滤波、温度采样、应用(GUI/MQTT/业务) | 功能层、**协议层**、工具层 |
| **协议层** | 通信/器件协议：I2C 寄存器访问、温度芯片协议、MQTT、显示协议 | **接口层**、工具层 |
| **接口层** | 硬件与 OS 抽象：总线/ADC/网络/时钟/锁 | 工具层 |
| **工具层** | 通用基础组件：日志、容器、字节序、数学、CRC | 无（可被所有层级调用） |

**硬性约束（违反即构建失败/CI 失败）：**
- ❌ 功能层 不能直接调用 接口层（`hal_*` / `plat_*`）——一切硬件访问必须经协议层。
- ❌ 接口层 不能反向依赖 协议层/功能层。
- ❌ 工具层 不得依赖任何上层。
- ✅ 同层互调允许（如 `proto_temp → proto_i2c`、`func_temp → func_sensor/func_filter`）。

> 该规则由 `tools/check_layers.py` 脚本强制检查：扫描各 `.c/.h` 的 `#include`，命中违规即报错（纳入 CI）。

---

## 4. 模块清单与层级归属

| 模块 | 层级 | 说明 | 关键跨层依赖 |
|---|---|---|---|
| `util_result` | 工具 | 统一返回码 / 错误码定义 | — |
| `util_log` | 工具 | 分级日志，输出可重定向 | — |
| `util_list` | 工具 | 双向链表（订阅者链表等） | — |
| `util_ringbuf` | 工具 | 环形缓冲（业务统计窗口等） | — |
| `util_byte` | 工具 | 大小端读写、位操作 | — |
| `util_math` | 工具 | clamp/minmax/线性插值等 | — |
| `util_crc` | 工具 | CRC8/16/32（SHT3x 校验、MQTT 扩展） | — |
| `hal_i2c` | 接口 | I2C 主机总线抽象（原始字节收发） | util_log |
| `hal_spi` | 接口 | SPI 总线抽象（显示等） | util_log |
| `hal_uart` | 接口 | 串口抽象（调试/透传） | util_log |
| `hal_adc` | 接口 | ADC 通道抽象（NTC 用） | util_log |
| `hal_gpio` | 接口 | GPIO 抽象 | util_log |
| `hal_net` | 接口 | TCP 网络抽象（MQTT 承载） | util_log |
| `hal_time` | 接口 | 单调毫秒时钟、延时 | — |
| `plat_os` | 接口 | 互斥锁/信号量抽象（裸机=空实现） | — |
| `ports/host_sim` | 接口(实现) | PC 模拟 HAL：文件/套接字模拟外设 | hal_* 头 |
| `ports/stm32f4xx` | 接口(实现) | 示例 MCU 移植模板（预留） | hal_* 头 |
| `proto_i2c` | 协议 | I2C 寄存器读/写协议（器件寻址、16bit） | hal_i2c, util_byte |
| `proto_adc` | 协议 | ADC 采集协议（原始值→电压） | hal_adc |
| `proto_temp` | 协议 | 温度芯片寄存器协议（LM75 / SHT3x） | proto_i2c, util_byte, util_crc |
| `proto_mqtt` | 协议 | MQTT 3.1.1 客户端（报文编解码+会话） | hal_net, util_log |
| `proto_display` | 协议 | 显示抽象协议（可替换串口屏/LCD/LVGL） | hal_uart/hal_spi/hal_gpio |
| `func_sensor` | 功能 | 核心：注册表、生命周期、**事件总线** | util_list, plat_os(可选) |
| `func_filter` | 功能 | **可插拔滤波框架** + 内置滤波器 | util_math |
| `func_temp` | 功能 | 温度传感器功能（采样策略/标定/上报） | func_sensor, func_filter, proto_temp |
| `func_ntc` | 功能 | NTC 热敏电阻温度（ADC 通道案例） | func_sensor, func_filter, proto_adc |
| `func_app_gui` | 功能 | 应用·GUI 显示 | func_sensor, proto_display |
| `func_app_mqtt` | 功能 | 应用·MQTT 上报 | func_sensor, proto_mqtt |
| `func_app_biz` | 功能 | 应用·业务计算（报警/统计/趋势） | func_sensor, util_math, util_ringbuf |

---

## 5. 关键接口设计（头文件草案）

### 5.1 工具层（节选）

```c
/* util_result.h —— 统一返回约定 */
#define SNS_OK                      0
#define SNS_ERR_PARAM             (-1)
#define SNS_ERR_NOT_FOUND         (-2)
#define SNS_ERR_BUSY              (-3)
#define SNS_ERR_TIMEOUT           (-4)
#define SNS_ERR_NOT_READY         (-5)
#define SNS_ERR_INVALID_DATA      (-6)
#define SNS_ERR_NO_MEM            (-7)
#define SNS_ERR_BAD_CRC           (-8)
/* 各层可在 0x-20 之外自扩: HAL_ERR_* / PROTO_ERR_* / FUNC_ERR_* */
const char *sns_err_str(int32_t err);
```

```c
/* util_log.h —— 分级日志 */
typedef enum { LOG_LVL_DBG=0, LOG_LVL_INFO, LOG_LVL_WARN, LOG_LVL_ERR } log_lvl_t;
typedef void (*log_sink_t)(log_lvl_t lvl, const char *tag, const char *msg);
void util_log_init(log_sink_t sink, log_lvl_t min_lvl);
void util_log(log_lvl_t lvl, const char *tag, const char *fmt, ...);
#define LOGD(tag, ...) util_log(LOG_LVL_DBG,  tag, __VA_ARGS__)
#define LOGI(tag, ...) util_log(LOG_LVL_INFO, tag, __VA_ARGS__)
#define LOGW(tag, ...) util_log(LOG_LVL_WARN, tag, __VA_ARGS__)
#define LOGE(tag, ...) util_log(LOG_LVL_ERR,  tag, __VA_ARGS__)
```

```c
/* util_byte.h */
uint16_t util_be16_read(const uint8_t *p);        /* 大端读 16bit */
uint32_t util_be24_read(const uint8_t *p);        /* SHT3x 用 */
int32_t  util_be16_to_s11_5(const uint8_t *p);    /* LM75 11bit补码→0.125℃倍数 */
```

### 5.2 接口层（HAL，全部为纯接口头）

```c
/* hal_i2c.h —— 只做原始字节收发，寄存器寻址交给 proto_i2c */
typedef struct { void *periph; uint32_t freq_hz; int32_t timeout_ms; } hal_i2c_cfg_t;
int32_t hal_i2c_init(uint8_t bus, const hal_i2c_cfg_t *cfg);
int32_t hal_i2c_write(uint8_t bus, uint8_t dev, const uint8_t *data, uint16_t len);
int32_t hal_i2c_read (uint8_t bus, uint8_t dev, uint8_t *data, uint16_t len);
```

```c
/* hal_adc.h */
int32_t hal_adc_init(uint8_t ch);
int32_t hal_adc_read_mv(uint8_t ch, int32_t *mv);   /* 参考源/缩放已在 port 内处理 */
```

```c
/* hal_net.h —— TCP 承载（PC=BSD/WinSock，MCU=AT模组/以太网栈） */
typedef struct { const char *host; uint16_t port; int32_t timeout_ms; } hal_net_cfg_t;
int32_t hal_net_connect(const hal_net_cfg_t *cfg);
int32_t hal_net_send(const uint8_t *data, uint16_t len);
int32_t hal_net_recv(uint8_t *data, uint16_t max_len, int32_t timeout_ms);
void    hal_net_close(void);
```

```c
/* hal_time.h */
uint32_t hal_time_ms(void);      /* 单调时钟，ms */
void     hal_delay_ms(uint32_t ms);
```

```c
/* plat_os.h —— 可选；裸机下为空实现，RTOS 下映射到互斥锁 */
typedef struct { void *hdl; } plat_mutex_t;
int32_t plat_mutex_create(plat_mutex_t *m);
int32_t plat_mutex_lock(plat_mutex_t *m);
int32_t plat_mutex_unlock(plat_mutex_t *m);
```

### 5.3 协议层

```c
/* proto_i2c.h —— I2C 寄存器访问协议 */
typedef struct { uint8_t bus; uint32_t timeout_ms; } proto_i2c_cfg_t;
int32_t proto_i2c_init(const proto_i2c_cfg_t *cfg);
int32_t proto_i2c_reg_read_8 (uint8_t dev, uint8_t reg, uint8_t *v);
int32_t proto_i2c_reg_read_16(uint8_t dev, uint8_t reg, uint16_t *v, bool big_endian);
int32_t proto_i2c_reg_write_8(uint8_t dev, uint8_t reg, uint8_t v);
int32_t proto_i2c_reg_write_16(uint8_t dev, uint8_t reg, uint16_t v);
```

```c
/* proto_temp.h —— 温度芯片寄存器协议（wire 级） */
int32_t proto_temp_lm75_read (uint8_t dev, int32_t *mdeg_c);   /* 12bit→0.125℃ */
int32_t proto_temp_sht3x_read(uint8_t dev, int32_t *mdeg_c);   /* 16bit+CRC8 */
```

```c
/* proto_mqtt.h —— MQTT 3.1.1 客户端（零外部依赖，跨平台） */
typedef struct {
    const char *client_id; const char *username; const char *password;
    uint16_t keepalive_s; uint8_t clean_session;
} proto_mqtt_cfg_t;
typedef void (*mqtt_msg_cb_t)(const char *topic, const uint8_t *payload,
                              uint16_t len, void *user);
int32_t proto_mqtt_init(const proto_mqtt_cfg_t *cfg);
int32_t proto_mqtt_connect(void);
int32_t proto_mqtt_publish(const char *topic, const void *payload, uint16_t len, uint8_t qos);
int32_t proto_mqtt_subscribe(const char *topic, uint8_t qos, mqtt_msg_cb_t cb, void *user);
int32_t proto_mqtt_poll(void);      /* 主循环调用：收包/心跳/自动重连 */
void    proto_mqtt_close(void);
```

```c
/* proto_display.h —— 显示抽象协议（可替换为 串口屏/LCD/LVGL） */
typedef struct { uint8_t rows, cols; } proto_display_cfg_t;
int32_t proto_display_init(const proto_display_cfg_t *cfg);
int32_t proto_display_show(const char *sensor_name, double value, const char *unit);
```

### 5.4 功能层

```c
/* func_sensor.h —— 注册表 + 生命周期 + 事件总线 */
typedef enum { FUNC_SENSOR_TEMP=0, FUNC_SENSOR_NTC, FUNC_SENSOR_TYPE_MAX } func_sensor_type_t;
typedef enum { SENSOR_Q_VALID=0, SENSOR_Q_STALE, SENSOR_Q_ERROR } func_sensor_quality_t;

typedef struct {
    uint32_t ts_ms;                 /* 单调时钟时间戳 */
    func_sensor_type_t type;
    double  value;                  /* 工程单位：℃ */
    uint8_t quality;
} func_sensor_event_t;

typedef void (*func_sensor_event_cb_t)(const func_sensor_event_t *evt, void *user);

typedef struct {                    /* 传感器驱动算子：新传感器实现它即可接入 */
    int32_t (*init)(func_sensor_t *s);
    int32_t (*poll)(func_sensor_t *s);   /* 采样→滤波→publish */
} func_sensor_drv_ops_t;

int32_t       func_sensor_init(void);
func_sensor_t *func_sensor_register(const char *name, func_sensor_type_t type,
                                    const func_sensor_drv_ops_t *ops, void *priv);
int32_t       func_sensor_subscribe(func_sensor_t *s, func_sensor_event_cb_t cb, void *user);
int32_t       func_sensor_publish(func_sensor_t *s, const func_sensor_event_t *evt);
int32_t       func_sensor_poll_all(void);                 /* 主循环驱动 */
double        func_sensor_get_latest(const func_sensor_t *s);
```

```c
/* func_filter.h —— 可插拔滤波框架 */
typedef enum {
    FUNC_FILTER_NONE=0,
    FUNC_FILTER_MA,        /* 滑动平均  */
    FUNC_FILTER_EMA,       /* 指数平均  */
    FUNC_FILTER_MEDIAN,    /* 中值      */
    FUNC_FILTER_LPF,       /* 一阶低通  */
    FUNC_FILTER_KALMAN,    /* 一维卡尔曼*/
    FUNC_FILTER_LIMIT,     /* 幅值+变化率限幅 */
    FUNC_FILTER_USER_BASE = 0x100,   /* 用户自定义起始 */
} func_filter_type_t;

typedef struct func_filter func_filter_t;

typedef struct {
    int32_t (*init)(func_filter_t *f, const void *cfg);
    int32_t (*reset)(func_filter_t *f);
    double  (*process)(func_filter_t *f, double raw);
} func_filter_ops_t;

typedef struct { func_filter_type_t type; const func_filter_ops_t *ops; } func_filter_reg_t;

int32_t func_filter_register(const func_filter_reg_t *reg);   /* 运行期注册新滤波器 */
int32_t func_filter_chain_create(func_filter_t *chain, const func_filter_reg_t *regs,
                                 uint8_t n, const void **cfgs);
double  func_filter_chain_process(func_filter_t *chain, uint8_t n, double raw);
void    func_filter_chain_reset(func_filter_t *chain, uint8_t n);
```

```c
/* func_temp.h —— 温度传感器功能配置（编译期静态表） */
typedef struct {
    const char *name;
    uint8_t i2c_dev;                 /* 从机地址 */
    uint16_t sample_ms;              /* 采样周期 */
    struct { func_filter_type_t type; const void *cfg; } filters[4];
    uint8_t filter_cnt;              /* 滤波链长度(按序执行) */
    double  pub_threshold;           /* 变化超此值即上报 */
    uint8_t pub_period_div;          /* 每 N 次采样强制上报 */
    double  cal_gain, cal_offset;    /* y = x*gain + offset */
} func_temp_cfg_t;
int32_t func_temp_init(func_sensor_t *s, const func_temp_cfg_t *cfg);
```

---

## 6. 滤波框架设计（重点）

**设计要点：算子表（ops）+ 注册表 + 配置驱动组装，新增滤波方式零改动核心。**

1. **算子表** `func_filter_ops_t` 定义统一行为：`init / reset / process`。每种滤波算法只需实现这三个函数，私用状态放 `func_filter_t.priv`（由配置的 `cfg` 决定大小，静态分配）。
2. **注册表** `func_filter_register()` 将 `{type, ops}` 挂入全局表；内置滤波器在 `func_filter.c` 中随模块初始化自动注册。用户算法用 `FUNC_FILTER_USER_BASE` 起自定义枚举。
3. **滤波链**：`func_filter_chain_create()` 按 `func_temp_cfg_t.filters[]` 顺序实例化多个滤波器，逐级串联；`process` 依次传入，前一级输出为后一级输入。
4. **配置即代码**：滤波组合、参数全部由各传感器的编译期配置表决定，运行期可改参数（`reset` 支持重初始化）。

内置滤波器一览：

| type | 说明 | 配置 |
|---|---|---|
| `FUNC_FILTER_MA` | 滑动平均 | `{uint8_t window}` |
| `FUNC_FILTER_EMA` | 指数平均 | `{float alpha}` |
| `FUNC_FILTER_MEDIAN` | 中值 | `{uint8_t window}` |
| `FUNC_FILTER_LPF` | 一阶低通 | `{float cutoff_hz; uint32_t dt_ms}` |
| `FUNC_FILTER_KALMAN` | 一维卡尔曼 | `{float q, r; float init_x}` |
| `FUNC_FILTER_LIMIT` | 幅值+变化率限幅 | `{double max_step; double min, max}` |

**新增一种滤波方式 = 3 步：**
```
① 定义配置结构体 + 实现 init/reset/process（一个 .c/.h）
② 调 func_filter_register(&reg)；  ③ 在目标传感器 cfg.filters[] 中引用
```

---

## 7. 传感器核心与事件总线

- `func_sensor` 维护**注册表**（`util_list`），每个传感器实例含：名字、类型、`drv_ops`、私有数据、最新值缓存、订阅者链表。
- **事件发布**：传感器驱动在 `poll` 内 采样 → 滤波链 → `func_sensor_publish`，核心同步遍历订阅者回调（可选加 `plat_mutex` 保护，RTOS 多任务安全）。
- **上报策略**（在 `func_temp` 实现）：周期采样 + **变化阈值触发** + **定时强制上报** 三策略组合，控制事件流量。
- **应用订阅**：`func_app_xxx` 启动时 `func_sensor_subscribe(s, cb, user)`；核心对应用零感知——**解耦的根本**。

```
采样周期到 → func_temp.poll → proto_temp 读寄存器 → 单位换算/标定
         → func_filter 滤波链 → func_sensor_publish(evt)
                                     │ 同步分发
              ┌──────────────────────┼──────────────────────┐
              ▼                      ▼                      ▼
       func_app_gui            func_app_mqtt          func_app_biz
       proto_display 渲染       proto_mqtt 发布 JSON    报警/统计/趋势
```

---

## 8. 应用解耦设计（GUI / MQTT / 业务）

| 模块 | 接入方式 | 输出 |
|---|---|---|
| `func_app_gui` | 订阅事件 | `proto_display_show("temp", v, "℃")`（协议层隔离显示硬件） |
| `func_app_mqtt` | 订阅事件 | 组 JSON → `proto_mqtt_publish("home/sensor/temp", ...)` |
| `func_app_biz` | 订阅事件 | 业务计算：过温报警(带迟滞状态机)、滑动窗口 min/max/avg、趋势斜率；报警经用户回调上抛 |

**解耦收益：**
- 核心与任一应用无依赖；应用间无依赖；**增删应用 = 新增/删除一个模块文件 + 配置**，核心零改动。
- 三个应用可各自独立裁剪（宏 `SENSOR_CFG_ENABLE_APP_GUI/MQTT/BIZ`）。
- MQTT 不阻塞采样：`publish` 为异步写出，网络收发包在 `proto_mqtt_poll()` 中处理。

---

## 9. 跨平台移植（HAL Porting）

**新 MCU 移植 = 只写接口层，框架零改动。**

```
新增 ports/<chip>/:
  hal_i2c.c  hal_spi.c  hal_uart.c  hal_adc.c  hal_gpio.c
  hal_net.c  hal_time.c  plat_os.c   board_cfg.h
```

步骤：
1. 复制 `ports/template/` 骨架；
2. 逐个实现 `hal_*` 函数（映射到该 MCU 外设驱动库 / 寄存器）；
3. 在 `board_cfg.h` 声明：传感器挂在哪条 I2C 总线/哪个从机地址、NTC 用哪个 ADC 通道、MQTT 走哪张网卡；
4. 在 `sensor_cfg.h` 设置资源上限与 OS 开关；
5. 编译，跑 `tools/check_layers.py` + 目标板自检。

**PC 端验证平台 `ports/host_sim`**：I2C 用注册的假从机（回放真实采样文件 / 脚本响应）、ADC 用文件数据、网络用 BSD 套接字——保证框架在无硬件时即可单测与端到端演示。

---

## 10. 编译期配置（`src/sensor_cfg.h`）

```c
#define SENSOR_CFG_MAX_SENSORS        4      /* 注册表容量 */
#define SENSOR_CFG_MAX_SUBSCRIBERS    8      /* 单传感器订阅者上限 */
#define SENSOR_CFG_MAX_FILTERS        4      /* 单链最大滤波器数 */
#define SENSOR_CFG_OS_ENABLE          0      /* 1=使用 plat_os 互斥锁 */
#define SENSOR_CFG_USE_DYNAMIC_MEM    0      /* 1=允许 malloc（默认静态） */
#define SENSOR_CFG_ENABLE_APP_GUI     1
#define SENSOR_CFG_ENABLE_APP_MQTT    1
#define SENSOR_CFG_ENABLE_APP_BIZ     1
#define SENSOR_CFG_UNIT                 ... /* 单位制 */
```

---

## 11. 目录结构与构建

```
e:/project/
├── doc/sensor_framework_design.md     # 本文档
├── src/
│   ├── sensor_cfg.h                   # 全局编译期配置
│   ├── tool/                          # 工具层 util_*.h/.c
│   ├── iface/                         # 接口层 仅头文件 hal_*.h / plat_os.h
│   ├── proto/                         # 协议层 proto_*.h/.c
│   ├── func/                          # 功能层 func_*.h/.c
│   │   └── app/                       # 功能层-应用模块 func_app_*.h/.c
│   └── ports/                         # 接口层实现
│       ├── template/  ├── host_sim/  └── stm32f4xx/   (预留)
├── examples/host_demo/main.c          # PC 端端到端演示
├── tests/                             # 各层单测 (tool/proto/func) + mocks
├── tools/check_layers.py              # 分层依赖强制检查
└── CMakeLists.txt                     # host 构建+测试；MCU 可直接取 src/ 集成
```

---

## 12. 编码规范

- **命名**：`层前缀_模块_功能`（`util_` / `hal_`/`plat_` / `proto_` / `func_`），小写下划线；类型/枚举大写；宏全大写。**前缀即层级，便于人审 + 脚本强制检查。**
- **返回**：统一 `int32_t`，0 成功 / 负值错误码（见 `util_result`）。
- **头文件**：自包含、带 `extern "C"` 保护；接口头内注释标明"协议/平台职责"。
- **内存**：默认静态；`cfg` 均为 `const`，可放 flash（`const` 段）。
- **C99**，禁止可变长数组/危险宏；编译告警 `-Wall -Wextra -Werror`。

---

## 13. 测试策略

| 层级 | 测试内容 | 手段 |
|---|---|---|
| 工具层 | 链表/环形缓冲/字节序/CRC 边界 | host 单测 |
| 协议层 | I2C 假从机收发、LM75 12bit 换算、**MQTT 报文编解码**（对比抓包参考字节） | host 单测 + host_sim |
| 功能层 | 各滤波器**数值正确性**（与参考输出比对）、滤波链组合、事件总线 fan-out、上报策略 | host 单测 |
| 应用层 | GUI/MQTT/业务 的订阅接线、报警状态机 | host 单测 + demo |
| 端到端 | `examples/host_demo`：模拟 2 个温度源 → 滤波 → 三应用 → 控制台/MQTT 输出 | 运行 demo |
| 分层合规 | `tools/check_layers.py` 扫描 include | CI |

---

## 14. 实施计划

| 阶段 | 内容 | 交付物 / 验收标准 |
|---|---|---|
| P0 | 工程骨架 | CMake、目录、`sensor_cfg.h`、`util_result`、编码规范落地；host 空跑通过 |
| P1 | 工具层 | `util_log/list/ringbuf/byte/math/crc` 全模块 + 单测通过 |
| P2 | 接口层 | `hal_*`/`plat_os` 头 + `host_sim` 实现（含 I2C 假从机、模拟 ADC、BSD 网络）；hal 自测 |
| P3 | 协议层 | `proto_i2c / proto_adc / proto_temp(LM75)` + 单测（用假从机） |
| P4 | 滤波框架 | `func_filter` 注册表 + 6 种内置滤波器 + 滤波链 + 数值单测 |
| P5 | 传感器核心 | `func_sensor` 注册表/事件总线 + `func_temp` + `func_ntc`；2 传感器 demo 出数 |
| P6 | 应用解耦 | `proto_mqtt`(含编解码单测)、`proto_display`、`func_app_gui/mqtt/biz`；三应用接入 |
| P7 | 端到端+收尾 | `examples/host_demo` 完整运行、`check_layers.py` 接入 CI、`doc/porting.md` 移植指南 |

> 每阶段结束提交一次，保证可回退；P4 的滤波数值单测与 P6 的 MQTT 编解码单测是质量闸门。

---

## 15. 待你拍板的决策点

| # | 决策点 | 我的建议（默认） | 说明 |
|---|---|---|---|
| 1 | 目标 MCU / RTOS | **暂无固定硬件**，先做 `host_sim`(PC)+预留 `stm32f4xx` 模板 | 有具体平台可排优先级 |
| 2 | MQTT 实现 | **内置自研 `proto_mqtt`（零外部依赖）**，后续可加 AT 模组适配 | 跨平台最干净 |
| 3 | 内存策略 | **静态分配（无 malloc）**，预留动态开关 | MCU 内存受限 |
| 4 | 上报策略 | **周期 + 变化阈值 + 定时强制** 三策略 | 业务需要何种粒度可再调 |
| 5 | 温度芯片 | **LM75（I2C,12bit）**为主例，SHT3x(带CRC)作第二例 | 有指定型号请指出 |
| 6 | 是否需要 Modbus/UART 对外接口 | 暂不做，作为可选 `proto_modbus` 预留 | 接口已允许扩展 |

---

*请在以上设计与决策点上给出意见（可直接在文档上批注，或告诉我 #序号的选择），确认后再开始 P0 编码。*
