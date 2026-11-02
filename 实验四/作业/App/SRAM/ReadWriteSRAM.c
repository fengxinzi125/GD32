/*********************************************************************************************************
* 模块名称：ReadWriteSRAM.c
* 摘    要：读写SRAM模块
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
#include "ReadWriteSRAM.h"
#include "gd32f30x_conf.h"
#include "GUITop.h"
#include "stdio.h"
#include "LCD.h"
/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
#define BANK0_REGON3_ADDR ((u32)(0x6C000000))    //Bank0 Regon3起始地址
#define EX_SRAM_BASE_ADDR BANK0_REGON3_ADDR      //外部SRAM起始地址
#define EX_SRAM_BUFFER_SIZE    (1 * 1024 * 1024) //外部SRAM缓冲区大小，1M字节
#define IN_SRAM_BUFFER_SIZE    (4 * 1024)        //内部SRAM缓冲区大小
#define MAX_STRING_LEN         (64)              //显示字符最大长度

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/
static StructGUIDev s_structGUIDev;                                                //GUI设备结构体
static u8   s_arrExSRAM[EX_SRAM_BUFFER_SIZE] __attribute__((at((u32)0x6C000000))); //外部SRAM缓冲区
static u8   s_arrInSRAM[IN_SRAM_BUFFER_SIZE];                                      //内部SRAM缓冲区
static char s_arrStringBuff[MAX_STRING_LEN];                                       //字符串转换缓冲区

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static u8   ReadByte(u32 addr);             //读取特定地址数据（1字节）
static void WriteByte(u32 addr, u8 data);   //往特定地址内写入1字节数据
static void ReadSRAM(u32 addr, u32 len);  //按字节读取SRAM
static void WriteSRAM(u32 addr, u8 data); //按字节写入SRAM

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：ReadByte
* 函数功能：读取特定地址数据（1字节）
* 输入参数：addr：读取地址
* 输出参数：void
* 返 回 值：读到的数据
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static u8 ReadByte(u32 addr)
{
  return *(u8*)addr;
}

/*********************************************************************************************************
* 函数名称：WriteByte
* 函数功能：往特定地址内写入1字节数据
* 输入参数：addr：写入地址，data：写入数据（字节）
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void WriteByte(u32 addr, u8 data)
{
  *(u8*)addr = data;
}

/*********************************************************************************************************
* 函数名称：ReadSRAM
* 函数功能：读取SRAM
* 输入参数：addr：读取地址，len：读取长度（字节）
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void ReadSRAM(u32 addr, u32 len)
{
  u32 i;    //循环变量
  u8  data; //读取到的数据

  //输出读取信息到终端和串口
  sprintf(s_arrStringBuff, "Read : 0x%08X - 0x%02X\r\n", addr, len);
  s_structGUIDev.showLine(s_arrStringBuff);
  printf("%s", s_arrStringBuff);

  //读取数据，并打印到终端和串口上
  for(i = 0; i < len; i++)
  {
    //读取
    data = ReadByte(addr + i);

    //输出
    sprintf(s_arrStringBuff, "0x%08X: 0x%02X\r\n", addr + i, data);
    s_structGUIDev.showLine(s_arrStringBuff);
    printf("%s", s_arrStringBuff);
  }
}

/*********************************************************************************************************
* 函数名称：WriteSRAM
* 函数功能：按字节写入SRAM
* 输入参数：addr：写入地址，data：写入数据
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void WriteSRAM(u32 addr, u8 data)
{
  //将数据写入外部SRAM
  if((addr >= s_structGUIDev.exSRAMBeginAddr) && (addr <= s_structGUIDev.exSRAMEndAddr))
  {
    //输出信息到终端和串口
    sprintf(s_arrStringBuff, "Write: 0x%08X - 0x%02X\r\n", addr, data);
    s_structGUIDev.showLine(s_arrStringBuff);
    printf("%s", s_arrStringBuff);

    //写入外部SRAM
    WriteByte(addr, data);
  }

  //将数据写入内部SRAM
  else if((addr >= s_structGUIDev.inSRAMBeginAddr) && (addr <= s_structGUIDev.inSRAMEndAddr))
  {
    //输出信息到终端和串口
    sprintf(s_arrStringBuff, "Write: 0x%08X - 0x%02X\r\n", addr, data);
    s_structGUIDev.showLine(s_arrStringBuff);
    printf("%s", s_arrStringBuff);

    //写入内部SRAM
    WriteByte(addr, data);
  }
  else
  {
    //无效地址
    s_structGUIDev.showLine("Write: Invalid address\r\n");
    printf("Write: Invalid address\r\n");
  }
}

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitReadWriteSRAM
* 函数功能：初始化读写SRAM模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitReadWriteSRAM(void)
{
  u32 i;

  //获取外部SRAM缓冲区首地址
  s_structGUIDev.exSRAMBeginAddr = (u32)s_arrExSRAM;

  //获取外部SRAM缓冲区结束地址
  s_structGUIDev.exSRAMEndAddr = s_structGUIDev.exSRAMBeginAddr + sizeof(s_arrExSRAM) / sizeof(u8) - sizeof(u8);

  //获取内部SRAM缓冲区首地址
  s_structGUIDev.inSRAMBeginAddr = (u32)s_arrInSRAM;

  //获取内部SRAM缓冲区结束地址
  s_structGUIDev.inSRAMEndAddr = s_structGUIDev.inSRAMBeginAddr + sizeof(s_arrInSRAM) / sizeof(u8) - sizeof(u8);

  //设置写入回调函数
  s_structGUIDev.writeCallback = WriteSRAM;

  //设置读取回调函数
  s_structGUIDev.readCallback = ReadSRAM;

  //初始化UI界面设计
  InitGUI(&s_structGUIDev);

  //初始化外部SRAM缓冲区
  for(i = 0; i < sizeof(s_arrExSRAM) / sizeof(u8); i++)
  {
    s_arrExSRAM[i] = 0;
  }

  //初始化内部SRAM缓冲区
  for(i = 0; i < sizeof(s_arrInSRAM) / sizeof(u8); i++)
  {
    s_arrInSRAM[i] = 0;
  }
	
  //打印地址范围到终端和串口
  sprintf(s_arrStringBuff, "Ex Addr: 0x%08X - 0x%08X\r\n", s_structGUIDev.exSRAMBeginAddr, s_structGUIDev.exSRAMEndAddr);
  s_structGUIDev.showLine(s_arrStringBuff);
  printf("%s", s_arrStringBuff);
  sprintf(s_arrStringBuff, "In Addr: 0x%08X - 0x%08X\r\n", s_structGUIDev.inSRAMBeginAddr, s_structGUIDev.inSRAMEndAddr);
  s_structGUIDev.showLine(s_arrStringBuff);
  printf("%s", s_arrStringBuff);
}

/*********************************************************************************************************
* 函数名称：ReadWriteSRAMTask
* 函数功能：读写SRAM模块任务
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：每隔20毫秒执行一次
*********************************************************************************************************/
void ReadWriteSRAMTask(void)
{
  GUITask(); //GUI任务
}


void SRAM_Test(void)
{
	
	u32 i;
	u8 errCountFF = 0;
	u8 errCount00 = 0;
	char buf[64];
	
	//全片写oxff
	for(i = 0; i < EX_SRAM_BUFFER_SIZE; i++)
	{
		s_arrExSRAM[i] = 0xFF;
	}
	
	for(i = 0; i < EX_SRAM_BUFFER_SIZE; i++)
	{
		if(s_arrExSRAM[i] != 0xFF)
		{
			errCountFF ++;
			
		}
	}
	
	//串口输出
	printf("=== SRAM Test: Write 0xFF ===\r\n");
	printf("Total: %d bytes, Errors: %d\r\n", EX_SRAM_BUFFER_SIZE, errCountFF);
	if(errCountFF == 0)
    printf("Result: PASS\r\n");
	else
    printf("Result: FAIL\r\n");
	
	//全片写ox00验证
	for(i = 0; i < EX_SRAM_BUFFER_SIZE; i++)
	{
		s_arrExSRAM[i] = 0x00;
	}
	for(i = 0; i< EX_SRAM_BUFFER_SIZE; i++)
	{
		if(s_arrExSRAM[i] != 0x00)
		errCount00 ++;
	}
	
	// 6. 串口输出结果
	printf("=== SRAM Test: Write 0x00 ===\r\n");
	printf("Total: %d bytes, Errors: %d\r\n", EX_SRAM_BUFFER_SIZE, errCount00);
	if(errCount00 == 0)
			printf("Result: PASS\r\n");
	else
			printf("Result: FAIL\r\n");
	
	LCDClear(WHITE);
	LCDShowString(10, 10, 400, 16, 16, (u8*)"=== External SRAM Test ===");
	
	sprintf(buf, "Write 0xFF: %s (Err:%d)", errCountFF==0?"PASS":"FAIL", errCountFF);
	LCDShowString(10, 40, 400, 16, 16, (u8*)buf);
	
	sprintf(buf, "Write 0x00: %s (Err:%d)", errCount00==0?"PASS":"FAIL", errCount00);
	LCDShowString(10, 60, 400, 16, 16, (u8*)buf);
	
	if(errCountFF == 0 && errCount00 == 0)
			LCDShowString(10, 90, 400, 16, 16, (u8*)"Overall: PASS");
	else
			LCDShowString(10, 90, 400, 16, 16, (u8*)"Overall: FAIL");
	
	
	
}