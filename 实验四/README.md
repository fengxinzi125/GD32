# 实验四：外部SRAM读写与检测

## 一、存储器基础知识

### 1.1 存储器分类

| 类型 | 特点 | 典型用途 |
|------|------|----------|
| ROM | 只读，掉电不丢失 | 存放固件、常量表 |
| RAM | 可读可写，掉电丢失 | 运行时数据、变量、堆栈 |
| Flash | 可读可写，掉电不丢失 | 程序存储、用户配置区 |

### 1.2 RAM的分类

1. **静态RAM（SRAM）**：读写速度快，无需刷新，价格高，容量小。只要不断电，数据就一直保持。本实验使用的外部SRAM即属于此类。
2. **动态RAM（DRAM）**：需要每隔一段时间刷新充电一次，否则数据消失。价格低，容量大，常用于DDR内存。

### 1.3 同步与异步访问

- **同步**：按顺序执行，发起请求后，调用者必须原地等待，直到任务返回结果后才能继续处理下一个操作。
- **异步**：发起请求后，调用者无需等待，任务完成后，会通过回调、事件或通知来告知结果。

> 本工程中EXMC总线访问是同步的（CPU等待总线完成），但软件架构上GUI模块使用了回调函数（writeCallback/readCallback）实现异步设计模式。

---

## 二、GD32F303 EXMC（外部存储器控制器）

### 2.1 EXMC简介

EXMC（External Memory Controller）是GD32F303的外部存储器控制器，支持NOR Flash、PSRAM、SRAM等外部存储器的并行接口访问。其核心优势是**内存映射**：外部存储器被映射到CPU的地址空间，可以像访问内部RAM一样用指针或数组直接读写。

### 2.2 Bank0 Region地址分配

| Region | 基地址 | 本实验用途 | 数据宽度 |
|--------|--------|-----------|---------|
| Region1 | 0x60000000 | LCD显示屏 | 16bit |
| Region3 | 0x6C000000 | 外部SRAM（1MB） | 16bit |

### 2.3 EXMC时序参数说明

| 参数 | 含义 | LCD值 | SRAM值 |
|------|------|-------|--------|
| asyn_address_setuptime | 地址建立时间 | 0 | 0 |
| asyn_address_holdtime | 地址保持时间（多路复用时有效） | 0 | 0 |
| asyn_data_setuptime | 数据建立时间（值越大越慢越稳） | 读15/写2 | 3 |
| bus_latency | 总线延迟周期 | 0 | 0 |

### 2.4 关键配置项

- `address_data_mux = DISABLE`：地址和数据总线独立，不复用
- `memory_type = EXMC_MEMORY_TYPE_SRAM`：存储器类型为SRAM
- `databus_width = EXMC_NOR_DATABUS_WIDTH_16B`：16位数据总线
- `extended_mode`：LCD设为ENABLE（读写不同时序），SRAM设为DISABLE
- `memory_write = ENABLE`：使能写操作
- `write_mode = EXMC_ASYN_WRITE`：异步写入模式

---

## 三、工程文件结构

```
实验四/作业/
├── HW/
│   ├── EXMC/EXMC.c          # EXMC初始化（GPIO + Region1 + Region3）
│   └── UART0/UART0.c/.h     # 串口驱动
├── App/
│   ├── SRAM/ReadWriteSRAM.c/.h  # SRAM读写模块 + 检测函数
│   ├── LCD/LCD.c/.h             # LCD显示驱动
│   ├── Main/Main.c              # 主程序入口
│   └── EmbGUI/App/GUITop.c/.h   # GUI顶层界面
└── FW/                          # GD32标准外设库
```

---

## 四、核心函数说明

### 4.1 EXMC相关函数（HW/EXMC/EXMC.c）

| 函数 | 功能 |
|------|------|
| `InitEXMC()` | 总初始化：配置GPIO → 配置Region1(LCD) → 配置Region3(SRAM) |
| `ConfigEXMCGPIO()` | 配置EXMC引脚：地址总线(PF/PG/PD)、数据总线(PD/PE)、控制信号(NE1/NE3/NOE/NWE/NBL0/NBL1)，全部为AF_PP复用推挽输出 |
| `ConfigBank0Region1()` | 配置LCD所在Region1的读写时序和参数 |
| `ConfigBank0Region3()` | 配置外部SRAM所在Region3的时序和参数 |
| `exmc_norsram_init()` | 标准库函数，初始化NOR/SRAM区域 |
| `exmc_norsram_enable()` | 标准库函数，使能指定区域 |

### 4.2 SRAM读写函数（App/SRAM/ReadWriteSRAM.c）

| 函数 | 功能 |
|------|------|
| `InitReadWriteSRAM()` | 初始化SRAM缓冲区地址范围、注册GUI回调、清零缓冲区、打印地址信息 |
| `ReadWriteSRAMTask()` | 周期任务（每40ms调用一次），处理GUI交互 |
| `SRAM_Test()` | SRAM全片检测：写0xFF验证→串口输出→写0x00验证→串口输出→LCD显示结果 |
| `ReadByte(addr)` | 底层读1字节：`return *(u8*)addr;` |
| `WriteByte(addr, data)` | 底层写1字节：`*(u8*)addr = data;` |

### 4.3 LCD显示函数（App/LCD/LCD.h）

| 函数 | 功能 |
|------|------|
| `InitLCD()` | 初始化LCD驱动芯片 |
| `LCDClear(color)` | 清屏，填充指定颜色 |
| `LCDShowString(x,y,w,h,size,p)` | 在(x,y)位置显示字符串，size为字号(12/16) |
| `LCDShowNum(x,y,num,len,size)` | 显示数字 |
| `LCDFill(sx,sy,ex,ey,color)` | 填充矩形区域 |
| `LCDDrawPoint(x,y)` | 画点 |

### 4.4 串口函数（HW/UART0/UART0.h）

| 函数 | 功能 |
|------|------|
| `InitUART0(bound)` | 初始化UART0，参数为波特率（本实验用115200） |
| `WriteUART0(pBuf, len)` | 向串口发送数据，返回已发送字节数 |
| `printf()` | 格式化输出（需在Keil中勾选Use MicroLIB） |

---

## 五、外部SRAM访问方式详解

本工程使用**内存映射方式**访问外部SRAM：

```c
#define EX_SRAM_BUFFER_SIZE (1 * 1024 * 1024)  // 1MB
static u8 s_arrExSRAM[EX_SRAM_BUFFER_SIZE] __attribute__((at(0x6C000000)));
```

- `__attribute__((at(addr)))` 是ARM编译器扩展语法，将变量强制定位到指定物理地址
- 0x6C000000 对应 EXMC Bank0 Region3 的起始地址
- 访问 `s_arrExSRAM[i]` 等同于直接读写外部SRAM的第i个字节
- 无需额外的SPI/I2C等串行协议，读写速度与内部RAM接近（受EXMC时序约束）

---

## 六、SRAM检测流程

```
开始
 │
 ├─ 1. 全片写入 0xFF
 ├─ 2. 逐字节读回验证，统计错误数 errCountFF
 ├─ 3. 串口输出 0xFF 测试结果
 │
 ├─ 4. 全片写入 0x00
 ├─ 5. 逐字节读回验证，统计错误数 errCount00
 ├─ 6. 串口输出 0x00 测试结果
 │
 └─ 7. LCD显示最终检测结果（PASS/FAIL + 错误数）
```

---

## 七、注意事项与常见坑

1. **编译器优化等级必须设为 -O0**：否则编译器可能把"写后立即读"优化掉，导致验证结果不正确。设置路径：Options for Target → C/C++ → Optimization → Level 0(-O0)。
2. **循环变量必须用 u32**：`u16` 最大值为65535，无法遍历1MB（1048576字节），会导致死循环。
3. **SRAM测试必须在LCD初始化之后调用**：`InitLCD()` 在 `InitSoftware()` 中执行，`SRAM_Test()` 应放在其后，否则操作未初始化的LCD硬件会导致程序跑飞。
4. **EXMC GPIO必须全部配置为 AF_PP（复用推挽输出）**，速度设为 MAX。
5. **printf需要MicroLIB支持**：在 Options for Target → Code Generation 中勾选 Use MicroLIB。
6. **不要将SRAM全片测试放在周期任务中**：1MB遍历耗时约200-300ms，会阻塞主循环，应在初始化阶段只执行一次。
