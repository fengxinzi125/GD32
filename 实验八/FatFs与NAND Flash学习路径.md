# 实验八 · 从 SD 卡扇区到 FatFs 文件，再到 NAND Flash

本文跟读 `08.SDCardFatFs（And NAND Flash）` 的实际 Keil 工程。下文源码路径以该项目为根目录，代码片段省略了部分日志或分支。建议沿着 `Holmes.txt` 的一个字符读完应用、FatFs、磁盘适配和 SDIO 四层，再沿同一个 `diskio` 接口走 NAND 分支。实验七软件 SPI 学过的扇区、缓冲区和指针，仍然是这两条链的基础。

**首次上电前先看启动行为。** 当前固件先尝试挂载板载 NAND。挂载成功时，自测用 `FA_CREATE_ALWAYS` 创建或覆盖 `1:text.txt`。挂载失败时，串口打印返回码，跳过 NAND 自测并继续初始化 SD 阅读器。清空和重建由调试器的一次性标志显式触发。第 12 节记录了这块板的实际故障和重启验证，其余动手练习中的结果是观察目标。

## 0. 先认识 SDIO 和 NAND Flash

### SDIO：GD32 和 SD 卡怎样通信

**作用**　这里的 SDIO 是 GD32 内部负责与 SD 卡通信的硬件外设。实验七软件 SPI 由 CPU 翻转 GPIO 时钟并逐位收发。本实验由 CPU 配置 SDIO 外设，外设通过 SD 总线收发命令和数据，DMA 负责在 SDIO 的数据 FIFO 与 SRAM 之间搬运。

**源码**　先看 `App/SDCard/SDCard.c` 的 `gpio_config()` 和 `sd_io_init()`。前者把 PC12 配为 `CK` 时钟、PD2 配为 `CMD` 命令和响应线、PC8～PC11 配为 `D0`～`D3` 数据线。PD3 是卡座检测输入。后者在选卡后配置 `SDIO_BUSMODE_4BIT` 和 `SD_DMA_MODE`。

#### SDIO 的内部结构与外部接线

```text
GD32 芯片内部                                             SD 存储卡内部
┌─────────────────────────┐                             ┌─────────────────┐
│ CPU → SDIO 命令单元      │ ── PC12 CK ────────────────→ │ SD 卡控制器      │
│       写命令、读响应      │ ←─ PD2  CMD ───────────────→ │ 解释命令和逻辑地址 │
│                         │                             │        ↕        │
│ SRAM ↔ DMA ↔ FIFO ↔ 数据单元│ ←─ PC8～PC11 DAT[3:0] ──→ │ 卡内存储阵列     │
└─────────────────────────┘                             └─────────────────┘
  PD3 ← 卡座检测输入 CD（按本工程软件定义，独立于 DAT3）
```

GD32 的 SDIO 可以按三部分来读。**时钟控制**生成输出给卡的 `CK`，**命令单元**负责把命令串行发出并接收响应，**数据单元**负责处理数据块、CRC 和 FIFO。FIFO 是外设和 MCU 内存之间的临时队列。CPU 配置寄存器，DMA 搬 FIFO 的正文数据，SD 卡控制器则在卡内部完成存储访问。这里的 SDIO 指 MCU 外设，实验连接的是 SD 存储卡。SD 标准中的 SDIO 功能卡另有自己的命令，例如访问功能寄存器的 CMD52、CMD53。 [GD32F30x 用户手册 SDIO 章节](https://www.gd32mcu.com/data/documents/userManual/GD32F30x_User_Manual_Rev2.9.pdf) · [SD Association 存储卡物理层规范 §3.7](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf) · [SDIO 规范](https://www.sdcard.org/downloads/pls/pdf/?e=EN_SSE1&f=PartE1_SDIO_Simplified_Specification_Ver3.00.pdf&p=PartE1_SDIO_Simplified_Specification_Ver3.00.jpg)

| 卡侧信号     | 本工程 GD32 引脚 | 方向与用途                                         | 实验七软件 SPI 中同一组线的角色 |
| ------------ | ---------------- | -------------------------------------------------- | ------------------------------- |
| `CLK` / `CK` | PC12             | GD32 → 卡，给命令和数据传输提供节拍                | `SCLK`，由软件翻转 GPIO         |
| `CMD`        | PD2              | 双向，先由主机发命令，需要响应时再由卡回传         | `MOSI`，主机发给卡              |
| `DAT0`       | PC8              | 双向，1 位和 4 位模式都使用，读写时方向随阶段变化  | `MISO`，卡发给主机              |
| `DAT1`       | PC9              | 双向，4 位传输的第 1 位                            | 当前软件 SPI 驱动未使用         |
| `DAT2`       | PC10             | 双向，4 位传输的第 2 位                            | 当前软件 SPI 驱动未使用         |
| `DAT3`       | PC11             | 双向，4 位传输的第 3 位                            | 作为低有效的 `CS`               |
| 卡座 `CD`    | PD3              | 卡座 → GD32，源码的 `sd_detect()` 将低电平判为插卡 | 同样是检测输入                  |
| `VDD`、`VSS` | 卡座供电、公共地 | 常规 SD 卡第一排触点的电源与地                     | 供电和地仍需要连接              |

常规全尺寸 SD 卡有 9 个触点，microSD 有 8 个，主要差别之一是地触点数量。卡座可能还有独立的检测开关引脚。规范里全尺寸卡触点名 `CD/DAT3` 与本工程单独读取的 PD3 `CD` 要分别追线。上表给出源码和 MCU 复用功能能确认的连接关系。实际卡座封装、电源、上拉和检测开关接法要对照板卡原理图。 [GD32F303xx 数据手册引脚表](https://www.gd32mcu.com/data/documents/datasheet/GD32F303xx%20Datasheet%20Rev3.1.pdf) · [SD Association 卡触点与供电说明](https://www.sdcard.org/developers/sd-standard-overview/)

**逐步拆解**

1. **先用窄总线识别卡。** `sd_power_on()` 配置初始化分频和 1 位数据模式，依次通过 `CMD0`、`CMD8` 和 `CMD55` + `ACMD41` 等完成复位、接口条件探测和上电协商。之后 `sd_card_init()` 用 `CMD2` 取 CID、`CMD3` 取相对卡地址 RCA、`CMD9` 取 CSD。RCA 用于标识和选择卡，文件偏移或扇区号用于定位存储数据。
2. **主机和卡一起切换宽度。** `sd_io_init()` 通过 `CMD7` 选卡。`sd_bus_width_config()` 用 `CMD55` + `ACMD6`、参数 `0x2` 通知卡采用 4 位模式，成功后驱动才在 MCU 端配置 4 位。`ACMD` 表示跟在 `CMD55` 后面的应用命令。`CK` 与 `CMD` 的角色保持原样，变化的是数据阶段使用的 `DAT` 线数。
3. **<u>读的时候先问，再收数据</u>。** `sd_block_read()` 配置数据路径，并通过 `CMD` 发送 `CMD17` 和地址。卡在同一根 `CMD` 线上返回响应，再经 `DAT0`～`DAT3` 发数据。数据块另有起止位和 CRC，硬件处理后把正文放入 FIFO。
4. **写的时候方向反过来。** 单块写用 `CMD24`。DMA 从 SRAM 向 FIFO 搬数据，SDIO 再通过 `DAT0`～`DAT3` 送给卡。驱动还要处理卡的状态。读、写的 DMA 方向不同，时钟仍由 GD32 发出。
5. **分开看三类状态。** `CMD` 响应说明命令阶段发生了什么，SDIO 数据结束或错误标志反映数据阶段，DMA `FTF` 表示配置的搬运量完成。出现超时时，先确认是哪一段在等。

**具体例子**　用逻辑分析仪理解 `CMD17`，可以先把它看成两段。`CMD` 上是请求和响应，`DAT[3:0]` 上才是读回的扇区。若某个正文数据字节是 `0xA5`，在 4 位模式中先送高四位 `1010`，再送低四位 `0101`。按 `DAT3, DAT2, DAT1, DAT0` 排列，就是两个节拍上的 `1010` 和 `0101`。因此仅算 512 字节正文，`512 × 8 ÷ 4 = 1024` 个数据时钟周期。在 MCU 内部，DMA 每次搬 32 位，需要 `512 ÷ 4 = 128` 次。这两个数字分别描述板上信号和芯片内部搬运。 [SD Association 存储卡物理层规范，数据包格式](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf)

```text
时间 →        命令阶段                         读数据阶段
CK            由 GD32 提供传输时钟 ───────────────────────────→
CMD           GD32 发 CMD17 → 卡回 R1            本次正文走 DAT
DAT[3:0]      等待数据                           起始 | 正文 | CRC | 结束
```

命令的 48 位可以拆成 `起始 1 位 + 方向 1 位 + 命令编号 6 位 + 参数 32 位 + CRC7 + 结束 1 位`。可对照 `sdio_command_response_config(SD_CMD_READ_SINGLE_BLOCK, readaddr, SDIO_RESPONSETYPE_SHORT)`。代码把命令编号、地址参数和期望的响应类型交给外设，外设负责线上的帧格式。这和实验七的 `SendCmd()` 手工拼命令字节可以对照着看。 [SD Association 存储卡物理层规范，命令帧格式](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf)

**注意事项**　SDIO 和 DMA 让硬件接手收发和搬运，当前驱动仍会等待传输完成。第 6 节继续追 `disk_read()` 怎样一路调用到这些硬件。

### NAND Flash：板上芯片怎样存数据

**作用**　NAND Flash 是断电后仍能保存数据的存储芯片。这里访问的是板载原始 NAND，GD32 经 EXMC 外部存储控制器发送命令和地址、读写数据。工程中的 FTL 把块、页、列地址转换包装成 FatFs 可以调用的 512 字节扇区接口。

**源码**　先看 `HW/NandFlash/NandFlash.h` 的 `NAND_PAGE_SIZE`、`NAND_SPARE_AREA_SIZE`、`NAND_BLOCK_SIZE`，再看 `NandFlash.c` 的 `InitNandFlash()` 和 `FTL.c` 的 `FTLReadSectors()`。

#### NAND 芯片的内部结构与 EXMC 接线

```text
GD32                                              板载原始 NAND 芯片
CPU → EXMC Bank1 ─── I/O[7:0] 双向 ──────────────↔ I/O 缓冲与锁存
          │         CE#、CLE、ALE、WE#、RE# ───→ 命令/地址寄存器和控制电路
          │                                        │             │
          └───────← R/B#（PD6）                    页缓冲区  ↔  存储阵列
                                                               ├─ 擦除块 0
                                                               │  ├─ 页 0 [Main | Spare]
                                                               │  └─ ... 页 63
                                                               └─ ... 物理块 1023
```

在存储阵列里，多个 NAND 存储单元串接成串，字线控制对应位置的单元。芯片通过行地址定位页，通过列地址定位页缓冲区内的字节。页缓冲区承接阵列与外部 I/O 之间的数据。读页先把阵列数据送进页缓冲区，再由外部总线取走。编程时先把要写的内容装入页缓冲区，再由芯片内部执行编程。软件只需要给出命令、地址和数据，并等待内部操作结束。 [Micron 存储基础，NAND 单元串结构](https://www.micron.com/content/dam/micron/educatorhub/intro-to-memory/micron-intro-to-memory-presentation.pdf) · [Micron NOR/NAND Flash 指南](https://assets.micron.com/adobe/assets/urn%3Aaaid%3Aaem%3A9b8c-03cb3d488f05/original/as/nor-nand-flash-guide.pdf)

| NAND 线        | GD32 引脚与复用                  | 方向与作用                                                                  |
| -------------- | -------------------------------- | --------------------------------------------------------------------------- |
| `I/O0`～`I/O3` | D0=PD14，D1=PD15，D2=PD0，D3=PD1 | 双向总线低 4 位                                                             |
| `I/O4`～`I/O7` | D4=PE7，D5=PE8，D6=PE9，D7=PE10  | 双向总线高 4 位。8 根线一起传一个字节，分阶段复用来传命令、地址、数据和状态 |
| `CLE`          | PD11 / `EXMC_A16`                | GD32 → NAND，高电平标记总线上当前是命令字节                                 |
| `ALE`          | PD12 / `EXMC_A17`                | GD32 → NAND，高电平标记总线上当前是地址字节                                 |
| `WE#`          | PD5 / `EXMC_NWE`                 | GD32 → NAND，写选通信号，配合 `CLE`、`ALE` 锁存命令、地址或写入数据         |
| `RE#`          | PD4 / `EXMC_NOE`                 | GD32 → NAND，读选通信号，NAND 在 I/O 总线上输出数据或状态                   |
| `CE#`          | PD7 / `EXMC_NCE1`                | GD32 → NAND，低有效片选，当前芯片被选中才参与访问                           |
| `R/B#`         | PD6，GPIO 上拉输入               | NAND → GD32，低电平表示忙，高电平表示就绪。源码的 `NANDWaitRB()` 直接读 PD6 |
| `WP#`          | 当前初始化函数未配置对应 GPIO    | 常见 NAND 写保护输入，低有效。实际是否接到固定电平或板上其他电路要查原理图  |
| `VCC`、`VSS`   | 板载电源与公共地                 | 供电条件和封装管脚号以实物型号及板卡原理图为准                              |

`#` 表示低有效。这套 NAND 接口靠 `WE#`、`RE#` 等选通控制每一拍，和 SD 卡的 `CK` 走法不同。源码 `InitNandFlash()` 里关于 ALE/CLE 的注释顺序容易看反。按本工程的地址宏和 GD32 手册，应以 **A16 = CLE、A17 = ALE** 为准。源码会打印读出的 NAND ID，具体芯片料号和实际 PCB 连线仍要查板卡资料或实物。 [GD32F303xx 数据手册引脚表](https://www.gd32mcu.com/data/documents/datasheet/GD32F303xx%20Datasheet%20Rev3.1.pdf) · [GD32F30x 用户手册 EXMC 章节](https://www.gd32mcu.com/data/documents/userManual/GD32F30x_User_Manual_Rev2.9.pdf)

#### 从 CPU 的三个地址看 NAND 的控制线

| 源码访问窗口     | CPU 访问地址 | EXMC 和 NAND 的动作                                                                     |
| ---------------- | ------------ | --------------------------------------------------------------------------------------- |
| `NAND_CMD_AREA`  | `0x70010000` | `CLE` 有效，把写出的 8 位值送成命令周期                                                 |
| `NAND_ADDR_AREA` | `0x70020000` | `ALE` 有效，把写出的 8 位值送成地址周期                                                 |
| `NAND_DATA_AREA` | `0x70000000` | 数据窗口。写时用 `WE#`，读时用 `RE#`，同一个 CPU 地址可连续读出 NAND 页缓冲中的多个字节 |

以 `NAND_CMD_AREA = 0x00` 为例，宏展开后相当于向 `volatile` 的 8 位地址 `0x70010000` 写一次。EXMC 根据地址位选择 `CLE`，再把值 `0x00` 放到 I/O 线上。紧接着写 `NAND_ADDR_AREA`，同样是写一个 `u8`，EXMC 改用 `ALE`，芯片便把它当作列地址或行地址。**CPU 地址负责选访问窗口，写进去的字节才是 NAND 收到的命令或地址内容。** 这正好能和你学过的指针解引用对上。 [GD32F30x 用户手册，NAND 地址映射与访问时序](https://www.gd32mcu.com/data/documents/userManual/GD32F30x_User_Manual_Rev2.9.pdf)

**逐步拆解**

1. **页**是读写时要定位的单位。本工程一页有 2048 字节 Main 区，能放 4 个逻辑扇区，另有 64 字节 Spare 区，可存放 ECC 校验值和块管理信息。
2. **块**是擦除单位。本工程每块有 64 页。擦除后数据回到 `0xFF`，编程可以把位从 `1` 改成 `0`。要恢复为 `1`，需要擦除整个块。
3. **FTL** 用查找表把逻辑块号映射到物理块号。当前页已使用时，工程可把旧块的数据和新数据合并到备用块，再更新映射。FatFs 继续使用原来的逻辑扇区号。

| 操作                  | 本工程驱动里的总线顺序                                                                                                                                                                                      |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 读页 `NandReadPage`   | `CLE: 00` → `ALE: 列低、列高、行低、行高` → `CLE: 30` → 等 `R/B#` 先忙后就绪 → 用 `RE#` 从数据窗口取字节。随后通过 `NandReadSpare()` 发起一次独立页读，取出 Spare 中的 ECC。旧版在此使用 `05` / `E0` 随机读 |
| 编程 `NandWritePage`  | `CLE: 80` → `ALE: 列地址和行地址` → 通过数据窗口写 Main 数据 → 源码用 `85` 调整列位置写入 ECC → `CLE: 10` 确认 → 等待并读状态                                                                               |
| 擦除 `NandEraseBlock` | 驱动先读坏块标记。允许擦除时，`CLE: 60` → `ALE: 块首页的行地址` → `CLE: D0` → 等待并检查状态。擦除针对整块，省去列地址                                                                                      |

**具体例子**　读取逻辑扇区 2 时，FTL 查逻辑块 0。假设 `lut[0] = 5`，就会调用 `NandReadPage(5, 0, 1024, buf, 512)`。列地址 `1024 = 0x0400`，行地址 `5 × 64 + 0 = 0x0140`。在 I/O 总线上依次看到命令 `00`、地址 `00 04 40 01`、命令 `30`。第一个 `00` 与紧跟着的 `00` 数值一样，分别用 `CLE` 与 `ALE` 区分。等芯片就绪后，程序连续读数据窗口，`buf[0]` 对应页内列 1024，`buf[511]` 对应列 1535。该扇区的 ECC 由源码从列 `2048 + 16 + 4 × 2 = 2072` 处开始取。

**注意事项**　***<u><mark>SDIO 是通信外设，NAND Flash 是存储介质</mark></u>*。** 本工程的两条路径是 `0:` → SDIO → SD 卡，以及 `1:` → FTL → EXMC → 板载 NAND。第 7 节继续讲块、页、列如何计算，以及重写为何可能触发换块。

| 对着开发板看什么         | SD 卡链                                         | 板载 NAND 链                                                |
| ------------------------ | ----------------------------------------------- | ----------------------------------------------------------- |
| 命令、地址、数据怎样上板 | `CMD` 串行传命令及地址参数，`DAT[3:0]` 传正文   | `I/O[7:0]` 分阶段传命令、地址和正文，`CLE` / `ALE` 标记阶段 |
| 谁提供时序               | GD32 的 SDIO 输出 `CK`                          | GD32 的 EXMC 输出 `WE#`、`RE#` 等选通                       |
| 插卡、忙或完成怎样区分   | PD3 代表卡座检测，传输完成看 SDIO 和 DMA 的状态 | PD6 的 `R/B#` 反映芯片忙闲，编程和擦除还要查状态字          |
| 上层传来的扇区号交给谁   | SD 卡控制器处理卡内存储映射                     | 本工程 FTL 查 LUT，再送 NAND 物理块、页和列                 |

上板想看线时，先确认原理图上的测试点和公共地。SD 侧可以先抓 `CK` 和 `CMD`，识别 `CMD17` 与 R1，再加入四条 `DAT` 线看正文。NAND 侧可以先抓 `CLE`、`ALE`、`WE#`、`RE#` 和 `R/B#` 看操作阶段，需要解码 `00`、`30` 或地址字节时再加入 8 条 I/O 线。用正常的阅读和 NAND 自测路径作观察入口，表里的顺序是按源码推导的预期波形。

## 1. 工程地图和地址账本

从 `Project/GD32KeilPrj.uvprojx` 打开工程。目标芯片为 **GD32F303ZE**，工程列入 `App/SDCard/SDCard.c`、`ff.c`、`diskio.c`、`FTL.c` 和 `NandFlash.c`。SD 卡主线使用 **SDIO 4 位总线 + DMA**，可接上实验七做过的 SDIO 对比。随工程提供的 FatFs 是 **R0.10b**，函数签名和配置宏以本地 `ff.h`、`ffconf.h` 为准。

| 位置                                                                      | 先读什么                                     |
| ------------------------------------------------------------------------- | -------------------------------------------- |
| `ARM/System/startup_gd32f30x_hd.s`、`App/Main/Main.c`、`HW/Timer/Timer.c` | 复位入口、初始化和定时标志                   |
| `App/FatFsTest/FatFsTest.c`、`ReadBookByte.c`                             | NAND 自测、SD 目录遍历、分页、文件位置和进度 |
| `App/EmbGUI/App/GUITop.c`、`App/EmbGUI/Module/GUIButton.c`、`App/KeyOne/` | 屏幕翻页和实体键回调                         |
| `TPSW/FatFs/src/ff.c`、`ff.h`、`ffconf.h`                                 | 卷、目录、文件对象、扇区缓存                 |
| `TPSW/FatFs/src/diskio.c`、`diskio.h`                                     | 逻辑盘到介质的分发，参数单位和返回码转换     |
| `App/SDCard/SDCard.c`                                                     | SDIO 命令、DMA、完成与错误                   |
| `HW/NandFlash/FTL.c`、`NandFlash.c`、`NandFlash.h`                        | NAND 逻辑块映射、换块和 EXMC 访问            |
| `HW/EXMC/EXMC.c`、`FW/Source/`、`App/LCD/`                                | 总线、外设库、绘屏。顺着当前调用按需查阅     |

| 量                 | 本项目中的例子                           | 它由哪层解释                                                                   |
| ------------------ | ---------------------------------------- | ------------------------------------------------------------------------------ |
| 路径和逻辑卷       | `0:/book/Holmes.txt`、`1:text.txt`       | FatFs。`0:` 对应 SD，`1:` 对应 NAND，`2:` 有定义但 `diskio.c` 没有该设备的实现 |
| 文件字节偏移       | `f_lseek(&s_fileBook, 5130)` 中的 `5130` | FatFs 按该文件的簇链定位。簇是文件系统分配空间的单位                           |
| 磁盘逻辑扇区号 LBA | `disk_read(0, buf, 2048, 1)` 中的 `2048` | 磁盘接口以 512 字节为单位，LBA 由文件系统算出                                  |
| SD 驱动参数        | `2048 << 9`，即 `1048576` 字节           | SDIO 驱动接收字节地址，高容量卡分支再除以 512 得到命令参数                     |
| NAND 位置          | 物理块号、块内页号、页内列地址           | FTL 先拆逻辑扇区，再查逻辑块到物理块的映射                                     |
| MCU 内存地址       | `buf`、`&newchar`、`&s_iByteRemain`      | 指向 SRAM 中的数据或结果，供函数或 DMA 使用                                    |

配置 `ffconf.h` 开启读写和 `f_mkfs`，扇区固定为 512 字节，`_USE_LFN=0` 使用短文件名，`_FS_TINY=0` 让每个 `FIL` 带自己的扇区缓存。本地库识别 FAT12/16/32。准备测试卡时可用已有 FAT32 卷，并使用源码指定的短路径。文件名支持和正文字符显示由不同代码处理。

## 2. 从复位读到按键回调

**作用**　启动底层外设和界面，然后轮询定时标志，调度触摸、按键和阅读器。

**源码**　`ARM/System/startup_gd32f30x_hd.s` 的 `Reset_Handler` 调用 `SystemInit` 并转入 `__main`。`App/Main/Main.c` 中的调用顺序如下。

```text
main
├─ InitHardware → InitRCU → InitNVIC → InitUART0(115200)
│                 → InitTimer → InitSysTick → InitEXMC
├─ InitSoftware → InitLED → InitLCD → InitJPEG → InitBMP → InitTouch
│                 → InitKeyOne → InitProcKeyOne → InitFatFsTest
└─ while (1)
   ├─ Proc1msTask  → 每处理 20 次 1ms 标志调用 ScanTouch
   ├─ Proc2msTask  → LEDFlicker(250)
   │                 每处理 20 次 2ms 标志调用 FatFsTask → GUITask
   │                 每处理一次 2ms 标志扫描 KEY1、KEY2、KEY3
   └─ Proc1SecTask → 检查并清除 1s 标志
```

**逐步拆解**

1. `InitEXMC()` 配置 LCD 使用的 Bank0 Region1。NAND 由 `InitNandFlash()` 配置 EXMC Bank1，稍后随 NAND 磁盘初始化进入。
2. `TIMER2_IRQHandler()` 设置 1ms 标志，并每两次设置 2ms 标志。`TIMER5_IRQHandler()` 分频产生 1s 标志。主循环通过 `Get*Flag()` 读取，再调用 `Clr*Flag()` 清除。
3. `InitFatFsTest()` 初始化历史位置，调用 `TestNandFlash()` 尝试挂载和自测。它返回后，继续尝试挂载 SD、遍历 `0:/book`、注册翻页回调、初始化 GUI，并调用 `NextPage()` 绘制首页。
4. 运行中，`GUITask()` 用 `ScanGUIButton()` 取屏幕按钮事件，然后通过 `s_pGUIDev` 调用前页或后页回调。实体键走 `ScanKeyOne(keyName, OnKeyOneUp, OnKeyOneDown)`，在按下事件调用 `ProcKeyDownKey1/2/3`。当前三个弹起回调为空。

**具体例子**　点屏幕“下一页”，调用链是 `ScanTouch` 更新输入 → `Proc2msTask` → `FatFsTask` → `GUITask` → `nextPageCallback()` → `NextPage` → `NewPage`。`InitGUI(&s_structGUIDev)` 保存了结构体地址，因此这个静态对象在初始化返回之后还有效。给 `ScanKeyOne` 传 `ProcKeyDownKey2` 是交出函数地址，按下时才执行其中的 `SaveReadProgress()`。

**注意事项**　触摸扫描名义约 20ms，`FatFsTask()` 名义约 40ms，以 `Proc2msTask()` 中的 `20 × 2ms` 为准。多个中断可以合并成一次布尔标志，读盘、绘页、打印和等待都会影响实际频率。旧固件遇到 NAND 自测失败会停在初始化里的 `while(1)`。当前自测失败分支返回，由 `InitFatFsTest()` 继续初始化阅读器。

## 3. 挂载、打开和关闭一个文件

**作用**　把磁盘上的 FAT 卷接到工程，再用目录项找到文件并管理读写状态。

**源码**　读 `App/FatFsTest/FatFsTest.c` 的 `InitFatFsTest()`，以及 `TPSW/FatFs/src/ff.c` 的 `f_mount()`、`find_volume()`、`f_open()`、`f_read()`、`f_close()`。

```c
static FATFS fs[2];
result = f_mount(&fs[0], FS_VOLUME_SD, 1);
/* 省略挂载结果打印 */
result = f_opendir(&direct, "0:/book");
```

**逐步拆解**

1. `FATFS` 是卷工作区。`f_mount(&fs[0], "0:", 1)` 注册它并立即尝试挂载；`opt=0` 则只注册，等后续访问时再挂载。`find_volume()` 调用 `disk_initialize()`，读取引导扇区并检查 FAT 卷，必要时再从分区表找卷。
2. `DIR` 保存一次目录遍历的状态，`FILINFO` 接收一项的名称等信息。打开目录成功后，循环 `f_readdir(&direct, &fileInfo)`，返回 `FR_OK` 且 `fname[0] == '\0'` 表示遍历结束。结束后用 `f_closedir()`。
3. `FIL` 保存一个已打开文件的状态，包括 `fptr`（下一次读写位置）、`fsize` 和数据缓存。`f_open()` 找目录项并初始化对象，`f_read()` 按文件偏移和簇链得到 LBA。整扇区可以直接读到调用者缓冲区，每次连续读至多跨到当前簇边界；零散部分从扇区缓存复制。
4. `f_read(&file, buf, wanted, &got)` 有两条结果通道。`FRESULT` 报告状态，`got` 通过输出指针接收实际字节数。写入也要同时检查 `FRESULT` 和写入量，并检查 `f_close()` 的返回值。当前版本的 `f_close()` 对写入路径会先调用 `f_sync()`。

**具体例子**　文件长 5200 字节，位置在 5120 时请求 5120 字节，正常返回 `FR_OK`，实际只读 80 字节，`fptr` 到 5200。再读一次可以返回 `FR_OK` 且读到 0 字节。`f_lseek()` 接受字节偏移，源码中的 5120 字节分批策略属于应用层。

**注意事项**　`fs[2]` 用 `static` 是因为 FatFs 注册并保留卷对象的地址。`FIL` 需要在打开到关闭之间有效，可以使用局部对象，前提是作用域、栈空间和传给底层的缓冲区满足条件。`FatFsTest.c` 关于局部 `FIL` 会卡死的注释是现场线索，原因需结合栈、地址对齐与调用路径验证。可先用 `sizeof(FIL)`、链接 map 和栈水位检查，启动文件里的 `Stack_Size` 配置为 `0x3200`。

## 4. 跟一个字符穿过 5 KiB 缓冲区

**作用**　每次向 FatFs 批量取一段正文，给绘页代码逐字符提供数据，并维护当前文件偏移。

**源码**　`App/FatFsTest/ReadBookByte.c` 的 `ReadData()`、`ReadBookByte()`、`GetBytePosition()`、`SetPosition()`，以及 `FatFsTest.c` 的 `NewPage()`、`NextPage()`、`PreviousPage()`。

```c
/* ReadData，省略检查和清缓冲区等步骤 */
result = f_open(&s_fileBook, "0:/book/Holmes.txt", FA_OPEN_EXISTING | FA_READ);
result = f_lseek(&s_fileBook, s_iFilePos * BOOK_READ_BUF_SIZE);
result = f_read(&s_fileBook, s_arrReadBuf, BOOK_READ_BUF_SIZE, &s_iByteRemain);
s_iFilePos = s_iFilePos + 1;
```

**逐步拆解**

1. `BOOK_READ_BUF_SIZE = 5120`。缓冲耗尽且文件还有数据时，`ReadBookByte()` 调用 `ReadData()`，由它打开文件、定位、批量读取、更新文件结束状态并关闭文件。
2. 补读后，`s_iFilePos` 表示下一批的编号，`s_iByteCnt` 表示当前批内下一个待取的下标，`s_iByteRemain` 表示本批剩余量。`ReadBookByte(&newchar, &visibleLen)` 经 `*byte` 和 `*visi` 写回字符和其后连续大于空格的字节数，再将下标加一、剩余量减一。这个前瞻在当前缓冲区内停止。
3. 已读入一批后，`GetBytePosition()` 用 `(s_iFilePos - 1) * 5120 + s_iByteCnt` 计算下一字节偏移。`SetPosition()` 反过来用整除定位哪一批，用取余定位批内下标，补读后减去已跳过的字节。
4. `NewPage()` 清显示区域，处理 `\r`、`\n`、可打印 ASCII，并用前瞻长度尝试整词换行。按 `(734 - 62) / 30` 得到最多 22 行。页面放不下已取出的字符时，代码可把它存进 `s_iLastChar`，下一页先画它。
5. `NextPage()` 用 64 项历史数组记录位置，`0xFFFFFFFF` 表示空项。首刷把当前偏移写入尾部两项，之后移动数组并在尾部存 `GetBytePosition() - 1`。`PreviousPage()` 使用历史位置调用 `SetPosition()`，清绘页层的结束标志和待显示字符，再重新绘页。

**具体例子**　文件长 5200 字节，执行 `SetPosition(5130)` 时，`5130 / 5120 = 1`，`5130 % 5120 = 10`。补读从 5120 开始并得到 80 字节，之后 `s_iFilePos=2`、`s_iByteCnt=10`、`s_iByteRemain=70`。成功再取一个字节，偏移变成 5131，剩余 69。这里的 5130 与 SD 卡上的 LBA 之间还有簇链转换。

**注意事项**　源码有两套同名的 `s_iEndFlag`，分别处于两个 `.c` 文件。取字节层在最后一批入缓冲时置位，此时还能继续读缓冲中剩下的数据。绘页层在 `ReadBookByte()` 返回 0 时置位，因此文件错误和正常结束都可能让界面停止翻页。正文实验先用 ASCII、无 BOM、CRLF 文本，以便把字符集和换行问题从文件读取链里分开观察。

## 5. 进度文件与 NAND 自测的两条写入链

**作用**　实体键演示创建、写入和删除 SD 卡上的进度文件。NAND 自测在启动时演示完整的写、关、重开、读、比较。

**源码**　`App/KeyOne/ProcKeyOne.c` 的 `ProcKeyDownKey1/2/3`，对应 `FatFsTest.c` 的 `CreatReadProgressFile()`、`SaveReadProgress()`、`DeleteReadProgress()`。

| 入口           | 实际调用与文件                                                                                                                        |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| KEY1 按下      | 检查 `0:/book/progress`，遇到 `FR_NO_PATH` 时尝试建目录，再以 `FA_CREATE_NEW \| FA_READ` 创建 `progress.txt`                          |
| KEY2 按下      | 用 `FA_OPEN_EXISTING \| FA_WRITE` 打开 `0:/book/progress/progress.txt`，写入形如 `阅读进度:5131/5200` 的文本并关闭                    |
| KEY3 按下      | `f_unlink("0:/book/progress/progress.txt")`                                                                                           |
| 上电 NAND 自测 | 用 `FA_CREATE_ALWAYS \| FA_WRITE` 创建或截断 `1:text.txt`，写 512 字节，关闭后以 `FA_OPEN_EXISTING \| FA_READ` 重开，读回并逐字节比较 |

**逐步拆解**　`FA_CREATE_NEW` 遇到已有文件会失败。`FA_OPEN_EXISTING | FA_WRITE` 从文件开头写，保留原文件长度中未覆盖的尾部。`FA_CREATE_ALWAYS` 会把旧长度截断，再从头写。NAND 自测用同一个 `writeNum` 变量先接收写入量、后接收读出量，观察时按当前调用解释。`GetBytePosition()` 返回缓冲层下一次要取的字节偏移，保存的数值可能已经跨过被暂存的 `s_iLastChar`。

**具体例子**　`(u8)i` 把 `0..511` 写成两轮 `00 01 ... FE FF`，可观察下标 0、255、256、511，预期依次为 `0x00`、`0xFF`、`0x00`、`0xFF`。进度实验先按 KEY1，再翻页按 KEY2，观察 `f_write` 返回码和实际写入量，最后按 KEY3 并确认文件消失。

**注意事项**　工程启动会调用 `NextPage()`，没有打开进度文件并恢复阅读位置的流程。源码日志存在误报的路径，实验以返回码、长度、实际文件内容为准。一次性 NAND 重建使用 `f_mkfs("1:", 1, 1024)`，第二参数 `1` 让 FAT 卷从逻辑扇区 0 开始，`1024` 是分配单元的字节数，即 2 扇区每簇。旧版调用的第二参数是 `0`，对应 FDISK 形式。

## 6. `diskio` 与 SDIO、DMA

**作用**　FatFs 只提出“对某个物理盘读写若干逻辑扇区”的请求，由 `diskio` 选择介质驱动。

**源码**　`TPSW/FatFs/src/diskio.c`、`diskio.h`，以及 `App/SDCard/SDCard.c` 的 `sd_io_init()`、`sd_block_read()`、`sd_multiblocks_read()`、`dma_receive_config()`、`SDIO_IRQHandler()`。

```c
/* disk_read 的 SD 分支，省略检查 */
if (1 == count) {
    status = sd_block_read(buff, sector << 9, SECTOR_SIZE);
} else {
    status = sd_multiblocks_read(buff, sector << 9, SECTOR_SIZE, count);
}
```

**逐步拆解**

1. `f_mount` 立即挂载时，经 `find_volume()` 进入 `disk_initialize(pdrv)`。SD 分支调用 `sd_io_init()`，依次尝试初始化、获取卡信息、选卡，再查状态，随后配置 4 位总线和 DMA 模式，并使能 SDIO 中断。NAND 分支调用 `InitFTL()`。
2. `disk_read()`、`disk_write()` 的 `count` 单位是扇区。SD 分支用 `sector << 9` 换算字节地址，再按 `count` 选择单块或多块接口；NAND 分支将原始 LBA 交给 `FTLReadSectors()` 或 `FTLWriteSectors()`。这里要区分 FatFs 的 `FRESULT`、扇区读写的 `DRESULT`、设备状态的 `DSTATUS` 和驱动自己的 `sd_error_enum`。
3. SDIO 单块读配置数据长度并发 CMD17。DMA1 `DMA_CH3` 从固定的 SDIO FIFO 地址读 32 位字，向递增的 SRAM 地址写，配置项 `number = bufsize / 4`。读 512 字节时搬 128 个字。写入的 DMA 方向相反。
4. 单块读的 DMA 分支轮询 `DMA_FLAG_FTF`，多块读和写入路径还会等待 `transend` 或 `transerror`。SDIO 中断进入 `sd_interrupts_process()`，处理数据结束或错误。需要停止多块传输时，数据结束分支会调用 `sd_transfer_stop()` 发送 CMD12。`diskio` 又等待 `sd_transfer_state_get()` 显示 SDIO 收发已停止。

**具体例子**　假设 FatFs 已经定位到 LBA `2048`。`disk_read(0, buf, 2048, 1)` 传给 `sd_block_read()` 的地址是 `1048576`，高容量卡分支再除以 `512`，CMD17 的地址参数就是 `2048`。若是 `disk_read(1, buf, 2048, 1)`，FTL 收到的仍是 `2048`，自行拆块、页和列。

**注意事项**　实验七软件 SPI 由 CPU 逐位搬运，此处由 DMA 搬运，但当前 API 仍同步等待。调试要分开观察 DMA 完成标志、SDIO 数据结束/错误标志和 `disk_read()` 返回值。驱动把缓冲区交给 32 位宽度的 DMA，排障时应记录实际 `buff` 地址和 `buff % 4`，包括 `FATFS.win`、`FIL.buf` 和应用缓冲区的路径。文件偏移能否设置成非 4 倍数，应按 `f_lseek()` 的行为判断，DMA 对齐看实际传给 DMA 的地址。

## 7. NAND FTL、物理页与 EXMC

**作用**　FTL 向 FatFs 提供 512 字节逻辑扇区，把请求翻译为 NAND 块、页和列地址，并通过换块支持重写。

**源码**　`HW/NandFlash/FTL.c` 的 `InitFTL()`、`CreateLUT()`、`FTLFormat()`、`FTLReadSectors()`、`FTLWriteSectors()`，以及 `NandFlash.h` 和 `NandFlash.c` 的地址宏与页读写。

```c
readBlock  = (sectorNo / s_structFTLDev.sectorPerPage) / s_structFTLDev.blockPageNum;
readPage   = (sectorNo / s_structFTLDev.sectorPerPage) % s_structFTLDev.blockPageNum;
readColumn = (sectorNo % s_structFTLDev.sectorPerPage) * sectorSize;
readBlock  = FTLLogicNumToPhysicalNum(readBlock);
```

**逐步拆解**

1. 芯片几何参数来自 `NandFlash.h`。一页 Main 区 2048 字节，可容纳 4 个逻辑扇区，另有 64 字节 Spare。每个擦除块 64 页，相当于 256 个逻辑扇区的 Main 区。源码配置 1024 个物理块。
2. `CreateLUT()` 扫描物理块，读前两页的坏块标志，并从第一页 Spare 的偏移 2～5 取逻辑块号，把 `lut[逻辑块号]` 填成物理块号。表在 SRAM，持久的块编号在 NAND Spare。可识别映射少于 100 项时返回失败，当前 `InitFTL()` 将错误返回给上层。显式重建时，`FTLFormat()` 才擦除识别出的好块并给约 90% 的好块分配逻辑号，其余好块用于换块。`f_mkfs()` 随后在逻辑扇区上建立 FAT 结构。
3. `FTLReadSectors()` 计算块、页、列，用 LUT 查到物理块后执行 `NandReadPage()`，失败再尝试一次。每成功读一扇区，`sectorNo += 1` 和 `pBuffer += 512`，分别推进介质位置与 SRAM 写入位置。
4. `FTLWriteSectors()` 用 `NandCheckPage()` 检查当前页的 Main 区是否仍是 `0xFF`。空页走直接写。已使用页尝试找同奇偶性的空闲物理块，调用 `NandCopyBlockWithWrite()`，边复制旧块边合入新数据。成功后更新 LUT，再擦除旧块以供后续使用，擦除失败则标坏。空页直写失败也有尝试换块、标坏并重试的分支。
5. `NandReadPage()` 通过 EXMC 发读命令和列/行地址，等待 R/B 状态，再从数据区逐字节读到 `buf`。形如 `*(__IO u8 *)(BANK_NAND_ADDR | EXMC_ADDR_AREA)` 的宏表示一次内存映射 I/O 访问。给它赋值会送出 NAND 地址周期。Main 区数据每 512 字节计算 ECC，与 Spare 中保存的 ECC 比较，必要时尝试校正。

**具体例子**　读 LBA `255`、`256` 两扇区。`255` 对应逻辑块 0、页 63、列 1536，`256` 对应逻辑块 1、页 0、列 0。若调试器显示 `lut[0]=2`、`lut[1]=5`，则实际从物理块 2 和 5 读，两个扇区分别落在 `buf[0..511]` 与 `buf[512..1023]`。

**注意事项**　和实验七的“改一个字节，读改写一扇区”相似，这里也需要保留同一存储单元内的其他数据，但处理范围可能扩展到整个 NAND 擦除块。把“检查空页 → 选备用块 → 复制合并 → 改 LUT → 擦旧块”读通即可开始调试。ECC 位级算法、块复制的全部时序和异常断电恢复可放到第二遍。

## 8. 第一遍必须会用和必须读懂的函数

| 学习要求     | 函数                                                                                                | 学到什么程度                                                                             |
| ------------ | --------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| 必须会用     | `f_mount`、`f_open`、`f_read`、`f_write`、`f_lseek`、`f_close`                                      | 能写出正确顺序，选好路径和打开模式，区分偏移、请求量、实际量、返回码和对象生命周期       |
| 必须会用     | `f_opendir`、`f_readdir`、`f_closedir`、`f_mkdir`、`f_unlink`                                       | 能列目录、区分正常结束与失败，知道创建和删除对应哪个卷                                   |
| 必须会用     | `f_mkfs`、`f_sync`                                                                                  | 能解释格式化目标卷和第三参数的单位，知道何时同步、何时关闭。纸面掌握后再操作可清空的介质 |
| 必须读懂实现 | `InitFatFsTest`、`Proc2msTask`、`GUITask`、`ScanKeyOne`                                             | 能从上电、触屏和实体键分别找到业务入口，并指出等待会阻塞哪一层                           |
| 必须读懂实现 | `ReadData`、`ReadBookByte`、`GetBytePosition`、`SetPosition`、`NextPage`、`PreviousPage`、`NewPage` | 能手算批号、索引、剩余量，追踪页边界的待显示字符                                         |
| 必须读懂实现 | `disk_initialize`、`disk_read`、`disk_write`、`disk_ioctl`                                          | 能按 `pdrv` 找到分支，说明输入单位、返回类型以及控制命令输出指针的类型                   |
| 必须读懂实现 | `sd_io_init`、`sd_block_read` 的 DMA 分支、`dma_receive_config`、`sd_interrupts_process`            | 能说明 SDIO、DMA、CPU 和中断各做什么，找到完成和报错位置                                 |
| 必须读懂实现 | `InitFTL`、`CreateLUT`、`FTLReadSectors`、`FTLWriteSectors`                                         | 能区分 NAND 格式化与 FAT 格式化，手算块页列并解释何时换块                                |
| 顺链读关键段 | `find_volume`、`f_read`、`NandReadPage`                                                             | 找到卷探测、整扇区直读、页读和 ECC 的分界；第二遍再细读 FAT 表操作和 ECC 位运算          |

## 9. 源码问题与验证办法

| 源码能够确认的事实                                                                                                                                                                                                         | 需要上板验证的现象或可用的观察点                                                                                                                                                  |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 旧版在 NAND 挂载失败后自动调用 `f_mkfs()`，且格式化的成功和失败日志写反。当前 NAND 自测失败后会返回，清空重建需要显式触发。                                                                                                | 看实际 `FRESULT` 和串口阶段。正常复位应走挂载和自测路径。经授权的重建应另有 `NAND rebuild 1: started` 和挂载结果。                                                                |
| 旧版 SD 打开目录失败后仍使用 `DIR`。当前先检查挂载与 `f_opendir()`，成功后才枚举并关闭目录。                                                                                                                               | 在正常卡与缺少 `book` 目录的卡上分别记录返回码和后续控制流。                                                                                                                      |
| NAND 自测检查读写的 `FRESULT` 并比较数组，未比较实际读写量，且两次 `f_close` 的结果未检查。进度保存检查写入量但同样未检查关闭，删除则直接打印成功。再次用较短文本保存还会保留旧尾部。                                      | 单步确认实际读写量和关闭返回码。连续保存长度不同的进度后在电脑查看原始内容，删除不存在的文件时对照 `f_unlink` 返回值与串口日志。                                                  |
| `ReadBookByte.c:139–183` 的补读成功分支未重新检查实际读取量。首次读空文件时可能从清零的缓冲区取出一个 `0`，再把无符号 `s_iByteRemain` 从 0 减到最大值。`ReadData()` 在打开成功、定位或读取失败时直接返回，未走关闭。       | 空文件路径先做纸面推演。要确认时在减法后设断点并立即停止，避免继续读取超出缓冲区。用断点确认错误路径是否经过 `f_close`。                                                          |
| `ReadBookByte.c:103–106,220–241` 中，`SetPosition()` 重新补读前未清取字节层的结束标志，`ReadData()` 只在读到文件尾时置位。`NextPage()` 后续保存位置时无条件减 1。                                                          | 用超过 5 KiB 的文本读到末页后回退到较早的一批，再前进跨批。观察旧的结束标志是否阻止补读。另用固定行号文本反复前后翻，对照页首是否出现重复或漏字，尤其区分是否存在 `s_iLastChar`。 |
| `diskio.c` 对两种盘的 `disk_status()` 直接报就绪，现有挂载和读写链未调用 `sd_detect()`。`sd_io_init()` 的状态查询会覆盖前一步的 `status`。                                                                                 | 用串口或调试器逐步记录 SD 初始化、选卡、查状态和挂载结果。缺卡或初始化失败时，检查第一个失败点，避免用最后一个 `status` 反推前面的调用。                                          |
| `diskio.c` 的 SD 地址转换 `sector << 9` 走 32 位；LBA `8388608` 对应 4 GiB 字节偏移，转换后丢掉高位。SD 卡信息的 `card_capacity` 也是 32 位，`GET_SECTOR_COUNT` 用它再除以 512。                                           | 纸面确认溢出即可。在板上仅用只读断点核对 LBA、传入的 `readaddr` 和容量字段。低 LBA 的文件读通无法覆盖这一边界。                                                                   |
| NAND 的 `GET_BLOCK_SIZE` 已改成以 `DWORD` 回传每擦除块的扇区数，`GET_SECTOR_COUNT` 按有效映射块计，LUT 上界改为 `>=`。SD 分支的 `GET_BLOCK_SIZE` 仍写 `WORD` 并使用 `card_blocksize`，与 `f_mkfs()` 所需类型和语义有差异。 | NAND 可对照 `validBlockNum` 与 `FTLGetSectorCount()`。本板重建后 920 个有效块，每块 256 扇区，合计 235520 扇区。                                                                  |
| `NandFlash.c` 的 `NandCopyBlockWithWrite()` 把 `copyBuf` 初始化为 `buf`，循环中更新剩余长度、页号和列号，但没有推进 `copyBuf`。一次换块写跨多页时，后续页仍从输入缓冲区开头取数据。                                        | 纸面设起始列为 1536，写 1024 字节，前 512 字节填 `0x11`，后 512 字节填 `0x22`。若走换块路径，应比较两次页写收到的指针与读回数据。启动时的 512 字节自测无法单独覆盖这个场景。      |
| `diskio.c` 的 SD 收发状态等待没有超时。驱动里单块读的 DMA 分支查 `FTF`，未像多块读一样在后续等待中检查 `transerror`。                                                                                                      | 出现超时、读失败或主循环不再响应时，记录停在哪个循环、`FTF`、`transend`、`transerror` 和 SDIO 状态位。硬件故障类型和是否能复现要靠观察。                                          |

## 10. 分阶段动手

这些练习可以先手算，再用断点和观察窗口完成。串口设 115200。测试卡使用已备份的 FAT 卷，根目录准备 `book/Holmes.txt`。建议让正文超过 10 KiB，每行带不同的 ASCII 编号并使用 CRLF，这样翻页或回退时容易核对位置。使用前确认板载 NAND 的数据可清空。

| 阶段               | 动手步骤                                                                                                                                   | 预期结果和验证点                                                                                                                                                                                             |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ① 启动路径         | 依次在 `main`、`InitFatFsTest`、`disk_initialize` 和 `Proc2msTask` 下断点。记录 `pdrv`、NAND ID 和挂载返回码。                             | NAND `pdrv=1` 先出现，挂载成功则自测，失败则打印错误并跳过自测。随后尝试 SD `pdrv=0`，之后进入主循环。                                                                                                       |
| ② 从目录读到首字符 | 在 `f_opendir`、`ReadData`、`f_read` 和 `ReadBookByte` 下断点。核对 `fname` 和第一个字符。                                                 | 串口枚举 `book` 目录。正文第一批定位到偏移 0。若文件超过 5120 且读取成功，实际量为 5120，`s_iFilePos=1`，读完第一个字节后 `s_iByteCnt=1`、剩余 5119。                                                        |
| ③ 翻页和跨批       | 屏幕点下一页，记录每页首行、`GetBytePosition()`、`s_iLastChar`，走到第二批后点上一页。用观察窗口跟到 `SetPosition()` 的除法和取余。        | 顺序阅读消耗完第一批后，下一批从文件偏移 5120 开始。回退会用历史数组里的字节偏移重读。对照正文行号记录实际页首，若出现重复、漏字或提前停止，回查第 9 节的边界。                                              |
| ④ 创建、保存、删除 | 按 KEY1、翻页、按 KEY2，在调试器确认关闭结果后停机并在电脑检查文件。再上电，用 KEY3 删除并检查。                                           | 成功时 SD 卡出现 `book/progress/progress.txt`，内容包含当前字节位置和总长度。重新启动应从首屏绘制。KEY3 成功后文件消失，以目录内容和 `f_unlink` 返回码判断。                                                 |
| ⑤ 追到 SDIO DMA    | 正常读书时在 `disk_read` 和 `dma_receive_config` 观察 `pdrv`、`sector`、`count`、`buff` 和 `number`。在停机快照里比较 DMA 与 SDIO 的状态。 | SD 读盘的 `pdrv=0`；驱动入口地址等于 LBA 乘 512（在 32 位表示范围内），单扇区搬运数是 128 个 32 位字。是否走多块由本次请求和簇边界决定，5 KiB 请求可能被分段。断点停机会影响外设时序，遇到异常再低干预复核。 |
| ⑥ 追到 NAND 物理页 | 启动自测时观察 `FTLReadSectors`、`FTLWriteSectors`。记录实际的 `sectorNo`、页、列和 LUT 查询结果，手算 255/256 作为对照。                  | 磁盘请求以 512 字节计，一页对应 4 个 LBA。读回测试数组应是两轮 `00…FF`。若遇到已编程页，进一步观察 `emptyBlock`、LUT 更新和旧块擦除。避免手动调用擦除函数来强行制造场景。                                    |

## 11. 自测题与参考答案

1. **实验七的卡字节地址与 `f_lseek(&file, 510)` 中的 510 有什么关系？**  
   参考答案　510 在这里是该文件的字节偏移。FatFs 要依据该文件的起始簇、簇链和卷布局找扇区。实验七的卡字节地址 510 直接指介质上的偏移。
2. **`0:/book/Holmes.txt` 与 `1:text.txt` 分别去哪里？`_VOLUMES=3` 说明有三个可用设备吗？**  
   参考答案　前者在 SD 卡，后者在 NAND。`_VOLUMES=3` 提供三个逻辑卷位置，磁盘适配实际实现了 SD 和 NAND 两个分支。
3. **`f_mount(&fs[0], "0:", 1)` 为什么需要 `fs[0]` 在调用返回后继续有效？**  
   参考答案　FatFs 保存工作区的指针，后面的文件调用还会使用它。这里用静态对象满足生命周期。
4. **请求读 5120 字节，返回 `FR_OK` 和 80，意味着什么？**  
   参考答案　调用成功，实际取得 80 字节，例如读到了文件尾。用实际量限定缓冲区有效范围。
5. **`ReadBookByte(&newchar, &visibleLen)` 怎样让两个局部变量发生变化？**  
   参考答案　调用把两个地址按值传入，函数解引用 `*byte`、`*visi` 写到调用者变量。函数返回值单独报告是否取到字符。
6. **文件长度 5200 时，`SetPosition(5130)` 补读从哪里开始，下一个字节在哪里？**  
   参考答案　批号 1，补读从 5120 开始，批内索引 10，下一个待取字节的文件偏移为 5130，当前剩 70 字节。
7. **为什么保存的进度可能已经跨过还未画在屏幕上的字符？**  
   参考答案　`ReadBookByte()` 先推进偏移，`NewPage()` 才判断能否画下；若放不下，字符留在 `s_iLastChar` 供下一页显示。保存取的是缓冲读取进度。
8. **只按 KEY2 能在空白卡上创建进度文件吗？下次开机会自动恢复吗？**  
   参考答案　KEY2 使用 `FA_OPEN_EXISTING`，文件应先由 KEY1 创建，且 `book` 目录应存在。启动代码直接绘制首页，没有读取进度文件的恢复流程。
9. **从 `disk_read(0, buf, 2048, 1)` 到 SDHC 命令，地址如何变化？**  
   参考答案　适配层传 `2048 × 512 = 1048576` 字节给驱动，驱动为 SDHC 除以 512，最终 CMD17 参数是 2048。
10. **DMA 搬 512 字节为什么配置 128 次？主循环是否继续跑？**  
    参考答案　每次搬 4 字节。驱动调用仍有同步等待，当前调用返回后主循环才能继续轮询任务。
11. **NAND LBA 255 和 256 的块、页、列是什么？SRAM 指针如何变？**  
    参考答案　分别是逻辑块 0、页 63、列 1536，以及逻辑块 1、页 0、列 0。每轮用逻辑块号查 LUT，完成一扇区后缓冲区指针前进 512 字节。
12. **为什么在已写页里改 512 字节，可能涉及整块复制？**  
    参考答案　NAND 按擦除块回收空间。工程找备用块，复制并合入新数据，使原块内其他页的数据得以保留，再切换逻辑映射。
13. **串口打印“格式化NandFlash成功”或“删除成功”，足以确认对应操作成功吗？**  
    参考答案　旧版格式化的打印条件写反，当前进度删除仍无条件打印成功。应检查 `f_mkfs`、`f_unlink` 的真实返回码，并核对重挂载或目录内容。
14. **为什么 `FatFsTask()` 实际可能比 40ms 更久才执行一次？**  
    参考答案　每处理 20 个 2ms 标志才调用一次。标志会合并，绘页、读盘和串口等同步调用占用主循环，不能按中断次数积累待处理任务。
15. **FTL 格式化和 `f_mkfs` 各建立什么？**  
    参考答案　FTL 格式化擦除识别出的好块并建立可重建的逻辑块映射。`f_mkfs` 在适配层提供的逻辑扇区上创建 FAT 卷结构。当前两步都由一次性的 NAND 重建入口触发。

## 12. 实板排障复盘：从创建文件失败追到 ECC 翻位

### 12.1 串口先确定停在哪一层

最初的症状是有一次能显示，随后启动时出现 `Ecc err` 和 `errorpos:170`。串口软件开头还出现过红色的 Windows I/O 中止提示。后面仍收到 `LCD ID`、`Touch ID` 和 NAND 日志，说明这份记录里板子继续执行过。英文正常而中文乱码时，把串口显示从 UTF-8 切到 GBK，最后两条关键日志才清楚地显示为「挂载NandFlash成功」和「创建文件失败」。

查看源码发现，失败发生在 `f_open(..., "1:text.txt", FA_CREATE_ALWAYS | FA_WRITE)`，早于测试文件的 `f_write()`，也早于 SD 挂载。当时的启动代码在失败分支执行 `while(1)`。Keil 反汇编显示指令反复跳回同一个地址，正好解释了为什么按 F5 似乎没有反应。

| 实际观察                                                                         | 当时可以确认什么                                                                      |
| -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| NAND 识别为 `0x1D80F1AD`，扫描得到 1024 个物理块、1023 个好块、920 个有效逻辑块  | 芯片可读出 ID，FTL 已建立映射。块 `461` 被记录为坏块，本次跟到的 ECC 异常发生在其他块 |
| `f_open()` 返回 `0x02 FR_INT_ERR`，而 `follow_path()` 返回 `FR_OK`               | 路径查询找到了 `text.txt` 的目录项，错误出现在后续覆盖旧文件的过程                    |
| `remove_chain()` 收到起始簇号 `0x04000003`                                       | 起始簇越过卷的合法范围，FatFs 拒绝继续释放簇链                                        |
| 物理块 2、页 23、列 1536 的目录扇区中，`buf[21]` 在纠错前后从 `0x00` 变成 `0x04` | 直接观察到了纠错对 SRAM 中目录项的修改                                                |

### 12.2 用一个字节把三层证据连起来

旧驱动读取这个扇区时，硬件 ECC 为 `0x0033CC0`，从 Spare 取得的是 `0x5556A559`。`NandECCCorrection()` 把这组差异判为单比特错误，打印 `errorpos:170` 并翻位。按源码计算，`170 / 8 = 21`，`170 % 8 = 2`，也就是 `buf[21] ^= 0x04`。

当时 `text.txt` 正好是该扇区第一个目录项。FAT32 的起始簇号高 16 位位于目录项偏移 20～21，低 16 位位于偏移 26～27。偏移 21 的 `0x00 → 0x04` 让高 16 位从 `0x0000` 变成 `0x0400`，与低 16 位 `0x0003` 组合后得到 `0x04000003`。`FA_CREATE_ALWAYS` 在首次创建与覆盖已有文件时走不同分支，只有后者会在这个场景释放旧簇链，因此故障在重启后暴露。

这段过程适合练习三处断点：

1. 在 `ff.c` 的 `res = follow_path(&dj, path);` 上按 **F10**，停在下一行再看 `res`，区分查目录与覆盖流程。箭头停在调用行时，Watch 里显示的还是前一步的结果。
2. 在 `res = remove_chain(dj.fs, cl);` 处看 `cl` 和 `dj.fs->n_fatent`。若停在 `LEAVE_FF`，调用栈只显示当前帧，`cl` 也可能已离开作用域。这次用本次构建的反汇编核对了返回位置和参数寄存器。
3. 在 `NandReadPage()` 的 `NandECCCorrection(...)` 调用处停住，用 **View → Watch Windows → Watch 1** 观察 `buf[21]`，记下前值，再用 **F10** 越过调用查看后值。比较前先核对这是目录读取。物理块号可能随 FTL 换块变化，复测时优先记录 `FTLReadSectors()` 的 `sectorNo` 和目录内容，物理块、页、列用于定位当次读操作。

### 12.3 修复、保留现场和验收

**先拆开 SD 与 NAND 的启动。** 把 NAND 自测移到 `TestNandFlash()`，失败就返回并保留实际错误码，让 SD 阅读器可以单独工作。同时关闭两处自动清空入口。先确认板载 NAND 的旧数据可以丢弃，再进行重建。旧卷如果有要保留的文件，应先只读备份 Main 和 Spare。

**再修 ECC 的读写边界。** 读页、写页和换块写入现在每处理一个独立的 512 字节数据段，先清零并重启硬件 ECC。读回校验值时，通过 `NandReadSpare()` 单独读取。新日志曾出现 `0x00A696A6` 对 `0x55A696A6`，低 24 位完全一样，说明仅第 4 字节不同。当前比较先看有效的低 24 位。低 24 位也不同且第 4 字节异常时，驱动会报错并保留缓冲区内容，避免重演误翻位。重建后遇到 Main 的 512 字节和对应的 Spare 校验字段均为 `0xFF` 时，驱动按擦除态处理。 [GD32F30x 用户手册，EXMC ECC 的计算与位宽](https://www.gd32mcu.com/data/documents/userManual/GD32F30x_User_Manual_Rev2.9.pdf)

**证据的边界。** 本次实测确认了 ECC 纠错造成簇号越界，也核实旧源码缺少每 512 字节独立重置 ECC。排障中还调整了 Spare 读取、校验值比较和启动行为，并最终重建了旧卷。因此最初的 ECC 不匹配究竟由哪一个底层因素单独造成，没有在旧卷上隔离验证。

**最后单独重建 `1:` 并复位验收。** 通过 Keil 一次性置位 `s_nandRebuildRequested`，程序消费标志并立即清零，然后先用 `FTLFormat()` 擦除识别出的好块、重建 LUT，再用 `f_mkfs("1:", 1, 1024)` 创建 FAT 卷。板上串口先显示 `NAND rebuild 1: mounted`，随后显示创建、写入、打开、读取和「校验成功」。再次复位后，直接挂载并再次通过 NAND 自测，没有重复重建。SD 卡也成功挂载，目录仍列出了 `HOLMES.TXT`。
