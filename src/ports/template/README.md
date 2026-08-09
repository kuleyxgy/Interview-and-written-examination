# Interface port template

This directory is a deliberately non-functional starting point for a new MCU
port. It is excluded from normal builds. Enable the dedicated
`SENSOR_BUILD_PORT_TEMPLATE` target only to compile-check the interface
contracts.

Copy this directory, rename the `template_hal_*_bind` entry points for the
target, and replace each `SNS_ERR_UNSUPPORTED` operation with an SDK call. Keep
the caller-owned `ctx` in every bound HAL object; do not introduce an active
global bus, clock, or connection.

- `hal_time.c`: bind the monotonic millisecond counter.
- `hal_i2c.c`: bind one explicit bus instance and preserve transfer counts.
- `hal_net.c`: bind one transport instance; TLS, if needed, belongs in this
  transport port rather than in `proto_mqtt`.

Port operations must translate SDK results to `sns_status_t`, honor the supplied
timeout, and report short transfers through the output count.
