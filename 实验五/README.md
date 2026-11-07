# 函数理解

## static void ReadProc(u32 addr, u32 len)

```c
static void ReadProc(u32 addr, u32 len)；从 NAND 的指定逻辑字节地址开始，读取并显示 
len 个字节，同时把结果输出到屏幕和串口。
//首先理解各个参数都是做什么
addr:起始字节地址
len：希望显示多少个字节
例如 ReadProc(0x205, 3); 是读取并显示地址0x205、`0x206`、`0x207的数据
```

```c
  if((addr >= s_structGUIDev.beginAddr) && ((addr + len - 1) <= s_structGUIDev.endAddr))
```

接下来是检查地址合不合理，至于为什么是addr + len - 1，  因为**（0x205 + 3 - 1 = 0x207）**  

### 核心代码：读取地址所在的整个扇区

```c
FTLReadSectors(s_arrWRBuff,
               addr / FTL_SECTOR_SIZE,
               FTL_SECTOR_SIZE,
               1);
               
相当于：FTLReadSectors(s_arrWRBuff, addr / 512, 512, 1);
```

- `s_arrWRBuff`       把读取的数据放入这个 RAM 缓冲区

- `addr / 512`         起始扇区号

- `512`                      每个扇区的字节数

- 1                            读取一个扇区

### 从缓冲区取出需要的字节

```c
for(i = 0; i < len; i++)
{
    if(((addr % FTL_SECTOR_SIZE) + i) > sizeof(s_arrWRBuff))
    {
        return;
    }

    data = s_arrWRBuff[(addr % FTL_SECTOR_SIZE) + i];

    sprintf(s_arrStringBuff,
            "0x%08X: 0x%02X\r\n", addr + i, data);

    s_structGUIDev.showLine(s_arrStringBuff);
    printf("%s", s_arrStringBuff);
}
```

字节地址 addr
    ├─ addr / 512 → 找到扇区，读取到缓冲区
    └─ addr % 512 → 找到缓冲区中需要的起始位置

### 显示本次读取信息

```c
sprintf(s_arrStringBuff,
        "Read : 0x%08X - 0x%02X\r\n", addr, len);

s_structGUIDev.showLine(s_arrStringBuff);
printf("%s", s_arrStringBuff);
```

`showLine()`：通过函数指针，把字符串显示到屏幕终端。

---

## static void WriteProc(u32 addr, u8 data)

**参数代表** 

addr           要修改的逻辑字节地址

data            写入的一个字节，范围为0x00~0xff

例如

```c
WriteProc(0x205, 0xAB);
```

把0x25地址的一个字节写为0xAB

### 检查地址合法性

```c
if((addr >= s_structGUIDev.beginAddr) &&
   (addr <= s_structGUIDev.endAddr))
```

### 显示要执行的操作

```c
sprintf(s_arrStringBuff,
        "Write: 0x%08X - 0x%02X\r\n", addr, data);

s_structGUIDev.showLine(s_arrStringBuff);
printf("%s", s_arrStringBuff);
```

### 先读取地址所在的整个扇区

```c
FTLReadSectors(s_arrWRBuff,
               addr / FTL_SECTOR_SIZE,
               FTL_SECTOR_SIZE,
               1);
              
//相当于
FTLReadSectors(s_arrWRBuff, addr / 512, 512, 1);
```

把目标地址所在的 **512 字节扇区**读入 RAM 缓冲区。

**为什么要先读** 因为只想修改一个字节，其余的511个字节保留

### 修改缓冲区中的一个字节

```c
s_arrWRBuff[addr % FTL_SECTOR_SIZE] = data;
```

取余得到目标地址在扇区的位置

缓冲区下标：  0    1    2    3    4    5    6  …  511
修改前：      原值 原值 原值 原值 原值 原值 原值 … 原值
修改后：      原值 原值 原值 原值 原值  AB  原值 … 原值

### 写回整个扇区

```c
FTLWriteSectors(s_arrWRBuff,
                addr / FTL_SECTOR_SIZE,
                FTL_SECTOR_SIZE,
                1);
```

1. 空白页：尝试直接写入。

2. 已经写过的页：寻找空闲块，搬移并合入新数据，更新映射，再擦除旧块。

---

## InitReadWriteNandFlash()

### 设置允许读写的地址范围

```c
s_structGUIDev.beginAddr = 0;

s_structGUIDev.endAddr = NAND_MAX_ADDRESS - 1;
```

***s_structGUIDev 是本文件定义的结构体变量***    

可以把它理解为一张“界面与读写模块之间的联系表”，里面存放：

- 允许操作的起止地址。
- 读、写操作对应的函数。
- 在屏幕终端显示文字的函数。

**为什么结束地址要减一** 地址是从0开始，比如允许操作的字节是1024，那么结束时地址是 1023

### 设置回调函数

```c
s_structGUIDev.writeCallback = WriteProc;

//这是一个指针函数
//而上面的WriteProc()正好符合
static void WriteProc(u32 addr, u8 data);
//因此可以将它赋给 writeCallback
```

**注意** 需要注意的两种不同的写法

```c
s_structGUIDev.writeCallback = WriteProc; // 保存函数地址
WriteProc(0x205, 0xAB);                   // 立即执行函数
```

### 设置回读函数

与上面设置写入回调函数大差不差

### 设置初始化界面

```c
InitGUI(&s_structGUIDev);
```

### 关于&s_structGUIDev

这个是表示结构体变量在内存中的地址，传入地址后，就可以访问和修改同一个地址

```c
// 连接函数指针
dev->clearTherminal = ClearGUITerminal;
dev->showLine = ShowStringLineInGUITerminal;

// 保存设备指针
s_pGUIDev = dev;
```

---

## InitFTL()

本函数的返回情况是：

| 结果 | 含义                   |
| ---- | ---------------------- |
| `0`  | 初始化成功             |
| `1`  | `InitNandFlash()` 失败 |
| `2`  | 自动格式化失败         |

### 将NADA参数保存到结构体

```c
s_structFTLDev.pageTotalSize = NAND_PAGE_TOTAL_SIZE;
s_structFTLDev.pageMainSize  = NAND_PAGE_SIZE;
s_structFTLDev.pageSpareSize = NAND_SPARE_AREA_SIZE;
s_structFTLDev.blockPageNum  = NAND_BLOCK_SIZE;
s_structFTLDev.planeBlockNum = NAND_ZONE_SIZE;
s_structFTLDev.blockTotalNum = NAND_BLOCK_COUNT;
```

**成员及其代表含义** 

| 成员            | 数值 | 含义                          |
| --------------- | ---- | ----------------------------- |
| `pageTotalSize` | 2112 | 每页主区与 Spare 区的总字节数 |
| `pageMainSize`  | 2048 | 每页主数据区字节数            |
| `pageSpareSize` | 64   | 每页 Spare 区字节数           |
| `blockPageNum`  | 64   | 每块包含的页数                |
| `planeBlockNum` | 1024 | 每个 Plane 包含的块数         |
| `blockTotalNum` | 1024 | 整个 NAND 的块数              |

### 检查扇区大小

 校验扇区大小，要保证扇区大小小于等于一页，而且为512整数倍

```c
if((0 == s_structFTLDev.pageMainSize / FTL_SECTOR_SIZE)
   || (0 != FTL_SECTOR_SIZE % 512))
{
  printf("InitFTL: error sector size\r\n");
  while(1);
} 
```

### 初始化NADA底层

```c
if(InitNandFlash())
{
  return 1;
}
```

### 建立LUT映射表

```c
if(0 != CreateLUT())
{
  printf("InitFTL: format nand flash...\r\n");

  if(FTLFormat())
  {
    printf("InitFTL: format failed!\r\n");
    return 2;
  }
}
```

可以理解为 逻辑块号 → 物理块号

比如

逻辑块0的数据在物理块3
逻辑块1的数据在物理块7

```c
lut[0] = 3;
lut[1] = 7;
```

### 这个函数在项目中的调用关系

```c
main()
  → InitFTL()
      → 保存 NAND 参数
      → 检查扇区配置，算出每页4个扇区
      → InitNandFlash()
      → CreateLUT()
          → 成功：继续
          → 失败：FTLFormat()，内部重建 LUT
      → 打印块信息，返回0

  → InitReadWriteNandFlash()
      → 设置界面及读写回调
```

---

## u32 FTLReadSectors(u8 *pBuffer, u32 sectorNo, u32 sectorSize, u32 sectorCount)

### 参数意义

| 参数          | 含义                               | 示例  |
| ------------- |:----------------------------------:| ----- |
| `pBuffer`     | 存放读取结果的 RAM 地址            | `buf` |
| `sectorNo`    | 起始逻辑扇区号，从 0 开始          | `261` |
| `sectorSize`  | 一个扇区的字节数，本项目固定为 512 | `512` |
| `sectorCount` | 连续读取几个扇区                   | `2`   |

**例如**

```c
u8 buf[1024];

FTLReadSectors(buf, 261, 512, 2);
```

### 计算逻辑块号、页号、列地址

**这是函数中最需要理解的部分：** 

```c
readBlock = (sectorNo / s_structFTLDev.sectorPerPage)
            / s_structFTLDev.blockPageNum;

readPage = (sectorNo / s_structFTLDev.sectorPerPage)
           % s_structFTLDev.blockPageNum;

readColumn = (sectorNo % s_structFTLDev.sectorPerPage)
             * sectorSize;
```

```c
sectorNo / sectorPerPage  每页 4 个扇区，所以“扇区号除以 4”
得到逻辑空间中从头开始数的页号
readBlock = 全局逻辑页号 / 64;
readPage  = 全局逻辑页号 % 64;
每 64 页构成一块：
- 除以 64，得到属于哪个逻辑块。
- 对 64 取余，得到在这个块内的页号。
```

---

## u32 FTLWriteSectors(u8 *pBuffer, u32 sectorNo, u32 sectorSize, u32 sectorCount)

```c
RETRY1_MARK:

logicBlock  = (sectorNo / s_structFTLDev.sectorPerPage)
              / s_structFTLDev.blockPageNum;

writePage   = (sectorNo / s_structFTLDev.sectorPerPage)
              % s_structFTLDev.blockPageNum;

writeColumn = (sectorNo % s_structFTLDev.sectorPerPage)
              * sectorSize;
```

和上面一样，可以理解为

```c
logicBlock  = sectorNo / 256;
writePage   = (sectorNo / 4) % 64;
writeColumn = (sectorNo % 4) * 512;
```

```textile
WriteProc(addr, data)
│
├─ FTLReadSectors(...)
│    先读取512字节，保留其他字节
│
├─ s_arrWRBuff[addr % 512] = data
│    在RAM中修改一个字节
│
└─ FTLWriteSectors(...)
     │
     ├─ FTLLogicNumToPhysicalNum()
     │    查出物理块
     │
     ├─ NandCheckPage()
     │    检查整页Main区是否全为FF
     │
     ├─ 全为FF → NandWritePage()
     │
     └─ 已有数据 → FTLFineUnuseBlock()
                   → NandCopyBlockWithWrite()
                   → 更新LUT
                   → NandEraseBlock()
```

---

## EXMC

### 她有什么作用

EXMC，外部存储器控制器。CPU读写一个指定地址，EMMC就会把这次访问转换为外部引脚的读写时序，完成和其他外设的通讯。

发送一个字节可以这样

```tex
设置数据引脚D0～D7
→ 设置命令/地址选择信号
→ 拉低写使能信号
→ 等待一定时间
→ 拉高写使能信号
```

使用 EXMC 后，程序可以写

```c
NAND_CMD_AREA = 0x80;
```

EXMC 硬件会根据事先配置的参数，自动控制相关引脚，产生这一次传输需要的时序。

| 外部器件   | EXMC配置区域  | 配置位置      |
| ---------- | ------------- | ------------- |
| LCD        | Bank0 Region1 | `EXMC.c`      |
| 外部SRAM   | Bank0 Region3 | `EXMC.c`      |
| NAND Flash | Bank1         | `NandFlash.c` |

NAND 主要涉及这些信号：

| 信号   | 作用                 |
| ------ | -------------------- |
| D0～D7 | 传输命令、地址、数据 |
| CLE    | 表示当前传输的是命令 |
| ALE    | 表示当前传输的是地址 |
| WE#    | 写使能               |
| RE#    | 读使能               |
| CE#    | 片选，选中NAND       |
| R/B#   | NAND报告忙或就绪     |

```text
nand_timing_init_struct.setuptime       = 2;
建立时间：正式传输前，让相关信号提前稳定
nand_timing_init_struct.waittime        = 3;
等待时间：给外部器件完成本次访问的时间
nand_timing_init_struct.holdtime        = 2;
保持时间：传输后，让相关信号继续保持
nand_timing_init_struct.databus_hiztime = 2;
高阻时间：给总线释放、切换留出时间
```

### 配置好以后，为什么“写地址”就能控制 NAND

| CPU访问地址  | 用途         |
| ------------ | ------------ |
| `0x70010000` | 发送NAND命令 |
| `0x70020000` | 发送NAND地址 |
| `0x70000000` | 读写NAND数据 |

例如

```c
NAND_CMD_AREA = 0x80;
//相当于
*(__IO u8 *)0x70010000 = 0x80;
```

0x70010000 → 告诉EXMC：这次访问要发送命令
0x80       → 告诉NAND：发送的命令内容是0x80

---

## u32 NandReadID(void)

```c
NAND_CMD_AREA = NAND_CMD_READID;
//头文件定义
#define NAND_CMD_READID ((uint8_t)0x90)
//所以相当于
NAND_CMD_AREA = 0x90;
```

```tremor
CPU向0x70010000写入0x90
        ↓
EXMC产生命令写入时序
        ↓
NAND收到0x90，准备执行读取ID操作
```

**从数据入口读取32位结果** 

```c
id = *(__IO u32 *)(BANK_NAND_ADDR | EXMC_DATA_AREA);
BANK_NAND_ADDR = 0x70000000
EXMC_DATA_AREA = 0x00000000
0x70000000 | 0x00000000 = 0x70000000
//所以相当于
id = *(__IO u32 *)0x70000000;
```

```c
NAND_ADDR_AREA = (eccAddr >> 0) & 0xFF;
//这样表示取低八位，为什么要与运算呢
0xFF = 00000000 11111111
column = 512 = 0x0200

column： 00000010 00000000
0x00FF： 00000000 11111111
         ─────────────────
结果：   00000000 00000000
这样就可以把上面高位的变成0啦
```
