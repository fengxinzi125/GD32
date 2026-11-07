/*********************************************************************************************************
* 模块名称：ReadWriteSDCard.c
* 摘    要：读写SD卡模块
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：                                                                  
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
#include "ReadWriteSDCard.h"
#include "SDCardBySPI.h"
#include "gd32f30x_conf.h"
#include "GUITop.h"
#include "stdio.h"
#include "SysTick.h"
#include "LED.h"
#include "LCD.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
#define MAX_STRING_LEN 64 //显示字符最大长度

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/
static StructGUIDev s_structGUIDev;                      //GUI设备结构体
static u8   s_arrSDBuffer[2048];                         //SD卡读写缓冲区
static char s_arrStringBuff[MAX_STRING_LEN];             //字符串转换缓冲区

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static u8*  Read(u32 addr, u32 len);                     //按字节读取SD卡
static void Write(u32 addr, u8 data);                    //按字节写入SD卡
static void ReadSDCard(u32 addr, u32 len);               //读取SD卡
static void WriteSDCard(u32 addr, u8 data);              //写入SD卡

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：Read
* 函数功能：按字节读取SD卡
* 输入参数：addr：读取地址，len：读取长度（字节）
* 输出参数：void
* 返 回 值：读取缓冲区首地址
* 创建日期：2021年07月01日
* 注    意：SD卡读写以数据块的形式进行
*********************************************************************************************************/
static u8* Read(u32 addr, u32 len)
{
  u32 addrOffset; //目标地址与实际写入地址偏移量
  u8* buff;       //读取缓冲区
  u8* result;     //返回地址

  //查找数据块首地址
  addrOffset = 0;
  while(0 != (addr % SD_SECTOR_SIZE))
  {
    addr = addr - 1;
    addrOffset = addrOffset + 1;
  }

  //按数据块读入数据保存到数据缓冲区
  buff = s_arrSDBuffer;
  len = len + addrOffset;
  while(len > 0)
  {
    //计算剩余读取数据量
    if(len >= SD_SECTOR_SIZE)
    {
      len = len - SD_SECTOR_SIZE;
    }
    else
    {
      len = 0;
    }

    //读入一整个数据块
    SDReadSector(buff, addr / SD_SECTOR_SIZE, 1);

    //设置读取地址为下一个数据块
    addr = addr + SD_SECTOR_SIZE;

    //设置下一个读取缓冲区地址
    buff = buff + SD_SECTOR_SIZE;
  }

  //计算返回地址
  result = s_arrSDBuffer + addrOffset;

  return result;
}

/*********************************************************************************************************
* 函数名称：WriteSDCard
* 函数功能：按字节写入SD卡
* 输入参数：addr：写入地址，data：写入数据
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：SD卡读写以数据块的形式进行
*********************************************************************************************************/
static void Write(u32 addr, u8 data)
{
  u32 addrOffset; //目标地址与实际写入地址偏移量

  //查找数据块首地址
  addrOffset = 0;
  while(0 != (addr % SD_SECTOR_SIZE))
  {
    addr = addr - 1;
    addrOffset = addrOffset + 1;
  }

  //读取一整个数据块
  SDReadSector(s_arrSDBuffer, addr / SD_SECTOR_SIZE, 1);

  //修改数据块，将要写入的数据储存到数据块指定位置中
  s_arrSDBuffer[addrOffset] = data;

  //写入修改后的数据块
  SDWriteSector(s_arrSDBuffer, addr / SD_SECTOR_SIZE, 1);
  }

/*********************************************************************************************************
* 函数名称：ReadSDCard
* 函数功能：读取SD卡
* 输入参数：addr：读取地址，len：读取长度（字节）
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void ReadSDCard(u32 addr, u32 len)
{
  u32 i;    //循环变量
  u8* buff; //读取缓冲区
  u8  data; //读取到的数据

  //检查SD卡是否插入
  if(0 == SDDetect())
  {
    //校验地址范围
    if((addr >= s_structGUIDev.beginAddr) && (addr + len - 1 <= s_structGUIDev.endAddr))
    {
      //输出读取信息到终端和串口
      sprintf(s_arrStringBuff, "Read : 0x%08X - 0x%02X\r\n", addr, len);
      s_structGUIDev.showLine(s_arrStringBuff);
      printf("%s", s_arrStringBuff);

      //从SD卡中读取数据
      buff = Read(addr, len);

      //打印到终端和串口上
      for(i = 0; i < len; i++)
      {
        //读取
        data = buff[i];

        //输出
        sprintf(s_arrStringBuff, "0x%08X: 0x%02X\r\n", addr + i, data);
        s_structGUIDev.showLine(s_arrStringBuff);
        printf("%s", s_arrStringBuff);
      }
    }
    else
    {
      //无效地址
      s_structGUIDev.showLine("Read: Invalid address\r\n");
      printf("Read: Invalid address\r\n");
    }
  }
  else
  {
    s_structGUIDev.showLine("Read: SD card not inserted\r\n");
    printf("Read: SD card not inserted\r\n");
  }
}

/*********************************************************************************************************
* 函数名称：WriteSDCard
* 函数功能：写入SD卡
* 输入参数：addr：写入地址，data：写入数据
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void WriteSDCard(u32 addr, u8 data)
{
  //检查SD卡是否插入
  if(0 == SDDetect())
  {
    //校验地址范围
    if((addr >= s_structGUIDev.beginAddr) && (addr <= s_structGUIDev.endAddr))
    {
      //输出信息到终端和串口
      sprintf(s_arrStringBuff, "Write: 0x%08X - 0x%02X\r\n", addr, data);
      s_structGUIDev.showLine(s_arrStringBuff);
      printf("%s", s_arrStringBuff);

      //写入SD卡
      Write(addr, data);
    }
    else
    {
      //无效地址
      s_structGUIDev.showLine("Write: Invalid address\r\n");
      printf("Write: Invalid address\r\n");
    }
  }
  else
  {
    s_structGUIDev.showLine("Write: SD card not inserted\r\n");
    printf("Write: SD card not inserted\r\n");
  }
}

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitReadWriteSDCard
* 函数功能：初始化读写SD卡模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitReadWriteSDCard(void)
{
  u8  firstShowFlag;
  
  //初始化SD卡
  firstShowFlag = 1;
  while(SD_OK != InitSDCard())
  {
    //LCD输出提示信息
    if(1 == firstShowFlag)
    {
      firstShowFlag = 0;
      LCDDisplayDir(1);
      s_iLCDPointColor = BRED;
      s_iLCDBackColor = WHITE;
      LCDClear(WHITE);
      LCDShowString(250, 200, 300, 30, 24, (u8*)"Please insert SD card");
    }

    //串口输出提示信息
    printf("Failed to initialize SD card\r\n"); 

    //LED灯闪烁
    LEDCtrl(0, 0);
    DelayNms(100);
    LEDCtrl(0, 1);
    DelayNms(100);

    LEDCtrl(0, 0);
    DelayNms(100);
    LEDCtrl(0, 1);
    DelayNms(100);

    LEDCtrl(0, 0);
    DelayNms(1000);
  }
  printf("Initialize SD card successfully\r\n");

  //SD卡首地址
  s_structGUIDev.beginAddr = 0;

  //SD卡结束地址
  s_structGUIDev.endAddr = s_structGUIDev.beginAddr + SDGetSectorCount() * SD_SECTOR_SIZE - 1;

  //设置写入回调函数
  s_structGUIDev.writeCallback = WriteSDCard;

  //设置读取回调函数
  s_structGUIDev.readCallback = ReadSDCard;

  //初始化UI界面设计
  InitGUI(&s_structGUIDev);

  //打印地址范围到终端和串口
  sprintf(s_arrStringBuff, "Addr: 0x%08X - 0x%08X\r\n", s_structGUIDev.beginAddr, s_structGUIDev.endAddr);
  s_structGUIDev.showLine(s_arrStringBuff);
  printf("%s", s_arrStringBuff);
}

/*********************************************************************************************************
* 函数名称：ReadWriteSDCardTask
* 函数功能：读写SD卡模块任务
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：每隔40毫秒执行一次
*********************************************************************************************************/
void ReadWriteSDCardTask(void)
{
  static u8 s_iTimeCnt = 0;
  static u8 s_iSDCardStatus = SD_OK;
  static u8 status;

  //每隔1s检查一遍SD卡是否存在
  s_iTimeCnt++;
  if(s_iTimeCnt >= 25)
  {
    //计时器清零
    s_iTimeCnt = 0;

    //SD卡插入检测
    status = SDDetect();

    //检测到SD卡插入则重新初始化SD卡
    if((SD_ERROR == s_iSDCardStatus) && (SD_OK == status))
    {
      //等待SD卡插入稳定
      DelayNms(1000);

      //输出提示信息到串口和终端
      printf("Reinitialize the SD card\r\n");
      s_structGUIDev.showLine("Reinitialize the SD card\r\n");

      //重新初始化
      InitSDCard();
    }
    s_iSDCardStatus = status;
  }

  GUITask(); //GUI任务
}

