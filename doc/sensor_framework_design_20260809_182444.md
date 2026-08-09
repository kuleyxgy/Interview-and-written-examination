# 跨 MCU 平台 Sensor 框架——修订版设计与实施计划

> 版本：v0.2
>
> 生成时间：2026-08-09 18:24:44（Asia/Hong_Kong）
>
> 状态：设计稿，等待评审；未经确认不进入 C 代码实现

---

## 1. 目标与非目标

### 1.1 目标

使用纯 C99 实现一个跨 MCU 平台的 Sensor 框架，首个完整案例采用数字温度传感器 TMP75 兼容器件，满足以下要求：

1. 框架严格分为功能层、协议层、接口层、工具层。
2. 功能层只能调用功能层、协议层和工具层。
3. 协议层可以调用协议层、接口层和工具层。
4. 接口层只能调用接口层和工具层。
5. 工具层不依赖任何上层，但可以被所有层调用。
6. 每个运行时 C 模块必须且只能归属一个层级。
7. 温度采样支持多个实例、多条总线和不同器件协议。
8. 滤波器可以自由组合和扩展，新增滤波算法不修改滤波核心。
9. GUI、MQTT 和业务计算互不依赖，通过独立事件队列消费 Sensor 数据。
10. 更换 MCU 时，仅替换接口层 Port 和协议层板级装配配置，不修改框架核心。
11. 默认不使用 malloc，不要求 MCU 具备 FPU。

### 1.2 首版非目标

为控制首版范围，以下能力不在首版实现中：

- MQTT QoS 1、QoS 2 和持久会话；
- 框架内部创建 RTOS 任务、互斥锁或信号量；
- 动态加载滤波器；
- 完整通用 GUI 控件库；
- Modbus、CANopen 等外部协议；
- 在一个框架实例中由多个线程并发调用 Sensor 核心 API。

这些能力保留扩展点，但不得影响首版的分层和接口稳定性。

---

## 2. 核心设计决策

### 2.1 实例句柄代替全局单例

HAL 总线、网络连接、协议客户端和具体 Sensor 都使用实例句柄。任何模块不得把当前总线、当前设备或当前 MQTT 客户端保存在唯一的隐式全局变量中。

该规则保证：

- 同时使用多条 I2C 总线；
- 同时挂接多个相同型号的 Sensor；
- 同时运行多个 MQTT 客户端；
- 单元测试中并行构造多个模拟实例；
- Port 实现可以替换而不改变上层接口。

### 2.2 默认使用定点工程量

温度公开值使用 int32_t 毫摄氏度：

- 25000 表示 25.000℃；
- 不依赖 double 或硬件 FPU；
- MQTT 和 GUI 在各自应用模块中进行格式化；
- 滤波算法默认处理 int32_t，内部需要乘法累加时使用 int64_t 防止溢出。

若后续确实需要浮点卡尔曼滤波，作为独立可裁剪模块加入，不改变 Sensor 事件格式。

### 2.3 静态内存由调用方显式提供

所有容量都在配置中明确，框架不调用 malloc。队列、协议报文缓冲、滤波器状态和 Sensor 驱动状态均由装配代码提供静态存储。

所有 API 必须明确：

- 存储区地址；
- 存储区大小；
- 生命周期；
- 满容量时的处理策略。

### 2.4 单所有者执行模型

首版采用协作式单所有者模型：

- Sensor 注册、轮询、发布和队列消费都由同一个主循环或同一个 RTOS Sensor 任务驱动；
- 框架核心不直接调用 plat_os、mutex 或 semaphore；
- RTOS 项目必须将对 Sensor 核心的操作投递给唯一 Sensor 任务；
- 首版队列不承诺跨线程安全。

这样可以严格满足“功能层不得调用接口层”，同时避免在回调持锁时产生死锁。

### 2.5 编译期直接引用滤波 ops

标准 C99 没有跨 GCC、IAR、Keil 统一的自动构造或链接段注册机制。因此首版不使用“自动注册表”，而是在静态配置中直接引用滤波器 ops。

新增滤波器只需：

1. 新增一个功能层滤波模块；
2. 实现统一 ops；
3. 在目标 Sensor 的静态滤波链配置中引用该 ops。

滤波核心不需要增加枚举或 switch 分支。

---

## 3. 四层架构

~~~
┌──────────────────────────── 功能层 ────────────────────────────┐
│ func_app_runtime                                              │
│  ├─ func_sensor_core ── func_event_queue                      │
│  ├─ func_temp_driver ── func_filter_chain                     │
│  ├─ func_app_gui                                              │
│  ├─ func_app_mqtt                                             │
│  └─ func_app_biz                                              │
└───────────────────────┬───────────────────────────────────────┘
                        │ 只能调用协议层、功能层、工具层
┌───────────────────────▼────── 协议层 ─────────────────────────┐
│ proto_clock    proto_i2c_reg    proto_temp_tmp75               │
│ proto_mqtt     proto_display    proto_board_profile            │
└───────────────────────┬───────────────────────────────────────┘
                        │ 调用接口层
┌───────────────────────▼────── 接口层 ─────────────────────────┐
│ hal_time   hal_i2c   hal_net   hal_uart   hal_spi   hal_gpio  │
│ ports/host_sim       ports/template       ports/stm32f4xx     │
└───────────────────────┬───────────────────────────────────────┘
                        │ 可调用工具层
┌───────────────────────▼────── 工具层 ─────────────────────────┐
│ util_status  util_log  util_byte  util_math  util_ringbuf     │
└───────────────────────────────────────────────────────────────┘
~~~

### 3.1 依赖矩阵

| 调用方 | 可调用目标 |
|---|---|
| 功能层 | 功能层、协议层、工具层 |
| 协议层 | 协议层、接口层、工具层 |
| 接口层 | 接口层、工具层 |
| 工具层 | 工具层 |

禁止事项：

- 功能层不得包含 hal_、port_ 或 plat_ 头文件；
- 功能层不得直接获得接口层对象；
- 接口层不得包含 proto_ 或 func_ 头文件；
- 工具层不得包含 hal_、proto_ 或 func_ 头文件；
- 不允许通过公共聚合头文件绕过依赖检查；
- 不允许在错误层级声明外部符号后绕过头文件检查。

### 3.2 时间和调度依赖

功能层不直接调用 hal_time。合法调用路径为：

~~~
func_app_runtime
    → proto_clock_now_ms
        → hal_time_now_ms

func_app_runtime
    → func_sensor_poll_all(now_ms)
~~~

时间由功能层运行模块从协议层取得，再以参数传给 Sensor 核心和驱动。单元测试可以直接传入模拟时间。

---

## 4. 模块归属

### 4.1 工具层

| 模块 | 职责 |
|---|---|
| util_status | 统一状态码和错误文本 |
| util_log | 日志格式化和可注入输出 sink |
| util_byte | 大小端、安全字节读取 |
| util_math | 饱和运算、定点计算和范围检查 |
| util_ringbuf | 调用方提供存储的定长环形缓冲 |

工具层不得访问硬件、OS、协议或业务对象。日志 sink 由外部注入；工具层只调用函数指针，不认识具体 UART 或控制台。

### 4.2 接口层

| 模块 | 职责 |
|---|---|
| hal_time | 单调时钟 |
| hal_i2c | I2C 原始事务 |
| hal_net | 面向字节流的网络传输 |
| hal_uart | UART 原始收发 |
| hal_spi | SPI 原始事务 |
| hal_gpio | GPIO 基本操作 |
| ports/host_sim | PC 模拟实现 |
| ports/template | 新 MCU Port 模板 |
| ports/stm32f4xx | 默认参考 MCU Port 边界 |

接口层只表达硬件能力，不解释器件寄存器、MQTT 报文、温度换算或 GUI 语义。

### 4.3 协议层

| 模块 | 职责 |
|---|---|
| proto_clock | 向功能层提供单调时间 |
| proto_i2c_reg | I2C 寄存器读写事务 |
| proto_temp_tmp75 | TMP75 寄存器、补码和温度换算 |
| proto_mqtt | MQTT 3.1.1 QoS 0 编解码、连接和有界发送队列 |
| proto_display | 标准化数据显示接口，适配控制台、串口屏或具体显示后端 |
| proto_board_profile | 将某块板的接口实例装配成协议设备实例 |

器件地址、寄存器格式和协议超时属于协议层；引脚号、外设句柄和底层驱动对象属于接口层 Port。

### 4.4 功能层

| 模块 | 职责 |
|---|---|
| func_sensor_core | Sensor 静态注册表、生命周期、轮询和最新快照 |
| func_event_queue | 每消费者独立的有界事件队列 |
| func_filter_chain | 通用滤波链执行器 |
| func_filter_ma | 滑动平均 |
| func_filter_ema | 定点指数平均 |
| func_filter_median | 中值滤波 |
| func_filter_limit | 幅值和变化率限制 |
| func_temp_driver | 温度采样周期、标定、滤波和质量状态 |
| func_app_gui | GUI 数据模型更新 |
| func_app_mqtt | Sensor 事件到 MQTT 消息的映射 |
| func_app_biz | 报警、统计和趋势计算 |
| func_app_runtime | 初始化顺序和协作式主循环 |

GUI、MQTT 和业务模块之间禁止互相包含头文件或直接调用。

### 4.5 非运行时文件

以下文件不属于运行时四层模块，但必须纳入独立规则：

| 文件类型 | 规则 |
|---|---|
| CMake 和工具链文件 | 只负责选择 Port、模块和配置 |
| tests | 可以使用 mock，但生产代码不得反向依赖 tests |
| examples/host_demo/main.c | 按功能层依赖规则检查 |
| tools/check_layers.py | 构建工具，不参与目标固件 |
| doc | 设计文档，不参与编译 |

配置按层拆分为 util_cfg.h、hal_cfg.h、proto_cfg.h 和 func_cfg.h，避免单个全局配置头把所有层耦合在一起。

---

## 5. 标准接口草案

以下代码仅定义接口方向，不是本阶段的实现代码。所有正式头文件必须自行包含 stdint.h、stddef.h 和需要的标准类型声明。

### 5.1 统一状态码

~~~c
typedef int32_t sns_status_t;

#define SNS_OK                 ((sns_status_t)0)
#define SNS_ERR_PARAM          ((sns_status_t)-1)
#define SNS_ERR_STATE          ((sns_status_t)-2)
#define SNS_ERR_NOT_FOUND      ((sns_status_t)-3)
#define SNS_ERR_NOT_READY      ((sns_status_t)-4)
#define SNS_ERR_TIMEOUT        ((sns_status_t)-5)
#define SNS_ERR_IO             ((sns_status_t)-6)
#define SNS_ERR_CRC            ((sns_status_t)-7)
#define SNS_ERR_NO_SPACE       ((sns_status_t)-8)
#define SNS_ERR_UNSUPPORTED    ((sns_status_t)-9)
#define SNS_ERR_INVALID_DATA   ((sns_status_t)-10)
~~~

规则：

- 所有可能失败的 API 返回 sns_status_t；
- 数据通过输出参数返回；
- 不使用特殊浮点值表达错误；
- 上层可以增加上下文，但不得丢失底层错误分类。

### 5.2 实例化 HAL

~~~c
typedef struct hal_i2c hal_i2c_t;

typedef struct {
    sns_status_t (*transfer)(void *ctx,
                             uint16_t address,
                             const uint8_t *tx,
                             uint16_t tx_len,
                             uint8_t *rx,
                             uint16_t rx_len,
                             uint32_t timeout_ms);
} hal_i2c_ops_t;

struct hal_i2c {
    const hal_i2c_ops_t *ops;
    void *ctx;
};
~~~

每个 Port 静态创建自己的 ctx 和 hal_i2c_t。协议层只持有 hal_i2c_t 指针，不知道厂商 SDK 句柄类型。

网络和显示底层使用同样的“ops + ctx”模式，不使用隐式全局连接。

### 5.3 I2C 协议设备

~~~c
typedef struct {
    hal_i2c_t *bus;
    uint16_t address;
    uint8_t address_bits;
    uint32_t timeout_ms;
} proto_i2c_device_t;

sns_status_t proto_i2c_reg_read(proto_i2c_device_t *dev,
                                const uint8_t *reg,
                                uint8_t reg_len,
                                uint8_t *data,
                                uint16_t data_len);
~~~

该对象显式包含总线和设备地址，因此可以同时支持多总线和多个同型号器件。

### 5.4 标准温度协议

~~~c
typedef struct proto_temp_device proto_temp_device_t;

typedef struct {
    sns_status_t (*init)(void *ctx);
    sns_status_t (*read_mdeg_c)(void *ctx, int32_t *value_mdeg_c);
} proto_temp_ops_t;

struct proto_temp_device {
    const proto_temp_ops_t *ops;
    void *ctx;
};
~~~

func_temp_driver 只依赖 proto_temp_device_t，不包含 I2C、地址或 TMP75 私有结构。

首版器件明确为 TMP75 兼容配置：

- 默认 12 位温度数据；
- 分辨率 0.0625℃；
- 输出统一换算为毫摄氏度；
- 负温度补码、量程边界和分辨率配置必须有测试向量。

若实际选用其他 LM75 系列变体，必须新增明确的器件 profile，不能继续使用含糊的“LM75 12bit/0.125℃”描述。

### 5.5 滤波器与静态状态

~~~c
typedef int32_t func_filter_value_t;

typedef struct {
    sns_status_t (*init)(void *state,
                         uint16_t state_size,
                         const void *cfg);
    sns_status_t (*reset)(void *state, const void *cfg);
    sns_status_t (*process)(void *state,
                            func_filter_value_t input,
                            func_filter_value_t *output);
} func_filter_ops_t;

typedef struct {
    const func_filter_ops_t *ops;
    void *state;
    uint16_t state_size;
    const void *cfg;
} func_filter_instance_t;

typedef struct {
    func_filter_instance_t *items;
    uint8_t count;
} func_filter_chain_t;
~~~

每个具体滤波模块公开自己的配置类型和状态类型。例如调用方静态声明 func_filter_ma_state_t，不依赖不透明对象的未知大小。

滤波链规则：

- 初始化时检查 ops、cfg、状态地址和状态大小；
- process 使用输出参数并传播错误；
- 任一级失败时停止后续滤波，原始最新有效值不被覆盖；
- 支持零级滤波，即原值直通；
- 最大链长由 func_cfg.h 限制；
- 不使用自动注册、构造函数或编译器私有链接段。

### 5.6 Sensor 事件

~~~c
typedef uint16_t func_sensor_id_t;

typedef enum {
    FUNC_MEAS_TEMPERATURE = 0
} func_measurement_kind_t;

typedef enum {
    FUNC_UNIT_MDEG_C = 0
} func_measurement_unit_t;

typedef enum {
    FUNC_QUALITY_VALID = 0,
    FUNC_QUALITY_STALE,
    FUNC_QUALITY_ERROR
} func_quality_t;

typedef struct {
    func_sensor_id_t sensor_id;
    func_measurement_kind_t kind;
    func_measurement_unit_t unit;
    int32_t value;
    uint32_t timestamp_ms;
    uint32_t sequence;
    func_quality_t quality;
    sns_status_t status;
} func_sensor_event_t;
~~~

sensor_id 标识实例；kind 标识测量物理量。TMP75、NTC 和热电偶都属于温度测量，不再把 NTC 当成独立测量类型。

### 5.7 Sensor 驱动和核心

~~~c
typedef struct {
    sns_status_t (*init)(void *ctx);
    sns_status_t (*poll)(void *ctx,
                         uint32_t now_ms,
                         func_sensor_event_t *event,
                         uint8_t *event_ready);
    sns_status_t (*deinit)(void *ctx);
} func_sensor_driver_ops_t;

typedef struct {
    func_sensor_id_t id;
    const char *name;
    const func_sensor_driver_ops_t *ops;
    void *driver_ctx;
} func_sensor_registration_t;

sns_status_t func_sensor_register(
    const func_sensor_registration_t *registration);

sns_status_t func_sensor_poll_all(uint32_t now_ms);

sns_status_t func_sensor_get_latest(
    func_sensor_id_t id,
    func_sensor_event_t *snapshot);
~~~

Sensor 核心从内部静态注册表分配槽位，不要求调用方知道核心私有对象大小。driver_ctx 由具体功能驱动提供静态存储。

### 5.8 每消费者独立事件队列

~~~c
typedef enum {
    FUNC_QUEUE_DROP_NEWEST = 0,
    FUNC_QUEUE_DROP_OLDEST
} func_queue_overflow_policy_t;

typedef struct {
    func_sensor_event_t *storage;
    uint16_t capacity;
    uint16_t read_index;
    uint16_t write_index;
    uint16_t count;
    uint32_t dropped;
    func_queue_overflow_policy_t overflow_policy;
} func_event_queue_t;

sns_status_t func_sensor_subscribe(func_sensor_id_t sensor_id,
                                   func_event_queue_t *queue);

sns_status_t func_event_queue_pop(func_event_queue_t *queue,
                                  func_sensor_event_t *event);
~~~

发布时复制完整事件到每个订阅队列。GUI、MQTT 或业务模块处理缓慢时，不会在 Sensor 发布路径内执行其业务逻辑。

队列满时按配置丢弃最旧或最新事件，同时增加 dropped 计数。不得静默覆盖且不计数。

---

## 6. 温度 Sensor 案例

### 6.1 静态配置

~~~c
typedef struct {
    proto_temp_device_t *source;
    uint32_t sample_period_ms;
    int32_t calibration_gain_ppm;
    int32_t calibration_offset_mdeg_c;
    func_filter_chain_t filters;
    int32_t publish_change_mdeg_c;
    uint32_t force_publish_period_ms;
} func_temp_cfg_t;
~~~

该配置不出现 I2C 总线号、寄存器或器件地址。这些信息已经封装在协议层 proto_temp_device_t 中。

### 6.2 处理顺序

~~~
到达采样周期
  → proto_temp_device.read_mdeg_c
  → 合法范围检查
  → 定点标定
  → 滤波链
  → 变化阈值或强制周期判断
  → 构造完整事件
  → 复制到 GUI、MQTT、业务三个独立队列
~~~

### 6.3 错误行为

- 首次读取失败：发布 FUNC_QUALITY_ERROR，status 保存错误原因；
- 已有成功值后暂时读取失败：保留最近值，发布 FUNC_QUALITY_STALE；
- 连续失败次数达到配置阈值：质量升级为 FUNC_QUALITY_ERROR；
- 滤波失败：不覆盖最新有效值，发布错误状态；
- uint32_t 时间回绕：所有周期判断使用无符号差值；
- 配置为零采样周期、空 source 或无效滤波状态：初始化失败。

---

## 7. GUI、MQTT 和业务计算解耦

### 7.1 结构解耦

每个应用拥有自己的静态事件队列：

~~~
func_temp_driver
      │
      ▼
func_sensor_core
      │ 复制事件
      ├────────► gui_queue  ──► func_app_gui  ──► proto_display
      ├────────► mqtt_queue ──► func_app_mqtt ──► proto_mqtt
      └────────► biz_queue  ──► func_app_biz
~~~

约束：

- func_sensor_core 不包含任何 func_app_ 头文件；
- 三个应用模块互不调用；
- 删除任一应用只修改功能层装配配置；
- 每个应用可以配置不同的队列容量和溢出策略；
- 一个应用失败不会改变其他应用的队列内容。

### 7.2 GUI

func_app_gui 每次 poll 最多消费配置数量的事件，更新显示模型，然后调用 proto_display。首版显示内容为：

- Sensor 名称或 ID；
- 温度值；
- 单位；
- 数据质量；
- 最近更新时间。

proto_display 负责适配 host 控制台或具体显示后端；GUI 模块不包含 hal_spi、hal_uart、LVGL Port 等接口层头文件。

### 7.3 MQTT

func_app_mqtt 将事件转换为固定上限 JSON：

~~~json
{
  "sensor_id": 1,
  "kind": "temperature",
  "value_mdeg_c": 25125,
  "quality": "valid",
  "timestamp_ms": 123456,
  "sequence": 42
}
~~~

首版 MQTT 范围：

- MQTT 3.1.1；
- QoS 0 publish；
- 固定容量发送队列；
- 非阻塞状态机或单次 poll 时间预算；
- 自动重连采用有上限退避；
- TLS 不在 MQTT 模块实现，未来可由 hal_net 的安全传输 Port 提供；
- 发送队列满时返回 SNS_ERR_NO_SPACE，并记录丢弃计数。

proto_mqtt_publish_enqueue 只复制数据到协议层静态队列；真正网络收发由 proto_mqtt_poll 执行。

### 7.4 业务计算

func_app_biz 独立实现：

- 带迟滞的高低温报警；
- 固定窗口 min、max、avg；
- 可选趋势斜率；
- 对 STALE 和 ERROR 数据采用单独策略；
- 业务结果通过其自身输出接口上报，不回调 GUI 或 MQTT 模块。

若业务结果也需要发送 MQTT，应将其发布为新的标准功能事件，由 MQTT 应用订阅，而不是让业务模块直接调用 func_app_mqtt。

---

## 8. 主循环与运行时预算

推荐协作式主循环：

~~~c
for (;;) {
    uint32_t now_ms;

    proto_clock_now_ms(&now_ms);
    func_sensor_poll_all(now_ms);

    func_app_gui_poll(now_ms);
    func_app_biz_poll(now_ms);
    func_app_mqtt_poll(now_ms);

    proto_mqtt_poll(mqtt_client, now_ms, MQTT_POLL_BUDGET_MS);
}
~~~

这里的代码只表示调用顺序。正式实现必须检查每个返回值。

时间预算要求：

- Sensor 轮询优先于应用消费；
- 每个应用单次最多消费固定数量事件；
- MQTT 单次 poll 有最大时间预算；
- HAL 超时必须可配置且有上限；
- 任何应用不得在事件发布路径内执行格式化、网络发送或显示刷新。

---

## 9. 跨 MCU Port 设计

### 9.1 目录

~~~
src/
├─ tool/
├─ iface/
├─ proto/
│  └─ profile/
├─ func/
│  ├─ filter/
│  └─ app/
└─ ports/
   ├─ template/
   ├─ host_sim/
   └─ stm32f4xx/
~~~

### 9.2 Port 边界

新增 MCU 时：

1. 复制 ports/template；
2. 实现本项目实际启用的 hal_time、hal_i2c、hal_net、hal_uart、hal_spi、hal_gpio；
3. 在接口层 Port 中静态创建 HAL ctx 和实例句柄；
4. 在协议层 profile 中配置总线实例、器件地址和协议实例；
5. 选择功能层静态 Sensor、滤波器和应用配置；
6. 运行分层检查和目标板自检。

框架核心不得包含：

- 厂商 SDK 头文件；
- 寄存器地址；
- 编译器中断关键字；
- RTOS 类型；
- 平台 socket 类型；
- 依赖字节序或未对齐访问的强制类型转换。

### 9.3 参考平台

首版默认交付：

- host_sim：用于全部单元测试和端到端演示；
- ports/template：新 MCU 的完整接口契约模板；
- stm32f4xx：作为默认参考 Port 边界；实际编译取决于项目提供的厂商 SDK。

若评审时指定其他 MCU，参考 Port 改为指定平台，框架接口不变。

---

## 10. 静态资源配置

配置按层拆分。默认建议值如下：

~~~c
/* func_cfg.h */
#define FUNC_CFG_MAX_SENSORS             4U
#define FUNC_CFG_MAX_SUBSCRIPTIONS       12U
#define FUNC_CFG_MAX_FILTERS_PER_SENSOR  4U
#define FUNC_CFG_GUI_QUEUE_CAPACITY       8U
#define FUNC_CFG_MQTT_QUEUE_CAPACITY     16U
#define FUNC_CFG_BIZ_QUEUE_CAPACITY      16U

/* proto_cfg.h */
#define PROTO_CFG_MQTT_TX_QUEUE_CAPACITY 8U
#define PROTO_CFG_MQTT_MAX_PACKET        512U
#define PROTO_CFG_MQTT_POLL_BUDGET_MS    2U
~~~

所有数组大小必须在编译期确定。初始化时仍需检查调用方实际提供的容量，不能只依赖宏。

---

## 11. 构建期分层检查

tools/check_layers.py 至少执行以下检查：

1. 根据目录和文件前缀确定模块层级；
2. 扫描所有生产 .c 和 .h 的 include；
3. 按依赖矩阵拒绝非法 include；
4. 拒绝功能层出现 hal_、port_、plat_ 符号前缀；
5. 拒绝接口层出现 proto_、func_ 符号前缀；
6. 拒绝工具层出现任何上层符号前缀；
7. 检查每个运行时源文件只能位于一个层级目录；
8. 检查公共头文件自包含；
9. 在 CI 中使用 -std=c99 -Wall -Wextra -Werror；
10. 对 Clang 或 GCC 额外启用可用的原型、转换和阴影告警。

脚本检查不是唯一保证。CMake 每层建立独立 target，并只暴露允许的 include 目录，形成第二道约束。

---

## 12. 测试策略

### 12.1 工具层

- 环形缓冲空、满和回绕；
- 两种队列溢出策略；
- 定点饱和运算；
- 大小端和未对齐字节输入；
- 状态码映射。

### 12.2 接口层

- 每个 host_sim HAL 可以创建两个以上独立实例；
- I2C 成功、NACK、超时和短传输；
- 网络分段发送、短接收、断连和重连；
- 单调时间回绕模拟。

### 12.3 协议层

- TMP75 正温、负温、零值、最小值和最大值；
- 12 位分辨率测试向量；
- 多 I2C 总线、多器件地址；
- MQTT CONNECT、PUBLISH、PING 报文黄金字节；
- MQTT 分包、队列满、断线和退避；
- display mock 调用参数。

### 12.4 功能层

- 每个滤波器的数值正确性和边界；
- 滤波链零级、单级、多级和中途失败；
- 两个同类温度 Sensor 通过 sensor_id 区分；
- 采样周期和 uint32_t 时间回绕；
- 校准溢出；
- VALID、STALE、ERROR 状态转换；
- 事件 fan-out 到三个独立队列；
- 某个队列满时其他消费者仍正常；
- GUI、MQTT、业务模块互不调用。

### 12.5 端到端验收

host_demo 同时模拟两个 TMP75：

- 位于不同 I2C 总线；
- 使用不同滤波链；
- 事件分别进入 GUI、MQTT 和业务队列；
- GUI 输出最新值与质量；
- MQTT 生成确定的 QoS 0 报文；
- 业务模块产生带迟滞报警和窗口统计；
- 强制制造 I2C 错误、MQTT 断线和队列溢出；
- 全程无动态内存；
- 分层检查和全部测试通过。

---

## 13. 实施计划

| 阶段 | 内容 | 验收标准 |
|---|---|---|
| P0 | 目录、四层 target、配置头、分层检查 | 空工程 C99 构建通过，非法依赖测试能失败 |
| P1 | util_status、util_ringbuf、util_math、util_byte、util_log | 工具层边界测试通过 |
| P2 | 实例化 HAL、host_sim、template | 同类型 HAL 多实例测试通过 |
| P3 | proto_clock、proto_i2c_reg、proto_temp_tmp75、板级 profile | 两总线两设备协议测试通过 |
| P4 | 滤波 ops、静态状态、四种基础滤波和滤波链 | 无 malloc，数值与错误测试通过 |
| P5 | Sensor 核心、温度驱动、事件队列 | 多 Sensor、时间回绕、质量状态和 fan-out 通过 |
| P6 | GUI、MQTT、业务应用及对应协议模块 | 三应用独立裁剪，慢消费者隔离通过 |
| P7 | host_demo、参考 Port 边界、移植说明和 CI | 全部验收项通过 |

每阶段完成后先运行本阶段单测、全量回归和分层检查，再进入下一阶段。

---

## 14. 接口稳定性要求

公开接口必须满足：

- 首参数为实例或上下文，禁止隐式单例；
- 输入指针尽量 const；
- 所有缓冲区同时传递容量；
- 所有失败使用 sns_status_t；
- 值通过输出参数返回；
- 明确对象所有权和生命周期；
- 明确函数是否可能阻塞及最大超时；
- 明确线程模型；
- 公共结构只包含固定宽度整数或明确的句柄；
- 不把厂商 SDK 类型暴露到接口层以上；
- 不用 double 作为 Sensor 公共数据格式；
- 不使用编译器私有自动注册机制。

修改公开接口时，必须同步更新：

- 头文件契约；
- mock；
- host_sim；
- 至少一个协议调用方；
- 单元测试；
- 本设计或移植文档。

---

## 15. 编码前验收清单

只有以下条件全部满足，才允许开始 P0：

- [ ] 评审确认四层依赖矩阵；
- [ ] 确认功能层不直接调用 plat_os 或 hal_time；
- [ ] 确认使用实例句柄而不是全局单例；
- [ ] 确认公开温度值使用 int32_t 毫摄氏度；
- [ ] 确认滤波状态由调用方静态提供；
- [ ] 确认使用直接 ops 引用，不做自动注册；
- [ ] 确认 GUI、MQTT、业务使用独立有界队列；
- [ ] 确认首版 MQTT 仅实现 QoS 0；
- [ ] 确认首个温度器件采用 TMP75 兼容 12 位 profile；
- [ ] 确认目标参考 MCU 或接受默认 stm32f4xx 边界；
- [ ] 确认本实施计划。

如无额外修改意见，以上条目按本文默认方案批准后进入 P0。当前仍停留在设计评审阶段，不创建或实现任何 C 模块。
