# 实验八 · 从 SD 卡扇区到 FatFs 文件，再到 NAND Flash

本文跟读 `08.SDCardFatFs（And NAND Flash）` 的实际 Keil 工程。下文源码路径以该项目为根目录，代码片段省略了部分日志或分支。建议沿着 `Holmes.txt` 的一个字符读完应用、FatFs、磁盘适配和 SDIO 四层，再沿同一个 `diskio` 接口走 NAND 分支。实验七软件 SPI 学过的扇区、缓冲区和指针，仍然是这两条链的基础。

**首次上电前先看启动行为。** 原程序会先尝试挂载板载 NAND，挂载失败会尝试 `f_mkfs("1:", 0, 1024)`；NAND 初始化时若 `CreateLUT()` 失败，还会进入擦除好块的 `FTLFormat()`。即使 NAND 挂载成功，每次启动也会用 `FA_CREATE_ALWAYS` 重建 `1:text.txt`。请在允许清空 NAND 的实验板和已备份的实验卡上操作。本文的硬件结果是观察目标，尚未编译、下载或上板实测。

## 0. 先认识 SDIO 和 NAND Flash

### SDIO：GD32 和 SD 卡怎样通信

**作用**　这里的 SDIO 是 GD32 内部负责与 SD 卡通信的硬件外设。实验七软件 SPI 由 CPU 翻转 GPIO 时钟并逐位收发。本实验由 CPU 配置 SDIO 外设，外设通过 SD 总线收发命令和数据，DMA 负责在 SDIO 的数据 FIFO 与 SRAM 之间搬运。

**源码**　先看 `App/SDCard/SDCard.c` 的 `gpio_config()` 和 `sd_io_init()`。前者把 PC12 配为 `CK` 时钟、PD2 配为 `CMD` 命令和响应线、PC8～PC11 配为 `D0`～`D3` 数据线。PD3 是卡座检测输入。后者在选卡后配置 `SDIO_BUSMODE_4BIT` 和 `SD_DMA_MODE`。

**逐步拆解**

1. CPU 告诉 SDIO 要发哪条命令、读哪个地址。读单块用 `CMD17`，卡在 `CMD` 线上返回响应。
2. 卡通过 `D0`～`D3` 送回数据。4 位总线表示一个数据时钟周期传 4 位，SDIO 收下数据后暂存在 FIFO。
3. DMA 从 FIFO 搬数据到程序提供的 SRAM 缓冲区。驱动和中断处理负责查看完成与错误状态。

**具体例子**　读一个 512 字节扇区时，卡通过 4 条数据线传送，DMA 在 MCU 内部每次搬 32 位，总共搬 128 次。**总线的 4 位**描述卡和 MCU 之间的数据宽度，**DMA 的 32 位**描述 MCU 内部每次搬多少数据。

**注意事项**　SDIO 和 DMA 让硬件接手收发和搬运，当前驱动仍会等待传输完成。第 6 节继续追 `disk_read()` 怎样一路调用到这些硬件。

### NAND Flash：板上芯片怎样存数据

**作用**　NAND Flash 是断电后仍能保存数据的存储芯片。这里访问的是板载原始 NAND，GD32 经 EXMC 外部存储控制器发送命令和地址、读写数据。工程中的 FTL 把块、页、列地址转换包装成 FatFs 可以调用的 512 字节扇区接口。

**源码**　先看 `HW/NandFlash/NandFlash.h` 的 `NAND_PAGE_SIZE`、`NAND_SPARE_AREA_SIZE`、`NAND_BLOCK_SIZE`，再看 `NandFlash.c` 的 `InitNandFlash()` 和 `FTL.c` 的 `FTLReadSectors()`。

**逐步拆解**

1. **页**是读写时要定位的单位。本工程一页有 2048 字节 Main 区，能放 4 个逻辑扇区，另有 64 字节 Spare 区，可存放 ECC 校验值和块管理信息。
2. **块**是擦除单位。本工程每块有 64 页。擦除后数据回到 `0xFF`，编程可以把位从 `1` 改成 `0`。要恢复为 `1`，需要擦除整个块。
3. **FTL** 用查找表把逻辑块号映射到物理块号。当前页已使用时，工程可把旧块的数据和新数据合并到备用块，再更新映射。FatFs 继续使用原来的逻辑扇区号。

**具体例子**　NAND 的逻辑扇区 0～3 对应逻辑块 0 的第 0 页，页内列偏移依次为 0、512、1024、1536。读取扇区 2 时，FTL 先查逻辑块 0 对应的物理块，再从该块的第 0 页、列 1024 开始读 512 字节。

**注意事项**　**SDIO 是通信外设，NAND Flash 是存储介质。** 本工程的两条路径是 `0:` → SDIO → SD 卡，以及 `1:` → FTL → EXMC → 板载 NAND。第 7 节继续讲块、页、列如何计算，以及重写为何可能触发换块。

## 1. 工程地图和地址账本

从 `Project/GD32KeilPrj.uvprojx` 打开工程。目标芯片为 **GD32F303ZE**，工程列入 `App/SDCard/SDCard.c`、`ff.c`、`diskio.c`、`FTL.c` 和 `NandFlash.c`。SD 卡主线使用 **SDIO 4 位总线 + DMA**，可接上实验七做过的 SDIO 对比。随工程提供的 FatFs 是 **R0.10b**，函数签名和配置宏以本地 `ff.h`、`ffconf.h` 为准。

| 位置 | 先读什么 |
| --- | --- |
| `ARM/System/startup_gd32f30x_hd.s`、`App/Main/Main.c`、`HW/Timer/Timer.c` | 复位入口、初始化和定时标志 |
| `App/FatFsTest/FatFsTest.c`、`ReadBookByte.c` | NAND 自测、SD 目录遍历、分页、文件位置和进度 |
| `App/EmbGUI/App/GUITop.c`、`App/EmbGUI/Module/GUIButton.c`、`App/KeyOne/` | 屏幕翻页和实体键回调 |
| `TPSW/FatFs/src/ff.c`、`ff.h`、`ffconf.h` | 卷、目录、文件对象、扇区缓存 |
| `TPSW/FatFs/src/diskio.c`、`diskio.h` | 逻辑盘到介质的分发，参数单位和返回码转换 |
| `App/SDCard/SDCard.c` | SDIO 命令、DMA、完成与错误 |
| `HW/NandFlash/FTL.c`、`NandFlash.c`、`NandFlash.h` | NAND 逻辑块映射、换块和 EXMC 访问 |
| `HW/EXMC/EXMC.c`、`FW/Source/`、`App/LCD/` | 总线、外设库、绘屏。顺着当前调用按需查阅 |

| 量 | 本项目中的例子 | 它由哪层解释 |
| --- | --- | --- |
| 路径和逻辑卷 | `0:/book/Holmes.txt`、`1:text.txt` | FatFs。`0:` 对应 SD，`1:` 对应 NAND，`2:` 有定义但 `diskio.c` 没有该设备的实现 |
| 文件字节偏移 | `f_lseek(&s_fileBook, 5130)` 中的 `5130` | FatFs 按该文件的簇链定位。簇是文件系统分配空间的单位 |
| 磁盘逻辑扇区号 LBA | `disk_read(0, buf, 2048, 1)` 中的 `2048` | 磁盘接口以 512 字节为单位，LBA 由文件系统算出 |
| SD 驱动参数 | `2048 << 9`，即 `1048576` 字节 | SDIO 驱动接收字节地址，高容量卡分支再除以 512 得到命令参数 |
| NAND 位置 | 物理块号、块内页号、页内列地址 | FTL 先拆逻辑扇区，再查逻辑块到物理块的映射 |
| MCU 内存地址 | `buf`、`&newchar`、`&s_iByteRemain` | 指向 SRAM 中的数据或结果，供函数或 DMA 使用 |

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
3. `InitFatFsTest()` 初始化历史位置，立即挂载 NAND，写入并读回测试文件，随后尝试挂载 SD、遍历 `0:/book`、注册翻页回调、初始化 GUI，并调用 `NextPage()` 绘制首页。
4. 运行中，`GUITask()` 用 `ScanGUIButton()` 取屏幕按钮事件，然后通过 `s_pGUIDev` 调用前页或后页回调。实体键走 `ScanKeyOne(keyName, OnKeyOneUp, OnKeyOneDown)`，在按下事件调用 `ProcKeyDownKey1/2/3`。当前三个弹起回调为空。

**具体例子**　点屏幕“下一页”，调用链是 `ScanTouch` 更新输入 → `Proc2msTask` → `FatFsTask` → `GUITask` → `nextPageCallback()` → `NextPage` → `NewPage`。`InitGUI(&s_structGUIDev)` 保存了结构体地址，因此这个静态对象在初始化返回之后还有效。给 `ScanKeyOne` 传 `ProcKeyDownKey2` 是交出函数地址，按下时才执行其中的 `SaveReadProgress()`。

**注意事项**　触摸扫描名义约 20ms，`FatFsTask()` 名义约 40ms，以 `Proc2msTask()` 中的 `20 × 2ms` 为准。多个中断可以合并成一次布尔标志，读盘、绘页、打印和等待都会影响实际频率。NAND 挂载循环和自测的 `while(1)` 位于初始化阶段，卡在这里时还没有进入主循环。

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

| 入口 | 实际调用与文件 |
| --- | --- |
| KEY1 按下 | 检查 `0:/book/progress`，遇到 `FR_NO_PATH` 时尝试建目录，再以 `FA_CREATE_NEW \| FA_READ` 创建 `progress.txt` |
| KEY2 按下 | 用 `FA_OPEN_EXISTING \| FA_WRITE` 打开 `0:/book/progress/progress.txt`，写入形如 `阅读进度:5131/5200` 的文本并关闭 |
| KEY3 按下 | `f_unlink("0:/book/progress/progress.txt")` |
| 上电 NAND 自测 | 用 `FA_CREATE_ALWAYS \| FA_WRITE` 创建或截断 `1:text.txt`，写 512 字节，关闭后以 `FA_OPEN_EXISTING \| FA_READ` 重开，读回并逐字节比较 |

**逐步拆解**　`FA_CREATE_NEW` 遇到已有文件会失败。`FA_OPEN_EXISTING | FA_WRITE` 从文件开头写，保留原文件长度中未覆盖的尾部。`FA_CREATE_ALWAYS` 会把旧长度截断，再从头写。NAND 自测用同一个 `writeNum` 变量先接收写入量、后接收读出量，观察时按当前调用解释。`GetBytePosition()` 返回缓冲层下一次要取的字节偏移，保存的数值可能已经跨过被暂存的 `s_iLastChar`。

**具体例子**　`(u8)i` 把 `0..511` 写成两轮 `00 01 ... FE FF`，可观察下标 0、255、256、511，预期依次为 `0x00`、`0xFF`、`0x00`、`0xFF`。进度实验先按 KEY1，再翻页按 KEY2，观察 `f_write` 返回码和实际写入量，最后按 KEY3 并确认文件消失。

**注意事项**　工程启动会调用 `NextPage()`，没有打开进度文件并恢复阅读位置的流程。源码日志存在误报的路径，实验以返回码、长度、实际文件内容为准。NAND 格式化的 `1024` 是此版 `f_mkfs` 的分配单元字节数，也就是这里的 2 扇区每簇；第二参数 `0` 选 FDISK 形式。

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
2. `CreateLUT()` 扫描物理块，读前两页的坏块标志，并从第一页 Spare 的偏移 2～5 取逻辑块号，把 `lut[逻辑块号]` 填成物理块号。表在 SRAM，持久的块编号在 NAND Spare。可识别映射少于 100 项时返回失败，`InitFTL()` 接着尝试 `FTLFormat()`，擦除识别出的好块并把约 90% 的好块分配逻辑号。其余好块给换块留空间。`FTLFormat()` 处理介质映射，`f_mkfs()` 在逻辑扇区上建立 FAT 结构。
3. `FTLReadSectors()` 计算块、页、列，用 LUT 查到物理块后执行 `NandReadPage()`，失败再尝试一次。每成功读一扇区，`sectorNo += 1` 和 `pBuffer += 512`，分别推进介质位置与 SRAM 写入位置。
4. `FTLWriteSectors()` 用 `NandCheckPage()` 检查当前页的 Main 区是否仍是 `0xFF`。空页走直接写。已使用页尝试找同奇偶性的空闲物理块，调用 `NandCopyBlockWithWrite()`，边复制旧块边合入新数据。成功后更新 LUT，再擦除旧块以供后续使用，擦除失败则标坏。空页直写失败也有尝试换块、标坏并重试的分支。
5. `NandReadPage()` 通过 EXMC 发读命令和列/行地址，等待 R/B 状态，再从数据区逐字节读到 `buf`。形如 `*(__IO u8 *)(BANK_NAND_ADDR | EXMC_ADDR_AREA)` 的宏表示一次内存映射 I/O 访问。给它赋值会送出 NAND 地址周期。Main 区数据每 512 字节计算 ECC，与 Spare 中保存的 ECC 比较，必要时尝试校正。

**具体例子**　读 LBA `255`、`256` 两扇区。`255` 对应逻辑块 0、页 63、列 1536，`256` 对应逻辑块 1、页 0、列 0。若调试器显示 `lut[0]=2`、`lut[1]=5`，则实际从物理块 2 和 5 读，两个扇区分别落在 `buf[0..511]` 与 `buf[512..1023]`。

**注意事项**　和实验七的“改一个字节，读改写一扇区”相似，这里也需要保留同一存储单元内的其他数据，但处理范围可能扩展到整个 NAND 擦除块。把“检查空页 → 选备用块 → 复制合并 → 改 LUT → 擦旧块”读通即可开始调试。ECC 位级算法、块复制的全部时序和异常断电恢复可放到第二遍。

## 8. 第一遍必须会用和必须读懂的函数

| 学习要求 | 函数 | 学到什么程度 |
| --- | --- | --- |
| 必须会用 | `f_mount`、`f_open`、`f_read`、`f_write`、`f_lseek`、`f_close` | 能写出正确顺序，选好路径和打开模式，区分偏移、请求量、实际量、返回码和对象生命周期 |
| 必须会用 | `f_opendir`、`f_readdir`、`f_closedir`、`f_mkdir`、`f_unlink` | 能列目录、区分正常结束与失败，知道创建和删除对应哪个卷 |
| 必须会用 | `f_mkfs`、`f_sync` | 能解释格式化目标卷和第三参数的单位，知道何时同步、何时关闭。纸面掌握后再操作可清空的介质 |
| 必须读懂实现 | `InitFatFsTest`、`Proc2msTask`、`GUITask`、`ScanKeyOne` | 能从上电、触屏和实体键分别找到业务入口，并指出等待会阻塞哪一层 |
| 必须读懂实现 | `ReadData`、`ReadBookByte`、`GetBytePosition`、`SetPosition`、`NextPage`、`PreviousPage`、`NewPage` | 能手算批号、索引、剩余量，追踪页边界的待显示字符 |
| 必须读懂实现 | `disk_initialize`、`disk_read`、`disk_write`、`disk_ioctl` | 能按 `pdrv` 找到分支，说明输入单位、返回类型以及控制命令输出指针的类型 |
| 必须读懂实现 | `sd_io_init`、`sd_block_read` 的 DMA 分支、`dma_receive_config`、`sd_interrupts_process` | 能说明 SDIO、DMA、CPU 和中断各做什么，找到完成和报错位置 |
| 必须读懂实现 | `InitFTL`、`CreateLUT`、`FTLReadSectors`、`FTLWriteSectors` | 能区分 NAND 格式化与 FAT 格式化，手算块页列并解释何时换块 |
| 顺链读关键段 | `find_volume`、`f_read`、`NandReadPage` | 找到卷探测、整扇区直读、页读和 ECC 的分界；第二遍再细读 FAT 表操作和 ECC 位运算 |

## 9. 源码问题与验证办法

| 源码能够确认的事实 | 需要上板验证的现象或可用的观察点 |
| --- | --- |
| `FatFsTest.c:295–315` 的 NAND 分支在任何挂载错误后尝试 `f_mkfs`，并将格式化成功与失败的日志写反。`f_mkfs` 会再调用 `disk_initialize`。 | 分别记录每次 `f_mount` 和 `f_mkfs` 的 `FRESULT`、`InitFTL` 返回值以及是否进了 `FTLFormat`。从返回值判断是在反复失败，还是已经格式化并在下一轮挂载成功。 |
| `FatFsTest.c:387–427` 在 SD 挂载失败后仍执行 `f_opendir`。即使打开目录失败，也会把 `direct` 交给 `f_readdir` 和 `f_closedir`。 | 在正常卡与缺少 `book` 目录的卡上分别记录返回码和后续控制流。目录错误后界面能否继续、报何种错误由实际状态决定。 |
| NAND 自测检查读写的 `FRESULT` 并比较数组，未比较实际读写量，且两次 `f_close` 的结果未检查。进度保存检查写入量但同样未检查关闭，删除则直接打印成功。再次用较短文本保存还会保留旧尾部。 | 单步确认实际读写量和关闭返回码。连续保存长度不同的进度后在电脑查看原始内容，删除不存在的文件时对照 `f_unlink` 返回值与串口日志。 |
| `ReadBookByte.c:139–183` 的补读成功分支未重新检查实际读取量。首次读空文件时可能从清零的缓冲区取出一个 `0`，再把无符号 `s_iByteRemain` 从 0 减到最大值。`ReadData()` 在打开成功、定位或读取失败时直接返回，未走关闭。 | 空文件路径先做纸面推演。要确认时在减法后设断点并立即停止，避免继续读取超出缓冲区。用断点确认错误路径是否经过 `f_close`。 |
| `ReadBookByte.c:103–106,220–241` 中，`SetPosition()` 重新补读前未清取字节层的结束标志，`ReadData()` 只在读到文件尾时置位。`NextPage()` 后续保存位置时无条件减 1。 | 用超过 5 KiB 的文本读到末页后回退到较早的一批，再前进跨批。观察旧的结束标志是否阻止补读。另用固定行号文本反复前后翻，对照页首是否出现重复或漏字，尤其区分是否存在 `s_iLastChar`。 |
| `diskio.c` 对两种盘的 `disk_status()` 直接报就绪，现有挂载和读写链未调用 `sd_detect()`。`sd_io_init()` 的状态查询会覆盖前一步的 `status`。 | 用串口或调试器逐步记录 SD 初始化、选卡、查状态和挂载结果。缺卡或初始化失败时，检查第一个失败点，避免用最后一个 `status` 反推前面的调用。 |
| `diskio.c` 的 SD 地址转换 `sector << 9` 走 32 位；LBA `8388608` 对应 4 GiB 字节偏移，转换后丢掉高位。SD 卡信息的 `card_capacity` 也是 32 位，`GET_SECTOR_COUNT` 用它再除以 512。 | 纸面确认溢出即可。在板上仅用只读断点核对 LBA、传入的 `readaddr` 和容量字段。低 LBA 的文件读通无法覆盖这一边界。 |
| `disk_ioctl(GET_BLOCK_SIZE)` 当前写 `WORD`，而 `f_mkfs()` 传的是 `DWORD n` 的地址，语义是每擦除块的扇区数。SD 分支给出的是 `card_blocksize`；NAND 分支的 `GET_SECTOR_COUNT` 按 1024 个物理块计算，和 FTL 实际建立的有效逻辑块范围有差异。`FTLLogicNumToPhysicalNum()` 还使用 `>` 检查上界，等于 `blockTotalNum` 会进入查表。 | 在可清空的板上记录格式化使用的 `n`、`n_vol` 和 `validBlockNum`。对 `logicalBlock == 1024` 用纸面或断点分析即可，避免实际读写超出有效逻辑卷的地址。 |
| `NandFlash.c` 的 `NandCopyBlockWithWrite()` 把 `copyBuf` 初始化为 `buf`，循环中更新剩余长度、页号和列号，但没有推进 `copyBuf`。一次换块写跨多页时，后续页仍从输入缓冲区开头取数据。 | 纸面设起始列为 1536，写 1024 字节，前 512 字节填 `0x11`，后 512 字节填 `0x22`。若走换块路径，应比较两次页写收到的指针与读回数据。启动时的 512 字节自测无法单独覆盖这个场景。 |
| `diskio.c` 的 SD 收发状态等待没有超时。驱动里单块读的 DMA 分支查 `FTF`，未像多块读一样在后续等待中检查 `transerror`。 | 出现超时、读失败或主循环不再响应时，记录停在哪个循环、`FTF`、`transend`、`transerror` 和 SDIO 状态位。硬件故障类型和是否能复现要靠观察。 |

## 10. 分阶段动手

这些练习可以先手算，再用断点和观察窗口完成。串口设 115200。测试卡使用已备份的 FAT 卷，根目录准备 `book/Holmes.txt`。建议让正文超过 10 KiB，每行带不同的 ASCII 编号并使用 CRLF，这样翻页或回退时容易核对位置。使用前确认板载 NAND 的数据可清空。

| 阶段 | 动手步骤 | 预期结果和验证点 |
| --- | --- | --- |
| ① 启动路径 | 依次在 `main`、`InitFatFsTest`、`disk_initialize` 和 `Proc2msTask` 下断点。记录 `pdrv`、NAND ID 和挂载返回码。 | NAND `pdrv=1` 先出现，随后自测写、关、开、读、比较。NAND 路径成功后才尝试 SD `pdrv=0`，之后才进入主循环。若停在 NAND，按实际返回码定位。 |
| ② 从目录读到首字符 | 在 `f_opendir`、`ReadData`、`f_read` 和 `ReadBookByte` 下断点。核对 `fname` 和第一个字符。 | 串口枚举 `book` 目录。正文第一批定位到偏移 0。若文件超过 5120 且读取成功，实际量为 5120，`s_iFilePos=1`，读完第一个字节后 `s_iByteCnt=1`、剩余 5119。 |
| ③ 翻页和跨批 | 屏幕点下一页，记录每页首行、`GetBytePosition()`、`s_iLastChar`，走到第二批后点上一页。用观察窗口跟到 `SetPosition()` 的除法和取余。 | 顺序阅读消耗完第一批后，下一批从文件偏移 5120 开始。回退会用历史数组里的字节偏移重读。对照正文行号记录实际页首，若出现重复、漏字或提前停止，回查第 9 节的边界。 |
| ④ 创建、保存、删除 | 按 KEY1、翻页、按 KEY2，在调试器确认关闭结果后停机并在电脑检查文件。再上电，用 KEY3 删除并检查。 | 成功时 SD 卡出现 `book/progress/progress.txt`，内容包含当前字节位置和总长度。重新启动应从首屏绘制。KEY3 成功后文件消失，以目录内容和 `f_unlink` 返回码判断。 |
| ⑤ 追到 SDIO DMA | 正常读书时在 `disk_read` 和 `dma_receive_config` 观察 `pdrv`、`sector`、`count`、`buff` 和 `number`。在停机快照里比较 DMA 与 SDIO 的状态。 | SD 读盘的 `pdrv=0`；驱动入口地址等于 LBA 乘 512（在 32 位表示范围内），单扇区搬运数是 128 个 32 位字。是否走多块由本次请求和簇边界决定，5 KiB 请求可能被分段。断点停机会影响外设时序，遇到异常再低干预复核。 |
| ⑥ 追到 NAND 物理页 | 启动自测时观察 `FTLReadSectors`、`FTLWriteSectors`。记录实际的 `sectorNo`、页、列和 LUT 查询结果，手算 255/256 作为对照。 | 磁盘请求以 512 字节计，一页对应 4 个 LBA。读回测试数组应是两轮 `00…FF`。若遇到已编程页，进一步观察 `emptyBlock`、LUT 更新和旧块擦除。避免手动调用擦除函数来强行制造场景。 |

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
    参考答案　格式化的打印条件写反，删除的打印无条件。应检查 `f_mkfs`、`f_unlink` 的真实返回码，并核对重挂载或目录内容。
14. **为什么 `FatFsTask()` 实际可能比 40ms 更久才执行一次？**  
    参考答案　每处理 20 个 2ms 标志才调用一次。标志会合并，绘页、读盘和串口等同步调用占用主循环，不能按中断次数积累待处理任务。
15. **FTL 格式化和 `f_mkfs` 各建立什么？**  
    参考答案　FTL 格式化擦除识别出的好块并建立可重建的逻辑块映射。`f_mkfs` 在适配层提供的逻辑扇区上创建 FAT 卷结构。启动时两者都有各自的触发条件。
