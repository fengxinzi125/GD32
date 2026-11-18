/*********************************************************************************************************
* 模块名称：GD25QXX.c
* 摘    要：GD25QXX模块
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：注意勾选Options for Target 'Target1'->Code Generation->Use MicroLIB，否则printf无法使用                                                                  
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期：
* 修改内容：
* 修改文件：
*********************************************************************************************************/

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "GD25QXX.h"
#include "gd32f30x_conf.h"
#include "SPI.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
#define SET_SPI_NSS()       gpio_bit_set(GPIOE, GPIO_PIN_2)
#define CLR_SPI_NSS()       gpio_bit_reset(GPIOE, GPIO_PIN_2)

#define W25Q16_PAGE_SIZE    256                            //一页的大小，256字节
#define W25Q16_SECTOR_SIZE (4 * 1024)                      //扇区大小，字节
#define W25Q16_BLOCK_SIZE  (16 * W25Q16_SECTOR_SIZE)
#define W25Q16_SIZE        (32 * W25Q16_BLOCK_SIZE)

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/
const unsigned char byDUMMY = 0xff;

//SPI Flash擦写缓冲区
static unsigned char s_arrWriteBuf[W25Q16_SECTOR_SIZE];

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static void GD25Q16WriteEnable(void);
static void GD25Q16WaitForWriteEnd(void);
static void GD25Q16PageProgram(unsigned char *buf, unsigned short len, unsigned int addr);

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：GD25Q16WriteEnable
* 函数功能：写使能
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void GD25Q16WriteEnable(void)
{
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x06);
  SET_SPI_NSS();
}

/*********************************************************************************************************
* 函数名称：GD25Q16WaitForWriteEnd
* 函数功能：等待FLASH内部时序操作完成
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void GD25Q16WaitForWriteEnd(void)
{
  unsigned char status = 0;
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x05);
  
  do
  {
    status = SPI1ReadWriteByte(byDUMMY);
  }
  while((status & 0x01) == 1);
  
  SET_SPI_NSS();
}

/*********************************************************************************************************
* 函数名称：GD25Q16PageProgram
* 函数功能：页写 
* 输入参数：buf-要写的数据,len-数据长度,addr-起始地址
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void GD25Q16PageProgram(unsigned char *buf, unsigned short len, unsigned int addr)
{
  GD25Q16WriteEnable();
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x02);
  SPI1ReadWriteByte((addr & 0xFF0000) >> 16);
  SPI1ReadWriteByte((addr & 0x00FF00) >> 8);
  SPI1ReadWriteByte(addr & 0xFF);
  
  while (len--)
  {
    SPI1ReadWriteByte(*buf);
    buf++;
  }
  
  SET_SPI_NSS();
  GD25Q16WaitForWriteEnd();
}

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitGD25QXX
* 函数功能：GD25QXX模块初始化
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitGD25QXX(void)
{
  //配置PE2
  rcu_periph_clock_enable(RCU_GPIOE);                                //使能GPIOE的时钟
  gpio_init(GPIOE, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2); //设置GPIO输出模式及速度
  gpio_bit_set(GPIOE, GPIO_PIN_2);                                   //将PE2默认状态设置为高电平

  //初始化SPI1
  InitSPI1();
}

/*********************************************************************************************************
* 函数名称：W25q64_Write
* 函数功能：数据存储 
* 输入参数：buf-要写的数据,len-数据长度,addr-起始地址 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void GD25Q16Write(unsigned char *buf, unsigned int len, unsigned int addr)
{
  unsigned int   writeAddr;  //当前写入地址
  unsigned int   dataRemain; //当前剩余数据量
  unsigned char* dataBuf;    //写入数据缓冲区首地址
  unsigned int   sectorAddr; //当前扇区首地址
  unsigned int   i, j;       //循环变量

  //设初值
  writeAddr  = addr; //设置写入地址
  dataRemain = len;  //设置剩余数据量
  dataBuf    = buf;  //保存数据缓冲区首地址

  //循环写入，每次擦写按扇区进行
  while(0 != dataRemain)
  {
    //计算当前扇区首地址
    sectorAddr = writeAddr - (writeAddr % W25Q16_SECTOR_SIZE);

    //写入地址不是扇区首地址，需要将当前扇区的数据读进来，修改后再写回去
    if(0 != writeAddr % W25Q16_SECTOR_SIZE)
    {
      //读入一整个扇区数据
      GD25Q16Read(s_arrWriteBuf, W25Q16_SECTOR_SIZE, sectorAddr);
    }

    //写入地址为扇区首地址但写入量不足一个扇区，也要将当前扇区的数据读进来，修改后再写回去
    else if((0 == writeAddr % W25Q16_SECTOR_SIZE) && (dataRemain < W25Q16_SECTOR_SIZE))
    {
      //读入一整个扇区数据
      GD25Q16Read(s_arrWriteBuf, W25Q16_SECTOR_SIZE, sectorAddr);
    }

    //擦除一整个扇区
    GD25Q16X4KErase(sectorAddr);

    //按页修改一整个扇区数据
    for(i = (writeAddr % W25Q16_SECTOR_SIZE) / W25Q16_PAGE_SIZE; i < (W25Q16_SECTOR_SIZE / W25Q16_PAGE_SIZE); i++)
    {
      //数据已全部写入，则跳出循环
      if(0 == dataRemain)
      {
        break;
      }

      //修改缓冲区中某一页的数据
      for(j = writeAddr % W25Q16_PAGE_SIZE; j < W25Q16_PAGE_SIZE; j++)
      {
        //数据已全部写入，则跳出循环
        if(0 == dataRemain)
        {
          break;
        }

        //修改缓冲区中的数据
        s_arrWriteBuf[i * W25Q16_PAGE_SIZE + j] = *dataBuf;

        //更新写入地址、剩余数据量以及数据缓冲区首地址
        writeAddr = writeAddr + 1;
        dataRemain = dataRemain - 1;
        dataBuf = dataBuf + 1;
      }
    }

    //修改后写回SPI Flash，按页写
    for(i = 0; i < (W25Q16_SECTOR_SIZE / W25Q16_PAGE_SIZE); i++)
    {
      GD25Q16PageProgram(s_arrWriteBuf + i * W25Q16_PAGE_SIZE, W25Q16_PAGE_SIZE, sectorAddr + i * W25Q16_PAGE_SIZE);
    }
  }
}

/*********************************************************************************************************
* 函数名称：GD25Q16Read
* 函数功能：读数据
* 输入参数：buf-数据缓冲区,len-长度,addr-起始地址 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void GD25Q16Read(unsigned char *buf, unsigned int len, unsigned int addr)
{
  unsigned int i;
  
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x03);
  SPI1ReadWriteByte((addr & 0xFF0000) >> 16);
  SPI1ReadWriteByte((addr & 0x00FF00) >> 8);
  SPI1ReadWriteByte(addr & 0xFF);
  
  for (i = 0; i < len; i++)
  {
    buf[i] = SPI1ReadWriteByte(byDUMMY);
  }
  
  SET_SPI_NSS();
}

/*********************************************************************************************************
* 函数名称：GD25Q16ReadStatus
* 函数功能：读状态寄存器 
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned short GD25Q16ReadStatus(void)
{
  unsigned short status=0;
  
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x05);
  status = SPI1ReadWriteByte(byDUMMY);
  SET_SPI_NSS();
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x35);
  status |= SPI1ReadWriteByte(byDUMMY) << 8;
  SET_SPI_NSS();
  
  return status;
}

/*********************************************************************************************************
* 函数名称：GD25Q16ReadDeviceID
* 函数功能：读芯片ID
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned short GD25Q16ReadDeviceID(void)
{
  unsigned char DeviceID = 0;
  
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0xAB);
  SPI1ReadWriteByte(byDUMMY);
  SPI1ReadWriteByte(byDUMMY);
  SPI1ReadWriteByte(byDUMMY);
  DeviceID = SPI1ReadWriteByte(0X00);
  SET_SPI_NSS();
  
  return DeviceID;
}

/*********************************************************************************************************
* 函数名称：GD25Q16ReadJEDEID
* 函数功能：读芯片ID 
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
unsigned short GD25Q16ReadJEDEID(void)
{
  unsigned short DeviceID = 0;
  
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x9F);
  SPI1ReadWriteByte(byDUMMY);
  DeviceID = SPI1ReadWriteByte(byDUMMY) << 8;
  DeviceID |= SPI1ReadWriteByte(byDUMMY);
  SET_SPI_NSS();
  
  return DeviceID;
}

/*********************************************************************************************************
* 函数名称：GD25Q16X4KErase
* 函数功能：4K片擦除
* 输入参数：addr-起始地址
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void  GD25Q16X4KErase(unsigned int addr)
{
  GD25Q16WriteEnable();
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x20);
  SPI1ReadWriteByte((addr & 0xFF0000) >> 16);
  SPI1ReadWriteByte((addr & 0x00FF00) >> 8);
  SPI1ReadWriteByte(addr & 0xFF);
  SET_SPI_NSS();
  GD25Q16WaitForWriteEnd();
}

/*********************************************************************************************************
* 函数名称：GD25Q16X32KErase
* 函数功能：32K片擦除
* 输入参数：addr-起始地址 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void  GD25Q16X32KErase(unsigned int addr)
{
  GD25Q16WriteEnable();
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0x52);
  SPI1ReadWriteByte((addr & 0xFF0000) >> 16);
  SPI1ReadWriteByte((addr & 0x00FF00) >> 8);
  SPI1ReadWriteByte(addr & 0xFF);
  SET_SPI_NSS();
  GD25Q16WaitForWriteEnd();
}

/*********************************************************************************************************
* 函数名称：GD25Q16X64KErase
* 函数功能：64K片擦除 
* 输入参数：addr-起始地址
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void  GD25Q16X64KErase(unsigned int addr)
{
  GD25Q16WriteEnable();
  CLR_SPI_NSS();
  SPI1ReadWriteByte(0xD8);
  SPI1ReadWriteByte((addr & 0xFF0000) >> 16);
  SPI1ReadWriteByte((addr & 0x00FF00) >> 8);
  SPI1ReadWriteByte(addr & 0xFF);
  SET_SPI_NSS();
  GD25Q16WaitForWriteEnd();
}
