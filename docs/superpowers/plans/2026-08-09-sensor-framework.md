# Cross-MCU Sensor Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. For every production task, a test subagent writes and verifies the failing contract tests first, then a different implementation subagent writes the production code.

**Goal:** Build, test, document, demonstrate, and publish a portable C99 Sensor framework using TMP75 temperature devices, pluggable fixed-point filters, and independently queued GUI, MQTT, and business consumers.

**Architecture:** Runtime C modules are split into tool, interface, protocol, and function layers with a mechanically enforced dependency matrix. Hardware and protocol objects use explicit ops-plus-context instance handles, all memory is statically supplied, and the cooperative runtime distributes copied events into bounded per-consumer queues.

**Tech Stack:** C99, CMake 3.20+, CTest, Ninja, GCC or Clang with warnings as errors, Python 3 for the layer checker, Git.

## Global Constraints

- Production language is pure C99; compile with -std=c99, -Wall, -Wextra, -Werror, -Wpedantic.
- Production code must not call malloc, calloc, realloc, or free.
- Function layer may depend only on function, protocol, and tool layers.
- Protocol layer may depend only on protocol, interface, and tool layers.
- Interface layer may depend only on interface and tool layers.
- Tool layer may depend only on tool layer.
- Every runtime C module belongs to exactly one layer.
- All HAL, protocol clients, and devices use explicit ops-plus-context instance handles; no implicit active global instance is allowed.
- Public temperature values use signed int32_t millidegrees Celsius.
- Framework APIs use sns_status_t and output parameters for fallible results.
- Sensor core uses a single-owner cooperative execution model and does not call plat_os.
- All queues, filter state, protocol buffers, and driver state use caller-provided or compile-time static storage.
- Filters are selected by direct ops references; compiler constructors, linker-section registration, and central type switches are forbidden.
- GUI, MQTT, and business applications own separate bounded event queues and do not call one another.
- MQTT scope is MQTT 3.1.1 QoS 0 publish, bounded TX queue, keepalive, reconnect backoff, and no TLS implementation in the protocol module.
- Primary device profile is TMP75-compatible 12-bit data at 0.0625 degrees Celsius per LSB, rounded to nearest millidegree with halves away from zero.
- Unit tests assert behavior with literal hand-derived expected values and exercise real production code.
- A test subagent and a distinct implementation subagent are used for every production task; agents run sequentially to avoid shared-tree conflicts.
- The main agent independently inspects diffs and runs focused and full verification after subagent reviews.
- No push occurs until the full build, CTest suite, layer checker, and host demo all succeed from a clean build directory.

---

## File Map

### Build and public configuration

- Create CMakeLists.txt: layer libraries, test executables, demo, warnings, and CTest registration.
- Create cmake/CompilerWarnings.cmake: strict portable warning flags.
- Create src/tool/util_cfg.h: tool capacities only.
- Create src/iface/hal_cfg.h: interface timeouts and host-port limits only.
- Create src/proto/proto_cfg.h: protocol packet, queue, and retry limits only.
- Create src/func/func_cfg.h: Sensor, subscription, filter, and application limits only.

### Tool layer

- Create src/tool/util_status.h and .c: sns_status_t and readable names.
- Create src/tool/util_byte.h and .c: explicit big-endian reads and writes.
- Create src/tool/util_math.h and .c: saturation and rounded integer division.
- Create src/tool/util_ringbuf.h and .c: caller-storage fixed-item ring buffer.
- Create src/tool/util_log.h and .c: injected sink with bounded formatting.

### Interface layer and ports

- Create src/iface/hal_time.h, hal_i2c.h, and hal_net.h: ops-plus-context contracts.
- Create src/iface/hal_uart.h, hal_spi.h, and hal_gpio.h: standardized extension contracts.
- Create src/ports/host_sim/host_time.h and .c: deterministic controllable clock.
- Create src/ports/host_sim/host_i2c.h and .c: multi-bus fake device transactions.
- Create src/ports/host_sim/host_net.h and .c: deterministic connect/send/receive capture.
- Create src/ports/template/README.md and interface implementation templates.
- Create src/ports/stm32f4xx/README.md: exact SDK binding points without claiming an unverified SDK build.

### Protocol layer

- Create src/proto/proto_clock.h and .c: monotonic clock adapter.
- Create src/proto/proto_i2c_reg.h and .c: register transfer helper.
- Create src/proto/proto_temp.h: generic temperature device contract.
- Create src/proto/proto_temp_tmp75.h and .c: TMP75 profile and conversion.
- Create src/proto/proto_display.h and .c: ops-plus-context display contract.
- Create src/proto/proto_mqtt.h and .c: MQTT 3.1.1 QoS 0 state and packet engine.

### Function layer

- Create src/func/filter/func_filter.h and .c: chain orchestration.
- Create src/func/filter/func_filter_ma.h and .c: moving average.
- Create src/func/filter/func_filter_ema.h and .c: Q15 exponential average.
- Create src/func/filter/func_filter_median.h and .c: median window.
- Create src/func/filter/func_filter_limit.h and .c: range and max-step limiter.
- Create src/func/func_sensor_types.h: shared Sensor identifiers, units, quality, and event value types.
- Create src/func/func_event_queue.h and .c: Sensor-event queue facade.
- Create src/func/func_sensor.h and .c: static registry, subscriptions, latest snapshots, and polling.
- Create src/func/func_temp.h and .c: periodic temperature driver, calibration, quality, filters, and publication policy.
- Create src/func/app/func_app_gui.h and .c: queue consumer and display model.
- Create src/func/app/func_app_biz.h and .c: alarm and fixed-window statistics.
- Create src/func/app/func_app_mqtt.h and .c: deterministic JSON and MQTT enqueue.
- Create src/func/app/func_app_runtime.h and .c: bounded cooperative poll ordering.

### Tests, checks, and demo

- Create tests/test_support.h: minimal assertion and test-runner macros.
- Create tests/tool, tests/iface, tests/proto, tests/func, and tests/integration focused test files.
- Create tests/support/fake_display.h and .c: protocol-facing capture fake.
- Create tools/check_layers.py: directory, include, and forbidden-symbol validation.
- Create tools/test_check_layers.py: behavior tests for valid and invalid fixtures.
- Create examples/host_demo/main.c: two TMP75 devices on different buses and three consumers.

### Main-agent documentation

- Create README.md: architecture, prerequisites, build, test, demo, porting, API example, and limitations.
- Create CHANGELOG.md: v0.1.0 delivered scope.
- Create doc/NEXT_VERSION_RECOMMENDATIONS.md: prioritized v0.2 recommendations.

---

### Task 1: Build Skeleton and Tool Layer

**Files:**
- Create: CMakeLists.txt
- Create: cmake/CompilerWarnings.cmake
- Create: src/tool/util_cfg.h
- Create: src/tool/util_status.h
- Create: src/tool/util_status.c
- Create: src/tool/util_byte.h
- Create: src/tool/util_byte.c
- Create: src/tool/util_math.h
- Create: src/tool/util_math.c
- Create: src/tool/util_ringbuf.h
- Create: src/tool/util_ringbuf.c
- Create: src/tool/util_log.h
- Create: src/tool/util_log.c
- Create: tests/test_support.h
- Create: tests/tool/test_tool.c

**Interfaces:**
- Produces: sns_status_t and SNS_OK through SNS_ERR_INVALID_DATA.
- Produces: util_be16_read, util_be16_write, util_div_round_nearest_i64, util_sat_i64_to_i32.
- Produces: util_ringbuf_init, util_ringbuf_push, util_ringbuf_pop, util_ringbuf_count, util_ringbuf_dropped.
- Produces: util_log_init and util_log_write with caller-provided sink.

- [ ] **Step 1: Test subagent writes the tool contract tests**

Create tests/tool/test_tool.c with literal cases for:

~~~c
TEST(status_name_maps_known_and_unknown_codes);
TEST(be16_round_trip_uses_network_order);
TEST(rounded_division_rounds_positive_and_negative_halves_away_from_zero);
TEST(saturation_clamps_both_int32_boundaries);
TEST(ringbuf_drop_newest_preserves_existing_items_and_counts_drop);
TEST(ringbuf_drop_oldest_replaces_oldest_item_and_counts_drop);
TEST(log_sink_receives_level_tag_and_formatted_message);
~~~

The ring buffer stores fixed-size items in memory passed to init. Push on full returns SNS_ERR_NO_SPACE for drop-newest and SNS_OK for drop-oldest; both increment dropped.

- [ ] **Step 2: Test subagent configures and verifies RED**

Run:

~~~powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_tool
~~~

Expected RED: compilation fails because tool headers or symbols do not exist. The test agent records the exact missing contract in its report and commits only build/test files.

- [ ] **Step 3: Implementation subagent writes the minimal tool layer**

Use no heap allocation. util_ringbuf_t contains storage, item_size, capacity, indices, count, dropped, and overflow policy. util_log_write uses vsnprintf into a fixed UTIL_CFG_LOG_LINE_MAX buffer and always terminates it.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run:

~~~powershell
cmake --build build --target test_tool
ctest --test-dir build -R tool --output-on-failure
~~~

Expected GREEN: all tool tests pass with no compiler warnings.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define tool layer contracts

Implementation commit subject: feat: add portable tool layer

Main agent generates a review package, dispatches a task reviewer, resolves findings through the prescribed fix loop, then independently runs the focused test.

---

### Task 2: Interface Contracts and Host Simulation

**Files:**
- Create: src/iface/hal_cfg.h
- Create: src/iface/hal_time.h
- Create: src/iface/hal_i2c.h
- Create: src/iface/hal_net.h
- Create: src/iface/hal_uart.h
- Create: src/iface/hal_spi.h
- Create: src/iface/hal_gpio.h
- Create: src/ports/host_sim/host_time.h
- Create: src/ports/host_sim/host_time.c
- Create: src/ports/host_sim/host_i2c.h
- Create: src/ports/host_sim/host_i2c.c
- Create: src/ports/host_sim/host_net.h
- Create: src/ports/host_sim/host_net.c
- Create: tests/iface/test_iface.c

**Interfaces:**
- Produces: hal_time_t with now_ms ops.
- Produces: hal_i2c_t with transfer ops and explicit address, buffers, lengths, and timeout.
- Produces: hal_net_t with connect, send, recv, close ops and ctx.
- Produces: controllable host_time_t, two independent host_i2c_bus_t objects, and captured host_net_t packets.

- [ ] **Step 1: Test subagent writes host instance tests**

Tests must prove:

~~~c
TEST(two_host_clocks_advance_independently);
TEST(two_i2c_buses_with_same_address_return_different_data);
TEST(i2c_propagates_nack_timeout_and_short_transfer);
TEST(two_network_instances_capture_packets_independently);
TEST(network_disconnect_and_reconnect_are_observable);
~~~

The fake I2C registration contract accepts a device callback and ctx. Tests derive expected bytes from local literal fixtures, not production conversion helpers.

- [ ] **Step 2: Test subagent verifies RED and commits**

Run test_iface target. Expected RED is missing interface/host symbols.

- [ ] **Step 3: Implementation subagent implements explicit instances**

No active global bus, clock, or connection is allowed. All host fake state is stored inside caller-owned context structures. Extension HAL headers are contracts only and must remain self-contained.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run test_iface and the existing tool tests. Expected: all pass without warnings.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define HAL instance contracts

Implementation commit subject: feat: add instance HAL and host simulation

---

### Task 3: Clock, I2C Register, and TMP75 Protocols

**Files:**
- Create: src/proto/proto_cfg.h
- Create: src/proto/proto_clock.h
- Create: src/proto/proto_clock.c
- Create: src/proto/proto_i2c_reg.h
- Create: src/proto/proto_i2c_reg.c
- Create: src/proto/proto_temp.h
- Create: src/proto/proto_temp_tmp75.h
- Create: src/proto/proto_temp_tmp75.c
- Create: tests/proto/test_temp_protocol.c

**Interfaces:**
- Consumes: hal_time_t and hal_i2c_t.
- Produces: proto_clock_now_ms(proto_clock_t *, uint32_t *).
- Produces: proto_i2c_reg_read and proto_i2c_reg_write on proto_i2c_device_t.
- Produces: proto_temp_device_t with init and read_mdeg_c ops.

- [ ] **Step 1: Test subagent writes literal protocol tests**

Required TMP75 conversion fixtures:

| Register bytes | Signed 12-bit code | Expected mdegC |
|---|---:|---:|
| 0x00 0x00 | 0 | 0 |
| 0x00 0x10 | 1 | 63 |
| 0x19 0x00 | 400 | 25000 |
| 0xFF 0xF0 | -1 | -63 |
| 0xF6 0x00 | -160 | -10000 |

Tests:

~~~c
TEST(clock_adapter_returns_bound_instance_time);
TEST(register_read_writes_register_then_reads_bytes);
TEST(register_error_is_propagated_without_output_mutation);
TEST(tmp75_converts_positive_negative_and_fractional_values);
TEST(tmp75_instances_on_two_buses_do_not_share_state);
~~~

- [ ] **Step 2: Test subagent verifies RED and commits**

Expected RED: missing protocol symbols while prior task tests remain green.

- [ ] **Step 3: Implementation subagent implements protocol adapters**

TMP75 conversion extracts the signed upper 12 bits. Convert code times 625 divided by 10 using util_div_round_nearest_i64, so half millidegrees round away from zero. The read output remains unchanged on failure.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run protocol, interface, and tool tests.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define temperature protocol behavior

Implementation commit subject: feat: add clock I2C and TMP75 protocols

---

### Task 4: Pluggable Fixed-Point Filter Chain

**Files:**
- Create: src/func/func_cfg.h
- Create: src/func/filter/func_filter.h
- Create: src/func/filter/func_filter.c
- Create: src/func/filter/func_filter_ma.h
- Create: src/func/filter/func_filter_ma.c
- Create: src/func/filter/func_filter_ema.h
- Create: src/func/filter/func_filter_ema.c
- Create: src/func/filter/func_filter_median.h
- Create: src/func/filter/func_filter_median.c
- Create: src/func/filter/func_filter_limit.h
- Create: src/func/filter/func_filter_limit.c
- Create: tests/func/test_filter.c

**Interfaces:**
- Produces: func_filter_ops_t, func_filter_instance_t, func_filter_chain_t.
- Produces: func_filter_chain_init, func_filter_chain_reset, func_filter_chain_process.
- Produces: direct exported ops objects func_filter_ma_ops, func_filter_ema_ops, func_filter_median_ops, func_filter_limit_ops.

- [ ] **Step 1: Test subagent writes filter behavior tests**

Literal sequences:

- MA window 3 over 1000, 2000, 6000 produces 1000, 1500, 3000.
- EMA alpha Q15 16384 over 0 then 1000 produces 0 then 500.
- Median window 3 over 100, 9000, 200 produces 100, 4550, 200 using the median of available samples and average of the two middle values for even count.
- Limit min -40000, max 125000, max step 5000 maps 20000 then 40000 to 20000 then 25000.
- Chain limit then MA produces hand-derived expected outputs.

Tests also cover invalid alpha, zero window, insufficient state size, null output, zero-stage pass-through, reset, and a custom test ops proving no central type switch is required.

- [ ] **Step 2: Test subagent verifies RED and commits**

Expected RED: missing filter headers or symbols.

- [ ] **Step 3: Implementation subagent implements minimal filter modules**

Each concrete public state type contains its own fixed array sized by FUNC_CFG_FILTER_MAX_WINDOW. No dynamic allocation. Chain stops at the first failure and leaves output unchanged.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run filter and all earlier tests.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define pluggable filter contracts

Implementation commit subject: feat: add fixed-point filter chain

---

### Task 5: Sensor Core, Event Queues, and Temperature Driver

**Files:**
- Create: src/func/func_sensor_types.h
- Create: src/func/func_event_queue.h
- Create: src/func/func_event_queue.c
- Create: src/func/func_sensor.h
- Create: src/func/func_sensor.c
- Create: src/func/func_temp.h
- Create: src/func/func_temp.c
- Create: tests/func/test_sensor.c
- Create: tests/func/test_temp_driver.c

**Interfaces:**
- Consumes: proto_temp_device_t and func_filter_chain_t.
- Produces: func_sensor_types.h containing func_sensor_event_t with sensor_id, kind, unit, value, timestamp_ms, sequence, quality, and status; both queue and core headers depend on this same-layer type header without circular inclusion.
- Produces: func_sensor_core_init, func_sensor_register, func_sensor_subscribe, func_sensor_poll_all, func_sensor_get_latest.
- Produces: func_temp_ctx_t and func_temp_driver_ops.

- [ ] **Step 1: Test subagent writes core and driver tests**

Core tests:

~~~c
TEST(two_temperature_sensors_remain_distinguishable_by_id);
TEST(publish_copies_event_to_three_independent_queues);
TEST(full_gui_queue_does_not_block_mqtt_or_business_queue);
TEST(latest_snapshot_includes_quality_timestamp_and_status);
TEST(registry_and_subscription_capacity_errors_are_reported);
~~~

Temperature tests:

~~~c
TEST(sample_period_uses_unsigned_time_difference_across_wrap);
TEST(calibration_gain_ppm_and_offset_are_applied_before_filters);
TEST(change_threshold_and_force_period_control_publication);
TEST(first_read_failure_publishes_error);
TEST(failure_after_valid_value_publishes_stale_then_error_at_threshold);
TEST(filter_failure_does_not_replace_last_valid_value);
~~~

Use a real proto_temp_device_t with a deterministic scripted ctx rather than asserting mock call counts.

- [ ] **Step 2: Test subagent verifies RED and commits**

Expected RED: missing Sensor APIs. Tool, interface, protocol, and filter tests stay green.

- [ ] **Step 3: Implementation subagent implements the single-owner core**

Registry and subscriptions are fixed arrays using FUNC_CFG_MAX_SENSORS and FUNC_CFG_MAX_SUBSCRIPTIONS. Queue overflow behavior and dropped count delegate to util_ringbuf. No mutex, plat_os, hal_time, callback-based application execution, or heap allocation.

func_temp polling receives now_ms. Calibration is value plus value times gain_ppm divided by 1000000, then offset, using int64_t and saturation. Read failure quality transitions use a configurable consecutive_failure_error_count.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run Sensor, temperature, filter, protocol, interface, and tool tests.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define Sensor core and temperature behavior

Implementation commit subject: feat: add Sensor core and temperature driver

---

### Task 6: Display, GUI, and Business Consumers

**Files:**
- Create: src/proto/proto_display.h
- Create: src/proto/proto_display.c
- Create: src/func/app/func_app_gui.h
- Create: src/func/app/func_app_gui.c
- Create: src/func/app/func_app_biz.h
- Create: src/func/app/func_app_biz.c
- Create: tests/support/fake_display.h
- Create: tests/support/fake_display.c
- Create: tests/func/test_gui_biz.c

**Interfaces:**
- Produces: proto_display_t with show ops and ctx.
- Produces: func_app_gui_init and func_app_gui_poll with maximum events per poll.
- Produces: func_biz_result_t, func_app_biz_init, func_app_biz_poll, and func_app_biz_get_latest.

- [ ] **Step 1: Test subagent writes consumer tests**

Tests prove:

- GUI formats 25125 mdegC as 25.125 C without floating point.
- GUI shows sensor ID, quality, and timestamp.
- GUI consumes at most configured events per poll.
- High alarm turns on at or above 80000 and off at or below 75000.
- Business min, max, and rounded average over literal window values are correct.
- STALE and ERROR events do not enter numeric statistics but update quality status.
- GUI and business consume different queues and do not change one another.

- [ ] **Step 2: Test subagent verifies RED and commits**

Expected RED: missing display/application symbols.

- [ ] **Step 3: Implementation subagent implements bounded consumers**

GUI writes a proto_display_record_t rather than exposing HAL. Business uses caller-owned fixed window storage and int64_t sum. Neither module includes the other application header.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run gui_biz and all earlier tests.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define GUI and business consumers

Implementation commit subject: feat: add display GUI and business modules

---

### Task 7: MQTT Protocol and MQTT Application

**Files:**
- Create: src/proto/proto_mqtt.h
- Create: src/proto/proto_mqtt.c
- Create: src/func/app/func_app_mqtt.h
- Create: src/func/app/func_app_mqtt.c
- Create: tests/proto/test_mqtt.c
- Create: tests/func/test_mqtt_app.c

**Interfaces:**
- Consumes: hal_net_t and func_event_queue_t.
- Produces: proto_mqtt_client_t with caller-provided packet slots and working buffer.
- Produces: proto_mqtt_init, proto_mqtt_connect, proto_mqtt_publish_enqueue, proto_mqtt_poll, proto_mqtt_close.
- Produces: func_app_mqtt_init and func_app_mqtt_poll.

- [ ] **Step 1: Test subagent writes MQTT golden-byte tests**

Use literal MQTT 3.1.1 packets:

- CONNECT for client ID sensor-demo, clean session, keepalive 30.
- QoS 0 PUBLISH to sensors/temperature with a literal payload.
- PINGREQ bytes 0xC0, 0x00.
- Remaining-length encoding boundaries 127 and 128.

Application tests assert exact JSON for a literal event:

~~~json
{"sensor_id":1,"kind":"temperature","value_mdeg_c":25125,"quality":"valid","timestamp_ms":123456,"sequence":42}
~~~

Also cover packet too large, TX queue full, disconnected enqueue, connect failure backoff, reconnect, keepalive, partial network send, and no source event. Enqueue while disconnected succeeds until the queue is full; queued packets remain ordered and are sent after reconnect. Reconnect backoff starts at 1000 ms and doubles to a 30000 ms cap. With keepalive 30 seconds, an idle connected client enqueues or sends PINGREQ at 30000 ms and treats a missing response at the next keepalive deadline as disconnected.

- [ ] **Step 2: Test subagent verifies RED and commits**

Expected RED: missing MQTT APIs.

- [ ] **Step 3: Implementation subagent implements scoped MQTT**

Implement only MQTT 3.1.1 QoS 0. Buffers and queue slots are caller-owned. poll sends at most one bounded work unit and accepts now_ms. TLS, subscribe, QoS 1/2, retained inbound messages, and heap allocation are out of scope.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run MQTT protocol/application and all earlier tests.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define MQTT protocol and application behavior

Implementation commit subject: feat: add bounded MQTT QoS zero pipeline

---

### Task 8: Runtime, Layer Checker, Templates, and Host Demo

**Files:**
- Create: src/func/app/func_app_runtime.h
- Create: src/func/app/func_app_runtime.c
- Create: tools/check_layers.py
- Create: tools/test_check_layers.py
- Create: tests/integration/test_pipeline.c
- Create: examples/host_demo/main.c
- Create: src/ports/template/README.md
- Create: src/ports/template/hal_time.c
- Create: src/ports/template/hal_i2c.c
- Create: src/ports/template/hal_net.c
- Create: src/ports/stm32f4xx/README.md

**Interfaces:**
- Consumes: all completed public layer APIs.
- Produces: func_app_runtime_poll_once with explicit budgets and no hidden timing.
- Produces: check_layers.py returning zero for valid tree and nonzero with file/line/rule for violations.
- Produces: host_demo executable.

- [ ] **Step 1: Test subagent writes integration and checker tests**

Integration test builds two TMP75 devices at the same address on distinct host buses, different filter chains, and three separate consumer queues. It asserts distinct final values, GUI records, MQTT packets, business results, and queue drop isolation.

tools/test_check_layers.py creates temporary fixture trees and asserts:

- valid tool/interface/protocol/function includes pass;
- function including hal_i2c.h fails;
- interface including proto_temp.h fails;
- tool including func_sensor.h fails;
- function source mentioning hal_i2c_ without an include still fails;
- one source under two layer roots is rejected by configuration validation.

- [ ] **Step 2: Test subagent verifies RED and commits**

Run the integration target and Python checker tests. Expected RED: runtime/checker/demo contracts missing.

- [ ] **Step 3: Implementation subagent completes runtime and demo**

The demo runs a deterministic finite number of iterations and exits zero. It prints:

- two sensor IDs and temperatures;
- GUI quality records;
- business alarm/statistics;
- MQTT packet count and representative packet hex;
- dropped counts.

The template C files compile only in an explicit SENSOR_BUILD_PORT_TEMPLATE target and return SNS_ERR_UNSUPPORTED where hardware binding is required. The stm32f4xx README names the exact HAL functions a real adapter must bind and does not claim it was compiled.

- [ ] **Step 4: Implementation subagent verifies GREEN**

Run:

~~~powershell
cmake --build build
ctest --test-dir build --output-on-failure
python tools/check_layers.py
python tools/test_check_layers.py
build\host_demo.exe
~~~

Expected: every command exits zero and build output has no warnings.

- [ ] **Step 5: Commit and review**

Test commit subject: test: define end-to-end framework behavior

Implementation commit subject: feat: add runtime layer checks and host demo

---

### Task 9: Main-Agent Verification and Documentation

**Files:**
- Create: README.md
- Create: CHANGELOG.md
- Create: doc/NEXT_VERSION_RECOMMENDATIONS.md

**Interfaces:**
- Consumes: verified build commands, actual demo output, public headers, and final review findings.
- Produces: accurate user-facing build, test, demo, architecture, extension, and porting guidance.

- [ ] **Step 1: Main agent performs clean verification before writing claims**

Delete only the resolved absolute build directory inside the feature worktree, reconfigure, rebuild, run all CTest tests, run both layer-check commands, and run host_demo. Capture exact tool versions, test count, and representative output.

- [ ] **Step 2: Main agent writes README.md**

README sections:

1. What the framework does.
2. Four-layer dependency table.
3. Directory map.
4. Prerequisites with tested versions.
5. Copy-paste configure, build, test, layer-check, and demo commands for PowerShell.
6. Expected demo excerpt copied from the verified run.
7. Minimal code example showing a TMP75 protocol device, filter chain, Sensor registration, and three subscriptions.
8. How to add an MCU Port.
9. How to add a temperature protocol.
10. How to add a filter without editing the core.
11. Static-memory and single-owner rules.
12. MQTT scope and known limitations.

- [ ] **Step 3: Main agent writes release and next-version documents**

CHANGELOG.md records v0.1.0 scope and verification.

doc/NEXT_VERSION_RECOMMENDATIONS.md prioritizes:

1. Real STM32F4 SDK adapter and hardware-in-loop CI.
2. Optional TLS transport and MQTT QoS 1.
3. RTOS message adapter while preserving single Sensor owner.
4. SHT3x CRC profile and NTC ADC profile.
5. Additional fixed-point Kalman and low-pass filters.
6. Compile-time resource sizing report and coverage/mutation testing.

Each recommendation includes motivation, design constraint, acceptance criteria, and compatibility impact.

- [ ] **Step 4: Main agent commits documentation**

Commit subject: docs: add build demo and roadmap guide

---

### Task 10: Final Review, Clean Build, and Remote Push

**Files:**
- Modify only files required by final review findings.

- [ ] **Step 1: Dispatch final whole-branch reviewer**

Generate a review package from the feature branch merge base through HEAD. The reviewer checks the approved design, this plan, the complete diff, the SDD ledger, architecture, errors, static-memory guarantees, tests, demo, and documentation.

- [ ] **Step 2: Resolve final findings through one fix wave**

If Critical or Important findings exist, dispatch one fix subagent with the complete findings list. Require focused regression tests and a fix report, then dispatch one scoped re-review. Main agent independently verifies residual findings.

- [ ] **Step 3: Run fresh release verification**

From a clean build directory run:

~~~powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python tools/check_layers.py
python tools/test_check_layers.py
build\host_demo.exe
git status --short
~~~

The first six commands must exit zero. git status may show only intentionally uncommitted ignored build artifacts; all source, tests, docs, and plan files must be committed.

- [ ] **Step 4: Push without rewriting remote history**

Confirm origin still equals git@github.com:kuleyxgy/Interview-and-written-examination.git, fetch remote refs, and check that the chosen remote feature branch is not ahead unexpectedly. Push with:

~~~powershell
git push -u origin feature/sensor-framework-v0.1
~~~

Never use force push. If authentication fails or the remote moved, stop and report the exact error instead of changing history.

- [ ] **Step 5: Report delivery evidence**

Report branch name, HEAD commit, remote ref, verification commands and counts, demo result, README path, design path, and next-version recommendation path.

---

## Plan Self-Review Checklist

- [ ] Every approved design section maps to an implementation task.
- [ ] Every production task has a test-first RED step owned by a distinct test subagent.
- [ ] Every implementation step has a focused GREEN command.
- [ ] All public types and units are consistent across tasks.
- [ ] No production task permits dynamic memory, direct function-to-interface dependencies, or implicit singleton state.
- [ ] MQTT and TMP75 scope values are exact and testable.
- [ ] README claims are written only after clean verification.
- [ ] Remote push is non-force and targets a feature branch.
