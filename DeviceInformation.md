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

## 产品简介
ESP32-C6-Touch-AMOLED-2.06 是一款由微雪电子 (Waveshare) 推出的高性能、可穿戴的手表形态开发板。该产品基于 ESP32-C6 微控制器，集成了 2.06 英寸电容触控高清 AMOLED 屏、六轴传感器、RTC、音频编解码芯片及电源管理等多种功能模块，配合定制表壳，外观形态与智能手表一致，专为可穿戴应用的原型开发与功能验证而设计。

## 产品特性
搭载 ESP32-C6 高性能 32 位 RISC-V 处理器，主频高达 160MHz
支持 Wi-Fi 6、蓝牙 5 和 IEEE 802.15.4 (Zigbee 3.0 和 Thread) 无线通信，具有出色的射频性能，板载天线
内置 512KB HP SRAM、16KB LP SRAM 和 320KB ROM，外挂 16MB Flash
采用 Type-C 接口，提高了用户的使用便捷性和设备的兼容性

## 硬件说明
板载 2.06 英寸电容触摸高清 AMOLED 屏，410 × 502 分辨率，16.7 M 彩色，能清晰地显示彩色图片
内置 CO5300 驱动芯片和 FT3168 电容触控芯片，分别使用 QSPI 和 I2C 接口通信，不占用过多接口引脚资源
板载 QMI8658 六轴惯性测量单元 （3 轴加速度、3 轴陀螺仪），可检测运动姿态、计步等功能
板载 PCF85063 RTC 芯片，通过 AXP2101 接入电池，实现不间断供电
板载 PWR、BOOT 两个可自定义功能的侧边按钮，方便使用按钮进行自定义功能开发
板载 3.7V MX1.25 锂电池充放电接口
引出 1 路 I2C、1 路 UART 和 1 路 USB 焊盘，可供外接设备和调试使用
使用 AXP2101 的好处包括高效的电源管理、支持多种输出电压、充电和电池管理功能以及对电池寿命的优化
使用 AMOLED 屏幕，具有更高的对比度、广视角、丰富的色彩、快速响应时间、纤薄设计、低功耗和灵活性等优点