可以看出，该设计中 `I2C_SDA` (GPIO8) 和 `I2C_SCL` (GPIO7) 被多个外设（PMIC、Touch、IMU、RTC、Audio）共用。

### 硬件引脚分配表

| 硬件模块 (Module) | 芯片/型号 (IC Model) | 信号名称 (Signal) | GPIO 引脚 (Pin) | 备注/关联 (Notes) |
| --- | --- | --- | --- | --- |
| **Display (屏幕)** | CO5300 | QSPI_SIO0 | GPIO1 | 接收来自 PMIC 的 `DSI_PWR_EN` 供电使能信号 |
|  |  | QSPI_SIO1 | GPIO2 |  |
|  |  | QSPI_SIO2 | GPIO3 |  |
|  |  | QSPI_SIO3 | GPIO4 |  |
|  |  | QSPI_SCL | GPIO0 |  |
|  |  | LCD_CS | GPIO5 |  |
|  |  | LCD_RESET | GPIO11 |  |
| **Touch (触摸)** | FT3168 | RESET | GPIO10 |  |
|  |  | Interrupt | GPIO15 |  |
|  |  | I2C_SDA | GPIO8 | 共享 I2C 总线 |
|  |  | I2C_SCL | GPIO7 | 共享 I2C 总线 |
| **PMIC (电源管理)** | AXP2101 | I2C_SDA | GPIO8 | 共享 I2C 总线；输出 `DSI_PWR_EN` 控制屏幕 |
|  |  | I2C_SCL | GPIO7 | 共享 I2C 总线 |
| **6-AXIS IMU (惯导)** | QMI8658 | Interrupt1 | GPIO16 |  |
|  |  | Interrupt2 | GPIO17 |  |
|  |  | I2C_SDA | GPIO8 | 共享 I2C 总线 |
|  |  | I2C_SCL | GPIO7 | 共享 I2C 总线 |
| **RTC (实时时钟)** | PCF85063 | I2C_SDA | GPIO8 | 共享 I2C 总线 |
|  |  | I2C_SCL | GPIO7 | 共享 I2C 总线 |
| **Audio (音频)** | Codec: ES8311 <br>

<br> ADC: ES7210 (含AEC) | I2C_SDA | GPIO8 | 共享 I2C 总线 |
|  |  | I2C_SCL | GPIO7 | 共享 I2C 总线 |
|  |  | I2S_MCLK | GPIO19 |  |
|  |  | I2S_SCLK | GPIO20 |  |
|  |  | I2S_ASDOUT | GPIO21 |  |
|  |  | I2S_LRCK | GPIO22 |  |
|  |  | I2S_DSDIN | GPIO23 |  |
|  |  | PA_CTRL | GPIO6 |  |
| **Buttons (按键)** | 手表侧边按键 | BOOT | GPIO9 |  |
|  |  | PWR | GPIO18 |  |
| **Storage (存储)** | 16MB Flash | - | - | 图中未标注具体引脚分配 |