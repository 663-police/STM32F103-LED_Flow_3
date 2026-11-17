# 电控组第三次招新培训作业 —— 三灯流水灯 + OLED + 串口控制

- 姓名：陈少兵
- 学号：20253001907
- 开发板：STM32F103C8T6（Blue Pill）
- 开发环境：CLion + STM32CubeMX + ARM GNU Toolchain + OpenOCD + ST-Link V2

## 一、功能说明

1. 三路 LED 流水灯，默认 300ms 切换一次，方向可正转/反转。
2. 两个按键：
   - PA11：切换流水方向（FWD / REV）
   - PA12：暂停 / 继续
3. 0.96 寸 SSD1306 OLED（软件 I²C）实时显示：
   - `STATE: RUN / PAUSE`
   - `DIR: FWD / REV`
   - `STEP: xxxx ms`
   - `LED: 1 / 2 / 3`
4. USART1 串口控制与状态回传：
   - 波特率 115200，8 数据位，1 停止位，无校验
   - 接收 6 字节控制帧，校验通过后执行
   - 状态变化后主动回传一行文本

## 二、硬件连接

### LED

| LED | STM32 引脚 | 接法 |
| --- | --- | --- |
| LED0 | PA0 | 引脚 → 330Ω → LED 正极，LED 负极 → GND |
| LED1 | PA2 | 同上 |
| LED2 | PA4 | 同上 |

### 按键

| 按键 | STM32 引脚 | 接法 |
| --- | --- | --- |
| 方向键 | PA11 | PA11 ↔ 按键 ↔ GND，内部上拉，下降沿触发 |
| 暂停键 | PA12 | PA12 ↔ 按键 ↔ GND，内部上拉，下降沿触发 |

### OLED（SSD1306，4 针 I²C）

| OLED | STM32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SCL | PB8 |
| SDA | PB9 |

### USB-TTL

| USB-TTL | STM32 |
| --- | --- |
| TXD | PA10 |
| RXD | PA9 |
| GND | GND |
| VCC | 不接 |

> 注意：USB-TTL 与 STM32 必须共地，否则串口收发都不通。

## 三、串口协议

### 控制帧格式（PC → STM32，6 字节）

| 字节 | 含义 |
| --- | --- |
| 0 | 帧头，固定 `0xAA` |
| 1 | 运行标志：`0x01` 运行，`0x00` 暂停 |
| 2 | 方向：`0x00` 正转，`0x01` 反转 |
| 3 | 流水间隔低字节 |
| 4 | 流水间隔高字节 |
| 5 | 校验和 = 前 5 字节之和的低 8 位 |

流水间隔范围：100 ~ 2000 ms。

### 回传格式（STM32 → PC，ASCII 文本）

```text
STATE=RUN,DIR=FWD,STEP=300,LED=1\r\n
```

### 常用测试帧

```text
AA 01 00 2C 01 D8    运行、正转、300ms
AA 00 00 2C 01 D7    暂停、正转、300ms
AA 01 01 2C 01 D9    运行、反转、300ms
AA 01 00 F4 01 A0    运行、正转、500ms
AA 01 00 E8 03 96    运行、正转、1000ms
```

## 四、程序结构

```text
Core/Src/main.c          HAL 初始化，调用 App_Init / App_Loop
Core/Src/app.c           业务逻辑：按键、串口协议、流水灯状态机、OLED 刷新
Core/Src/oled.c          SSD1306 软件 I²C 驱动 + 5×7 字库
Core/Src/stm32f1xx_it.c  中断入口：EXTI15_10、USART1
Core/Inc/app.h           App 接口
Core/Inc/oled.h          OLED 接口
```

主要实现点：

- 流水灯使用 `HAL_GetTick()` 非阻塞计时，不使用 `HAL_Delay()`。
- 按键使用 EXTI 下降沿中断 + 200ms 软件消抖。
- 串口使用 RX 中断逐字节接收，收满 6 字节后置位由主循环解析。
- 串口加入 100ms 断帧超时，丢字节后能自动重新对齐帧头。
- 先回传状态再刷新 OLED，保证串口响应及时。
- 流水灯每步只刷新 OLED 的 `LED:` 行，避免整屏重写。

## 五、参考资料与驱动来源

- STM32F103x8/xB 参考手册、数据手册（ST 官方）
- STM32CubeF1 HAL 库 `STM32Cube_FW_F1_V1.8.7`
- SSD1306 数据手册（Solomon Systech）
- 软件 I²C 与 SSD1306 初始化命令序列按数据手册自行实现
- 5×7 ASCII 字库为项目内自建精简字库
- CH340 驱动：南京沁恒官方 `CH341SER.EXE`（仅用于电脑端 USB-TTL）

## 六、编译与烧录

```bash
mingw32-make -j8
openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg \
  -c "program build/LED_Flow_3.elf verify reset exit"
```

## 七、演示视频

仓库内文件：

- [video/demo_1.mp4](video/demo_1.mp4)
- [video/demo_2.mp4](video/demo_2.mp4)

## 八、思考题

`[按作业要求补充]`
