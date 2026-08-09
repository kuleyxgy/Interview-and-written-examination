# STM32F4xx reference port boundary

This directory documents the adapter boundary only. It has not been compiled or
validated against a particular STM32CubeF4 release, board, clock tree, or RTOS.

A real adapter should keep one caller-owned context per peripheral instance and
bind these interface operations:

- `hal_time_ops_t.now_ms`: obtain a monotonic millisecond value from
  `HAL_GetTick`, or from a target timer when tick suspension is possible.
- `hal_i2c_ops_t.transfer`: map the explicit bus handle and address to
  `HAL_I2C_Master_Transmit`, `HAL_I2C_Master_Receive`, or the target's sequential
  transfer APIs. Convert the framework timeout to the SDK timeout and preserve
  NACK, timeout, and short-transfer results.
- `hal_net_ops_t.connect/send/recv/close`: bind the chosen Ethernet, Wi-Fi, or
  modem stack. For LwIP sockets this normally maps to `connect`, `send`, `recv`,
  and `close`; TLS is supplied by the port when required.
- `hal_uart_ops_t.send/recv`: bind a specific `UART_HandleTypeDef` through
  `HAL_UART_Transmit` and `HAL_UART_Receive` (or interrupt/DMA equivalents).
- `hal_spi_ops_t.transfer`: bind a specific `SPI_HandleTypeDef` through
  `HAL_SPI_TransmitReceive`.
- `hal_gpio_ops_t.configure/write/read`: translate the logical pin in the port
  context to `HAL_GPIO_Init`, `HAL_GPIO_WritePin`, and `HAL_GPIO_ReadPin`.

Do not store an active peripheral in a process-wide singleton. Translate
`HAL_StatusTypeDef` and stack errors to `sns_status_t`, and bound every blocking
operation by the timeout supplied by the interface contract.
