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

## InitReadWriteNandFlash

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

## InitFTL
