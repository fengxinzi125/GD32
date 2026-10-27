/*********************************************************************************************************
* 模块名称：LCD.c
* 摘    要：LCD模块
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
#include "LCD.h"
#include "stdlib.h"
#include "Font.h"
#include "stdio.h"
#include "systick.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/
//LCD的画笔颜色和背景色
u16 s_iLCDPointColor = 0x0000;  //画笔颜色
u16 s_iLCDBackColor  = 0xFFFF;  //背景色

//管理LCD重要参数
//默认为竖屏
StructLCDDev s_structLCDDev;

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：LCDWriteCMD
* 函数功能：向LCD写命令 
* 输入参数：cmd：命令
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDWriteCMD(u16 cmd)
{
  LCD->cmd = cmd;
}

/*********************************************************************************************************
* 函数名称：LCDWriteData
* 函数功能：向LCD写数据 
* 输入参数：data：数据
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDWriteData(u16 data)
{
  LCD->data = data;
}

/*********************************************************************************************************
* 函数名称：LCDReadData
* 函数功能：从LCD读数据 
* 输入参数：void
* 输出参数：void
* 返 回 值：从LCD中读到的数据
* 创建日期：2021年07月01日
* 注    意：加volatile关键字是为了防止编译器优化
*********************************************************************************************************/
u16 LCDReadData(void)
{
  volatile u16 ram;
  ram = LCD->data;
  return ram;
}

/*********************************************************************************************************
* 函数名称：LCDWriteReg
* 函数功能：写寄存器
* 输入参数：reg：寄存器地址，value：寄存器数值
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDWriteReg(u16 reg,u16 value)
{
  LCD->cmd  = reg;    //写入要写的寄存器序号
  LCD->data = value;  //写入数据
}

/*********************************************************************************************************
* 函数名称：LCDReadReg
* 函数功能：读寄存器
* 输入参数：reg：寄存器地址
* 输出参数：void
* 返 回 值：寄存器数值
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
u16 LCDReadReg(u16 reg)
{
  LCDWriteCMD(reg);     //写入要读的寄存器序号
  DelayNus(5);          //延时5微秒
  return LCDReadData(); //返回读到的值
}

/*********************************************************************************************************
* 函数名称：LCDSendWriteGramCMD
* 函数功能：发送开始写GRAM命令
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDSendWriteGramCMD(void)
{
  LCD->cmd = s_structLCDDev.wramcmd;
}

/*********************************************************************************************************
* 函数名称：LCDWriteRAM
* 函数功能：写GRAM
* 输入参数：rgb：颜色值
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDWriteRAM(u16 rgb)
{
  LCD->data = rgb; //写十六位GRAM
}

/*********************************************************************************************************
* 函数名称：LCDBGRToRGB
* 函数功能：BGR转RGB
* 输入参数：bgr：颜色值
* 输出参数：void
* 返 回 值：RGB
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
u16 LCDBGRToRGB(u16 bgr)
{
  u16  r, g, b, rgb;
  b   = (bgr >> 0 ) & 0x1F;
  g   = (bgr >> 5 ) & 0x3F;
  r   = (bgr >> 11) & 0x1F;
  rgb = (b   << 11) + (g << 5) + (r << 0);
  return(rgb);
} 

/*********************************************************************************************************
* 函数名称：LCDReadPoint
* 函数功能：读取个某点的颜色值
* 输入参数：x,y：坐标
* 输出参数：void
* 返 回 值：颜色值
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
u16 LCDReadPoint(u16 x, u16 y)
{
  u16 r = 0;
  u16 g = 0;
  u16 b = 0;

  //超过了范围,直接返回
  if(x >= s_structLCDDev.width || y >= s_structLCDDev.height)
  {
    return 0;	
  }

  //设置光标位置
  LCDSetCursor(x, y);

  //5510 发送读GRAM指令
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(0x2E00);

    //假读
    r = LCDReadData();

    //读取实际坐标颜色
    DelayNus(2);
    r = LCDReadData();

    //NT35510要分2次读出
    DelayNus(2);
    b = LCDReadData(); 
    g = r & 0xFF; //对于5510,第一次读取的是RG的值,R在前,G在后,各占8位
    g <<= 8;

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
  }
  
  //9806 发送读GRAM指令
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(0X2E);
    
    //假读
    r = LCDReadData();
    
    //读取实际坐标颜色
    DelayNus(2);
    r = LCDReadData();
    
    //ILI9806要分2次读出
    DelayNus(2);
    b = LCDReadData(); 
    g = r & 0xFF; //对于9806,第一次读取的是RG的值,R在前,G在后,各占8位
    g <<= 8;
    
    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
  }

  //其他IC
  else 
  {
    return 0;
  }
}

/*********************************************************************************************************
* 函数名称：LCDDisplayOn
* 函数功能：LCD开启显示
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDisplayOn(void)
{
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(0x2900);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(0x29);
  }
}

/*********************************************************************************************************
* 函数名称：LCDDisplayOff
* 函数功能：LCD关闭显示
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDisplayOff(void)
{
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(0x2800);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(0x28);
  }
}

/*********************************************************************************************************
* 函数名称：LCDSetCursor
* 函数功能：设置光标位置
* 输入参数：x：横坐标，y：纵坐标
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDSetCursor(u16 x, u16 y)
{
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd); LCDWriteData(x >> 8);
    LCDWriteCMD(s_structLCDDev.setxcmd + 1); LCDWriteData(x & 0xFF);
    LCDWriteCMD(s_structLCDDev.setycmd); LCDWriteData(y >> 8);
    LCDWriteCMD(s_structLCDDev.setycmd + 1); LCDWriteData(y & 0xFF);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd);
    LCDWriteData(x >> 8);LCDWriteData(x & 0xFF);
    
    LCDWriteCMD(s_structLCDDev.setycmd); 
    LCDWriteData(y >> 8); LCDWriteData(y & 0xFF);
  }
}

/*********************************************************************************************************
* 函数名称：LCDScanDir
* 函数功能：dir：自动扫描方向
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDScanDir(u8 dir)
{
  u16 regval = 0;
  u16 dirreg = 0;
  u16 temp;

  //横屏时，对6804和1963不改变扫描方向！竖屏时1963改变方向
  if(s_structLCDDev.dir == 1)
  {
    switch(dir)//方向转换
    {
      case 0: dir = 6; break;
      case 1: dir = 7; break;
      case 2: dir = 4; break;
      case 3: dir = 5; break;
      case 4: dir = 1; break;
      case 5: dir = 0; break;
      case 6: dir = 3; break;
      case 7: dir = 2; break;
    }
  }

  //5510特殊处理
  if(s_structLCDDev.id == 0x5510 || s_structLCDDev.id == 0x9806)
  {
    switch(dir)
    {
      case L2R_U2D: //从左到右,从上到下
        regval |= (0 << 7) | (0 << 6) | (0 << 5); 
        break;
      case L2R_D2U: //从左到右,从下到上
        regval |= (1 << 7) | (0 << 6) | (0 << 5); 
        break;
      case R2L_U2D: //从右到左,从上到下
        regval |= (0 << 7) | (1 << 6) | (0 << 5); 
        break;
      case R2L_D2U: //从右到左,从下到上
        regval |= (1 << 7) | (1 << 6) | (0 << 5); 
        break;	 
      case U2D_L2R: //从上到下,从左到右
        regval |= (0 << 7) | (0 << 6) | (1 << 5); 
        break;
      case U2D_R2L: //从上到下,从右到左
        regval |= (0 << 7) | (1 << 6) | (1 << 5); 
        break;
      case D2U_L2R: //从下到上,从左到右
        regval |= (1 << 7) | (0 << 6) | (1 << 5); 
        break;
      case D2U_R2L: //从下到上,从右到左
        regval |= (1 << 7) | (1 << 6) | (1 << 5); 
        break;	 
    }

    //寄存器
    if(s_structLCDDev.id == 0x5510)
    {
      dirreg = 0x3600;
    }
    else
    {
      dirreg = 0x36;
    }

    //写寄存器
    LCDWriteReg(dirreg, regval);

    //方向转换
    if(regval & 0x20)
    {
      if(s_structLCDDev.width < s_structLCDDev.height) //交换X,Y
      {
        temp = s_structLCDDev.width;
        s_structLCDDev.width = s_structLCDDev.height;
        s_structLCDDev.height = temp;
      }
    }
    else
    {
      if(s_structLCDDev.width > s_structLCDDev.height)//交换X,Y
      {
        temp = s_structLCDDev.width;
        s_structLCDDev.width = s_structLCDDev.height;
        s_structLCDDev.height = temp;
      }
    }
    
    if(s_structLCDDev.id == 0x5510)
    {
      LCDWriteCMD(s_structLCDDev.setxcmd); LCDWriteData(0); 
      LCDWriteCMD(s_structLCDDev.setxcmd + 1); LCDWriteData(0); 
      LCDWriteCMD(s_structLCDDev.setxcmd + 2); LCDWriteData((s_structLCDDev.width - 1) >> 8); 
      LCDWriteCMD(s_structLCDDev.setxcmd + 3); LCDWriteData((s_structLCDDev.width - 1) & 0xFF); 
      LCDWriteCMD(s_structLCDDev.setycmd); LCDWriteData(0); 
      LCDWriteCMD(s_structLCDDev.setycmd + 1); LCDWriteData(0); 
      LCDWriteCMD(s_structLCDDev.setycmd + 2); LCDWriteData((s_structLCDDev.height - 1) >> 8); 
      LCDWriteCMD(s_structLCDDev.setycmd + 3); LCDWriteData((s_structLCDDev.height - 1) & 0xFF);
    }
    else if(s_structLCDDev.id == 0x9806)
    {
      LCDWriteCMD(s_structLCDDev.setxcmd);
      LCDWriteData(0);  LCDWriteData(0); 
      LCDWriteData((s_structLCDDev.width - 1) >> 8); 
      LCDWriteData((s_structLCDDev.width - 1) & 0xFF); 
      LCDWriteCMD(s_structLCDDev.setycmd);
      LCDWriteData(0);  LCDWriteData(0); 
      LCDWriteData((s_structLCDDev.height - 1) >> 8); 
      LCDWriteData((s_structLCDDev.height - 1) & 0xFF);
    }
  }
}

/*********************************************************************************************************
* 函数名称：LCDDrawPoint
* 函数功能：画点
* 输入参数：x,y:坐标
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDrawPoint(u16 x,u16 y)
{
  LCDSetCursor(x, y);            //设置光标位置 
  LCDSendWriteGramCMD();         //开始写入GRAM
  LCD->data = s_iLCDPointColor;  //写GRAM
}

/*********************************************************************************************************
* 函数名称：LCDDrawPoint
* 函数功能：快速画点
* 输入参数：x,y:坐标，color:颜色
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDFastDrawPoint(u16 x,u16 y,u16 color)
{
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd); LCDWriteData(x >> 8);
    LCDWriteCMD(s_structLCDDev.setxcmd + 1); LCDWriteData(x & 0xFF);
    LCDWriteCMD(s_structLCDDev.setycmd); LCDWriteData(y >> 8);
    LCDWriteCMD(s_structLCDDev.setycmd + 1); LCDWriteData(y & 0xFF);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd); 
    LCDWriteData(x >> 8); LCDWriteData(x & 0xFF);
    
    LCDWriteCMD(s_structLCDDev.setycmd); 
    LCDWriteData(y >> 8); LCDWriteData(y & 0xFF);
  }
  LCD->cmd = s_structLCDDev.wramcmd; 
  LCD->data = color; 
}

/*********************************************************************************************************
* 函数名称：LCDSSDBackLightSet
* 函数功能：SSD1963 背光设置
* 输入参数：pwm:背光等级,0~100.越大越亮
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDSSDBackLightSet(u8 pwm)
{	
  LCDWriteCMD(0xBE);        //配置PWM输出
  LCDWriteData(0x05);       //1设置PWM频率
  LCDWriteData(pwm * 2.55); //2设置PWM占空比
  LCDWriteData(0x01);       //3设置C
  LCDWriteData(0xFF);       //4设置D
  LCDWriteData(0x00);       //5设置E
  LCDWriteData(0x00);       //6设置F
}

/*********************************************************************************************************
* 函数名称：LCDDisplayDir
* 函数功能：设置LCD显示方向
* 输入参数：dir:0,竖屏；1,横屏
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDisplayDir(u8 dir)
{
  //竖屏
  if(dir == 0)
  {
    s_structLCDDev.dir = 0; //竖屏
    if(s_structLCDDev.id == 0x5510)
    {
      s_structLCDDev.wramcmd = 0x2C00;
      s_structLCDDev.setxcmd = 0x2A00;
      s_structLCDDev.setycmd = 0x2B00; 
      s_structLCDDev.width   = 480;
      s_structLCDDev.height  = 800;
    }
    else if(s_structLCDDev.id == 0x9806)
    {
      s_structLCDDev.wramcmd = 0x2C;
      s_structLCDDev.setxcmd = 0x2A;
      s_structLCDDev.setycmd = 0x2B; 
      s_structLCDDev.width   = 480;
      s_structLCDDev.height  = 800;
    }
  }


  //横屏
  else
  {
    s_structLCDDev.dir = 1;	//横屏
    if(s_structLCDDev.id == 0x5510)
    {
      s_structLCDDev.wramcmd = 0x2C00;
      s_structLCDDev.setxcmd = 0x2A00;
      s_structLCDDev.setycmd = 0x2B00; 
      s_structLCDDev.width   = 800;
      s_structLCDDev.height  = 480;
    }
    else if(s_structLCDDev.id == 0x9806)
    {
      s_structLCDDev.wramcmd = 0x2C;
      s_structLCDDev.setxcmd = 0x2A;
      s_structLCDDev.setycmd = 0x2B; 
      s_structLCDDev.width   = 800;
      s_structLCDDev.height  = 480;
    }
  }
  
  LCDScanDir(DFT_SCAN_DIR);	//默认扫描方向
}

/*********************************************************************************************************
* 函数名称：LCDSetWindow
* 函数功能：设置窗口,并自动设置画点坐标到窗口左上角(sx,sy)
* 输入参数：sx,sy:窗口起始坐标(左上角)，width,height:窗口宽度和高度,必须大于0!!
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：窗体大小:width*height.
*********************************************************************************************************/
void LCDSetWindow(u16 sx,u16 sy,u16 width,u16 height)
{
  u16 twidth, theight;
  twidth = sx + width - 1;
  theight = sy + height - 1;
  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd); LCDWriteData(sx >> 8);  
    LCDWriteCMD(s_structLCDDev.setxcmd + 1); LCDWriteData(sx & 0xFF);	  
    LCDWriteCMD(s_structLCDDev.setxcmd + 2); LCDWriteData(twidth >> 8);   
    LCDWriteCMD(s_structLCDDev.setxcmd + 3); LCDWriteData(twidth & 0xFF);   
    LCDWriteCMD(s_structLCDDev.setycmd); LCDWriteData(sy >> 8);   
    LCDWriteCMD(s_structLCDDev.setycmd + 1); LCDWriteData(sy & 0xFF);  
    LCDWriteCMD(s_structLCDDev.setycmd + 2); LCDWriteData(theight >> 8);   
    LCDWriteCMD(s_structLCDDev.setycmd + 3); LCDWriteData(theight & 0xFF);  
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    LCDWriteCMD(s_structLCDDev.setxcmd); 
    LCDWriteData(sx >> 8);  
    LCDWriteData(sx & 0xFF);	  
    LCDWriteData(twidth >> 8);   
    LCDWriteData(twidth & 0xFF);   
    LCDWriteCMD(s_structLCDDev.setycmd);
    LCDWriteData(sy >> 8);   
    LCDWriteData(sy & 0xFF);  
    LCDWriteData(theight >> 8);   
    LCDWriteData(theight & 0xFF);  
  }
}

/*********************************************************************************************************
* 函数名称：InitLCD
* 函数功能：初始化lcd
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitLCD(void)
{
  //GPIOB时钟使能
  rcu_periph_clock_enable(RCU_GPIOB);

  //配置背光控制GPIO
  gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_MAX, GPIO_PIN_0);

  //延时50ms
  DelayNms(50);
  
  //校验是否为NT35510
  LCDWriteCMD(0xDA00);
  s_structLCDDev.id = LCDReadData();  //读回0x00
  LCDWriteCMD(0xDB00);
  s_structLCDDev.id = LCDReadData();  //读回0x80
  s_structLCDDev.id <<= 8;
  LCDWriteCMD(0xDC00);
  s_structLCDDev.id |= LCDReadData(); //读回0x00
  if(s_structLCDDev.id == 0x8000)
  {
    //NT35510读回的ID是8000H,为方便区分,强制设置为5510
    s_structLCDDev.id=0x5510;
  }
  else
  {
    //读回的ID不是8000H,校验是否为ILI9806
    LCDWriteCMD(0XD3);				   
    s_structLCDDev.id=LCDReadData();	//假读	
    s_structLCDDev.id=LCDReadData();	//读到0X00
    s_structLCDDev.id=LCDReadData();  //读取98								   
    s_structLCDDev.id<<=8;
    s_structLCDDev.id|=LCDReadData();//读取06	
  }
  
  printf("LCD ID:%x\r\n", s_structLCDDev.id); //打印LCD ID

  if(s_structLCDDev.id == 0x5510)
  {
    LCDWriteReg(0xF000, 0x55);
    LCDWriteReg(0xF001, 0xAA);
    LCDWriteReg(0xF002, 0x52);
    LCDWriteReg(0xF003, 0x08);
    LCDWriteReg(0xF004, 0x01);

    //AVDD Set AVDD 5.2V
    LCDWriteReg(0xB000, 0x0D);
    LCDWriteReg(0xB001, 0x0D);
    LCDWriteReg(0xB002, 0x0D);
    
    //AVDD ratio
    LCDWriteReg(0xB600, 0x34);
    LCDWriteReg(0xB601, 0x34);
    LCDWriteReg(0xB602, 0x34);

    //AVEE -5.2V
    LCDWriteReg(0xB100, 0x0D);
    LCDWriteReg(0xB101, 0x0D);
    LCDWriteReg(0xB102, 0x0D);

    //AVEE ratio
    LCDWriteReg(0xB700, 0x34);
    LCDWriteReg(0xB701, 0x34);
    LCDWriteReg(0xB702, 0x34);

    //VCL -2.5V
    LCDWriteReg(0xB200, 0x00);
    LCDWriteReg(0xB201, 0x00);
    LCDWriteReg(0xB202, 0x00);

    //VCL ratio
    LCDWriteReg(0xB800, 0x24);
    LCDWriteReg(0xB801, 0x24);
    LCDWriteReg(0xB802, 0x24);

    //VGH 15V (Free pump)
    LCDWriteReg(0xBF00, 0x01);
    LCDWriteReg(0xB300, 0x0F);
    LCDWriteReg(0xB301, 0x0F);
    LCDWriteReg(0xB302, 0x0F);

    //VGH ratio
    LCDWriteReg(0xB900, 0x34);
    LCDWriteReg(0xB901, 0x34);
    LCDWriteReg(0xB902, 0x34);

    //VGL_REG -10V
    LCDWriteReg(0xB500, 0x08);
    LCDWriteReg(0xB501, 0x08);
    LCDWriteReg(0xB502, 0x08);
    LCDWriteReg(0xC200, 0x03);

    //VGLX ratio
    LCDWriteReg(0xBA00, 0x24);
    LCDWriteReg(0xBA01, 0x24);
    LCDWriteReg(0xBA02, 0x24);

    //VGMP/VGSP 4.5V/0V
    LCDWriteReg(0xBC00, 0x00);
    LCDWriteReg(0xBC01, 0x78);
    LCDWriteReg(0xBC02, 0x00);

    //VGMN/VGSN -4.5V/0V
    LCDWriteReg(0xBD00, 0x00);
    LCDWriteReg(0xBD01, 0x78);
    LCDWriteReg(0xBD02, 0x00);

    //VCOM
    LCDWriteReg(0xBE00, 0x00);
    LCDWriteReg(0xBE01, 0x64);

    //Gamma Setting
    LCDWriteReg(0xD100, 0x00);
    LCDWriteReg(0xD101, 0x33);
    LCDWriteReg(0xD102, 0x00);
    LCDWriteReg(0xD103, 0x34);
    LCDWriteReg(0xD104, 0x00);
    LCDWriteReg(0xD105, 0x3A);
    LCDWriteReg(0xD106, 0x00);
    LCDWriteReg(0xD107, 0x4A);
    LCDWriteReg(0xD108, 0x00);
    LCDWriteReg(0xD109, 0x5C);
    LCDWriteReg(0xD10A, 0x00);
    LCDWriteReg(0xD10B, 0x81);
    LCDWriteReg(0xD10C, 0x00);
    LCDWriteReg(0xD10D, 0xA6);
    LCDWriteReg(0xD10E, 0x00);
    LCDWriteReg(0xD10F, 0xE5);
    LCDWriteReg(0xD110, 0x01);
    LCDWriteReg(0xD111, 0x13);
    LCDWriteReg(0xD112, 0x01);
    LCDWriteReg(0xD113, 0x54);
    LCDWriteReg(0xD114, 0x01);
    LCDWriteReg(0xD115, 0x82);
    LCDWriteReg(0xD116, 0x01);
    LCDWriteReg(0xD117, 0xCA);
    LCDWriteReg(0xD118, 0x02);
    LCDWriteReg(0xD119, 0x00);
    LCDWriteReg(0xD11A, 0x02);
    LCDWriteReg(0xD11B, 0x01);
    LCDWriteReg(0xD11C, 0x02);
    LCDWriteReg(0xD11D, 0x34);
    LCDWriteReg(0xD11E, 0x02);
    LCDWriteReg(0xD11F, 0x67);
    LCDWriteReg(0xD120, 0x02);
    LCDWriteReg(0xD121, 0x84);
    LCDWriteReg(0xD122, 0x02);
    LCDWriteReg(0xD123, 0xA4);
    LCDWriteReg(0xD124, 0x02);
    LCDWriteReg(0xD125, 0xB7);
    LCDWriteReg(0xD126, 0x02);
    LCDWriteReg(0xD127, 0xCF);
    LCDWriteReg(0xD128, 0x02);
    LCDWriteReg(0xD129, 0xDE);
    LCDWriteReg(0xD12A, 0x02);
    LCDWriteReg(0xD12B, 0xF2);
    LCDWriteReg(0xD12C, 0x02);
    LCDWriteReg(0xD12D, 0xFE);
    LCDWriteReg(0xD12E, 0x03);
    LCDWriteReg(0xD12F, 0x10);
    LCDWriteReg(0xD130, 0x03);
    LCDWriteReg(0xD131, 0x33);
    LCDWriteReg(0xD132, 0x03);
    LCDWriteReg(0xD133, 0x6D);
    LCDWriteReg(0xD200, 0x00);
    LCDWriteReg(0xD201, 0x33);
    LCDWriteReg(0xD202, 0x00);
    LCDWriteReg(0xD203, 0x34);
    LCDWriteReg(0xD204, 0x00);
    LCDWriteReg(0xD205, 0x3A);
    LCDWriteReg(0xD206, 0x00);
    LCDWriteReg(0xD207, 0x4A);
    LCDWriteReg(0xD208, 0x00);
    LCDWriteReg(0xD209, 0x5C);
    LCDWriteReg(0xD20A, 0x00);

    LCDWriteReg(0xD20B, 0x81);
    LCDWriteReg(0xD20C, 0x00);
    LCDWriteReg(0xD20D, 0xA6);
    LCDWriteReg(0xD20E, 0x00);
    LCDWriteReg(0xD20F, 0xE5);
    LCDWriteReg(0xD210, 0x01);
    LCDWriteReg(0xD211, 0x13);
    LCDWriteReg(0xD212, 0x01);
    LCDWriteReg(0xD213, 0x54);
    LCDWriteReg(0xD214, 0x01);
    LCDWriteReg(0xD215, 0x82);
    LCDWriteReg(0xD216, 0x01);
    LCDWriteReg(0xD217, 0xCA);
    LCDWriteReg(0xD218, 0x02);
    LCDWriteReg(0xD219, 0x00);
    LCDWriteReg(0xD21A, 0x02);
    LCDWriteReg(0xD21B, 0x01);
    LCDWriteReg(0xD21C, 0x02);
    LCDWriteReg(0xD21D, 0x34);
    LCDWriteReg(0xD21E, 0x02);
    LCDWriteReg(0xD21F, 0x67);
    LCDWriteReg(0xD220, 0x02);
    LCDWriteReg(0xD221, 0x84);
    LCDWriteReg(0xD222, 0x02);
    LCDWriteReg(0xD223, 0xA4);
    LCDWriteReg(0xD224, 0x02);
    LCDWriteReg(0xD225, 0xB7);
    LCDWriteReg(0xD226, 0x02);
    LCDWriteReg(0xD227, 0xCF);
    LCDWriteReg(0xD228, 0x02);
    LCDWriteReg(0xD229, 0xDE);
    LCDWriteReg(0xD22A, 0x02);
    LCDWriteReg(0xD22B, 0xF2);
    LCDWriteReg(0xD22C, 0x02);
    LCDWriteReg(0xD22D, 0xFE);
    LCDWriteReg(0xD22E, 0x03);
    LCDWriteReg(0xD22F, 0x10);
    LCDWriteReg(0xD230, 0x03);
    LCDWriteReg(0xD231, 0x33);
    LCDWriteReg(0xD232, 0x03);
    LCDWriteReg(0xD233, 0x6D);
    LCDWriteReg(0xD300, 0x00);
    LCDWriteReg(0xD301, 0x33);
    LCDWriteReg(0xD302, 0x00);
    LCDWriteReg(0xD303, 0x34);
    LCDWriteReg(0xD304, 0x00);
    LCDWriteReg(0xD305, 0x3A);
    LCDWriteReg(0xD306, 0x00);
    LCDWriteReg(0xD307, 0x4A);
    LCDWriteReg(0xD308, 0x00);
    LCDWriteReg(0xD309, 0x5C);
    LCDWriteReg(0xD30A, 0x00);

    LCDWriteReg(0xD30B, 0x81);
    LCDWriteReg(0xD30C, 0x00);
    LCDWriteReg(0xD30D, 0xA6);
    LCDWriteReg(0xD30E, 0x00);
    LCDWriteReg(0xD30F, 0xE5);
    LCDWriteReg(0xD310, 0x01);
    LCDWriteReg(0xD311, 0x13);
    LCDWriteReg(0xD312, 0x01);
    LCDWriteReg(0xD313, 0x54);
    LCDWriteReg(0xD314, 0x01);
    LCDWriteReg(0xD315, 0x82);
    LCDWriteReg(0xD316, 0x01);
    LCDWriteReg(0xD317, 0xCA);
    LCDWriteReg(0xD318, 0x02);
    LCDWriteReg(0xD319, 0x00);
    LCDWriteReg(0xD31A, 0x02);
    LCDWriteReg(0xD31B, 0x01);
    LCDWriteReg(0xD31C, 0x02);
    LCDWriteReg(0xD31D, 0x34);
    LCDWriteReg(0xD31E, 0x02);
    LCDWriteReg(0xD31F, 0x67);
    LCDWriteReg(0xD320, 0x02);
    LCDWriteReg(0xD321, 0x84);
    LCDWriteReg(0xD322, 0x02);
    LCDWriteReg(0xD323, 0xA4);
    LCDWriteReg(0xD324, 0x02);
    LCDWriteReg(0xD325, 0xB7);
    LCDWriteReg(0xD326, 0x02);
    LCDWriteReg(0xD327, 0xCF);
    LCDWriteReg(0xD328, 0x02);
    LCDWriteReg(0xD329, 0xDE);
    LCDWriteReg(0xD32A, 0x02);
    LCDWriteReg(0xD32B, 0xF2);
    LCDWriteReg(0xD32C, 0x02);
    LCDWriteReg(0xD32D, 0xFE);
    LCDWriteReg(0xD32E, 0x03);
    LCDWriteReg(0xD32F, 0x10);
    LCDWriteReg(0xD330, 0x03);
    LCDWriteReg(0xD331, 0x33);
    LCDWriteReg(0xD332, 0x03);
    LCDWriteReg(0xD333, 0x6D);
    LCDWriteReg(0xD400, 0x00);
    LCDWriteReg(0xD401, 0x33);
    LCDWriteReg(0xD402, 0x00);
    LCDWriteReg(0xD403, 0x34);
    LCDWriteReg(0xD404, 0x00);
    LCDWriteReg(0xD405, 0x3A);
    LCDWriteReg(0xD406, 0x00);
    LCDWriteReg(0xD407, 0x4A);
    LCDWriteReg(0xD408, 0x00);
    LCDWriteReg(0xD409, 0x5C);
    LCDWriteReg(0xD40A, 0x00);
    LCDWriteReg(0xD40B, 0x81);

    LCDWriteReg(0xD40C, 0x00);
    LCDWriteReg(0xD40D, 0xA6);
    LCDWriteReg(0xD40E, 0x00);
    LCDWriteReg(0xD40F, 0xE5);
    LCDWriteReg(0xD410, 0x01);
    LCDWriteReg(0xD411, 0x13);
    LCDWriteReg(0xD412, 0x01);
    LCDWriteReg(0xD413, 0x54);
    LCDWriteReg(0xD414, 0x01);
    LCDWriteReg(0xD415, 0x82);
    LCDWriteReg(0xD416, 0x01);
    LCDWriteReg(0xD417, 0xCA);
    LCDWriteReg(0xD418, 0x02);
    LCDWriteReg(0xD419, 0x00);
    LCDWriteReg(0xD41A, 0x02);
    LCDWriteReg(0xD41B, 0x01);
    LCDWriteReg(0xD41C, 0x02);
    LCDWriteReg(0xD41D, 0x34);
    LCDWriteReg(0xD41E, 0x02);
    LCDWriteReg(0xD41F, 0x67);
    LCDWriteReg(0xD420, 0x02);
    LCDWriteReg(0xD421, 0x84);
    LCDWriteReg(0xD422, 0x02);
    LCDWriteReg(0xD423, 0xA4);
    LCDWriteReg(0xD424, 0x02);
    LCDWriteReg(0xD425, 0xB7);
    LCDWriteReg(0xD426, 0x02);
    LCDWriteReg(0xD427, 0xCF);
    LCDWriteReg(0xD428, 0x02);
    LCDWriteReg(0xD429, 0xDE);
    LCDWriteReg(0xD42A, 0x02);
    LCDWriteReg(0xD42B, 0xF2);
    LCDWriteReg(0xD42C, 0x02);
    LCDWriteReg(0xD42D, 0xFE);
    LCDWriteReg(0xD42E, 0x03);
    LCDWriteReg(0xD42F, 0x10);
    LCDWriteReg(0xD430, 0x03);
    LCDWriteReg(0xD431, 0x33);
    LCDWriteReg(0xD432, 0x03);
    LCDWriteReg(0xD433, 0x6D);
    LCDWriteReg(0xD500, 0x00);
    LCDWriteReg(0xD501, 0x33);
    LCDWriteReg(0xD502, 0x00);
    LCDWriteReg(0xD503, 0x34);
    LCDWriteReg(0xD504, 0x00);
    LCDWriteReg(0xD505, 0x3A);
    LCDWriteReg(0xD506, 0x00);
    LCDWriteReg(0xD507, 0x4A);
    LCDWriteReg(0xD508, 0x00);
    LCDWriteReg(0xD509, 0x5C);
    LCDWriteReg(0xD50A, 0x00);
    LCDWriteReg(0xD50B, 0x81);

    LCDWriteReg(0xD50C, 0x00);
    LCDWriteReg(0xD50D, 0xA6);
    LCDWriteReg(0xD50E, 0x00);
    LCDWriteReg(0xD50F, 0xE5);
    LCDWriteReg(0xD510, 0x01);
    LCDWriteReg(0xD511, 0x13);
    LCDWriteReg(0xD512, 0x01);
    LCDWriteReg(0xD513, 0x54);
    LCDWriteReg(0xD514, 0x01);
    LCDWriteReg(0xD515, 0x82);
    LCDWriteReg(0xD516, 0x01);
    LCDWriteReg(0xD517, 0xCA);
    LCDWriteReg(0xD518, 0x02);
    LCDWriteReg(0xD519, 0x00);
    LCDWriteReg(0xD51A, 0x02);
    LCDWriteReg(0xD51B, 0x01);
    LCDWriteReg(0xD51C, 0x02);
    LCDWriteReg(0xD51D, 0x34);
    LCDWriteReg(0xD51E, 0x02);
    LCDWriteReg(0xD51F, 0x67);
    LCDWriteReg(0xD520, 0x02);
    LCDWriteReg(0xD521, 0x84);
    LCDWriteReg(0xD522, 0x02);
    LCDWriteReg(0xD523, 0xA4);
    LCDWriteReg(0xD524, 0x02);
    LCDWriteReg(0xD525, 0xB7);
    LCDWriteReg(0xD526, 0x02);
    LCDWriteReg(0xD527, 0xCF);
    LCDWriteReg(0xD528, 0x02);
    LCDWriteReg(0xD529, 0xDE);
    LCDWriteReg(0xD52A, 0x02);
    LCDWriteReg(0xD52B, 0xF2);
    LCDWriteReg(0xD52C, 0x02);
    LCDWriteReg(0xD52D, 0xFE);
    LCDWriteReg(0xD52E, 0x03);
    LCDWriteReg(0xD52F, 0x10);
    LCDWriteReg(0xD530, 0x03);
    LCDWriteReg(0xD531, 0x33);
    LCDWriteReg(0xD532, 0x03);
    LCDWriteReg(0xD533, 0x6D);
    LCDWriteReg(0xD600, 0x00);
    LCDWriteReg(0xD601, 0x33);
    LCDWriteReg(0xD602, 0x00);
    LCDWriteReg(0xD603, 0x34);
    LCDWriteReg(0xD604, 0x00);
    LCDWriteReg(0xD605, 0x3A);
    LCDWriteReg(0xD606, 0x00);
    LCDWriteReg(0xD607, 0x4A);
    LCDWriteReg(0xD608, 0x00);
    LCDWriteReg(0xD609, 0x5C);
    LCDWriteReg(0xD60A, 0x00);
    LCDWriteReg(0xD60B, 0x81);

    LCDWriteReg(0xD60C, 0x00);
    LCDWriteReg(0xD60D, 0xA6);
    LCDWriteReg(0xD60E, 0x00);
    LCDWriteReg(0xD60F, 0xE5);
    LCDWriteReg(0xD610, 0x01);
    LCDWriteReg(0xD611, 0x13);
    LCDWriteReg(0xD612, 0x01);
    LCDWriteReg(0xD613, 0x54);
    LCDWriteReg(0xD614, 0x01);
    LCDWriteReg(0xD615, 0x82);
    LCDWriteReg(0xD616, 0x01);
    LCDWriteReg(0xD617, 0xCA);
    LCDWriteReg(0xD618, 0x02);
    LCDWriteReg(0xD619, 0x00);
    LCDWriteReg(0xD61A, 0x02);
    LCDWriteReg(0xD61B, 0x01);
    LCDWriteReg(0xD61C, 0x02);
    LCDWriteReg(0xD61D, 0x34);
    LCDWriteReg(0xD61E, 0x02);
    LCDWriteReg(0xD61F, 0x67);
    LCDWriteReg(0xD620, 0x02);
    LCDWriteReg(0xD621, 0x84);
    LCDWriteReg(0xD622, 0x02);
    LCDWriteReg(0xD623, 0xA4);
    LCDWriteReg(0xD624, 0x02);
    LCDWriteReg(0xD625, 0xB7);
    LCDWriteReg(0xD626, 0x02);
    LCDWriteReg(0xD627, 0xCF);
    LCDWriteReg(0xD628, 0x02);
    LCDWriteReg(0xD629, 0xDE);
    LCDWriteReg(0xD62A, 0x02);
    LCDWriteReg(0xD62B, 0xF2);
    LCDWriteReg(0xD62C, 0x02);
    LCDWriteReg(0xD62D, 0xFE);
    LCDWriteReg(0xD62E, 0x03);
    LCDWriteReg(0xD62F, 0x10);
    LCDWriteReg(0xD630, 0x03);
    LCDWriteReg(0xD631, 0x33);
    LCDWriteReg(0xD632, 0x03);
    LCDWriteReg(0xD633, 0x6D);

    //LV2 Page 0 enable
    LCDWriteReg(0xF000, 0x55);
    LCDWriteReg(0xF001, 0xAA);
    LCDWriteReg(0xF002, 0x52);
    LCDWriteReg(0xF003, 0x08);
    LCDWriteReg(0xF004, 0x00);

    //Display control
    LCDWriteReg(0xB100,  0xCC);
    LCDWriteReg(0xB101,  0x00);

    //Source hold time
    LCDWriteReg(0xB600, 0x05);

    //Gate EQ control
    LCDWriteReg(0xB700, 0x70);
    LCDWriteReg(0xB701, 0x70);

    //Source EQ control (Mode 2)
    LCDWriteReg(0xB800, 0x01);
    LCDWriteReg(0xB801, 0x03);
    LCDWriteReg(0xB802, 0x03);
    LCDWriteReg(0xB803, 0x03);

    //Inversion mode (2-dot)
    LCDWriteReg(0xBC00, 0x02);
    LCDWriteReg(0xBC01, 0x00);
    LCDWriteReg(0xBC02, 0x00);

    //Timing control 4H w/ 4-delay
    LCDWriteReg(0xC900, 0xD0);
    LCDWriteReg(0xC901, 0x02);
    LCDWriteReg(0xC902, 0x50);
    LCDWriteReg(0xC903, 0x50);
    LCDWriteReg(0xC904, 0x50);
    LCDWriteReg(0x3500, 0x00);
    LCDWriteReg(0x3A00, 0x55);  //16-bit/pixel
    LCDWriteCMD(0x1100);
    DelayNus(120);
    LCDWriteCMD(0x2900);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    //EXTC Command Set enable register
    LCDWriteCMD(0xFF); 
    LCDWriteData(0xFF);
    LCDWriteData(0x98);
    LCDWriteData(0x06);
    
    //SPI Interface Setting
    //LCDWriteCMD(0xBA);
    //LCDWriteData(0x60);
    
    //GIP 1
    LCDWriteCMD(0xBC); 
    LCDWriteData(0x01);
    LCDWriteData(0x0F);
    LCDWriteData(0x61);
    LCDWriteData(0xFF);
    LCDWriteData(0x01);
    LCDWriteData(0x01);
    LCDWriteData(0x0B);
    LCDWriteData(0x10);
    LCDWriteData(0x37);
    LCDWriteData(0x63);
    LCDWriteData(0xFF);
    LCDWriteData(0xFF);
    LCDWriteData(0x01);
    LCDWriteData(0x01);
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0xFF);
    LCDWriteData(0x52);
    LCDWriteData(0x01);
    LCDWriteData(0x00);
    LCDWriteData(0x40);
    
    //GIP 2
    LCDWriteCMD(0xBD); 
    LCDWriteData(0x01);
    LCDWriteData(0x23);
    LCDWriteData(0x45);
    LCDWriteData(0x67);
    LCDWriteData(0x01);
    LCDWriteData(0x23);
    LCDWriteData(0x45);
    LCDWriteData(0x67);
    
    //GIP 3
    LCDWriteCMD(0xBE); 
    LCDWriteData(0x00);
    LCDWriteData(0x01);
    LCDWriteData(0xAB);
    LCDWriteData(0x60);
    LCDWriteData(0x22);
    LCDWriteData(0x22);
    LCDWriteData(0x22);
    LCDWriteData(0x22);
    LCDWriteData(0x22);
    
    //VCOM Control
    LCDWriteCMD(0xC7); 
    LCDWriteData(0x36);
    
    //EN_volt_reg	 VGMP / VGMN /VGSP / VGSN voltage to output
    LCDWriteCMD(0xED); 
    LCDWriteData(0x7F);
    LCDWriteData(0x0F);
    
    //Power Control 1	Setting AVDD / AVEE / VGH / VGL
    LCDWriteCMD(0XC0); 
    LCDWriteData(0x0F);
    LCDWriteData(0x0B);
    LCDWriteData(0x0A);//VGH 15V,VGLO-10V
    
    //AVDD / AVEE generated by internal pumping
    LCDWriteCMD(0XFC);  
    LCDWriteData(0x08);
    
    LCDWriteCMD(0XDF); 
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x20);
    
    //DVDD Voltage Setting 
    LCDWriteCMD(0XF3);
    LCDWriteData(0x74);
    
    //Inversion Type
    LCDWriteCMD(0xB4); 
    LCDWriteData(0x00);//02
    LCDWriteData(0x00);//02
    LCDWriteData(0x00);//02
		
    //Panel Control
	  //LCDWriteCMD(0xB9); 
    //LCDWriteData(0x00);//0
    
    //esolution Control
    LCDWriteCMD(0xF7); 
    LCDWriteData(0x82);//480*800
    
    //FRAME RATE Setting
    LCDWriteCMD(0xB1); 
    LCDWriteData(0x00);
    LCDWriteData(0x13);
    LCDWriteData(0x13); 
    
    //CR_EQ_PC_SDT  #C0,06,40,28
    LCDWriteCMD(0XF2); 
    LCDWriteData(0x80);
    LCDWriteData(0x04);
    LCDWriteData(0x40);
    LCDWriteData(0x28);
    
    //Power Control 2	 SD OP Bias_VRH1_VRH2_EXT_CPCK_SEL
    LCDWriteCMD(0XC1); 
    LCDWriteData(0x17);
    LCDWriteData(0x88);//VGMP
    LCDWriteData(0x88);//VGMN
    LCDWriteData(0x20);
    
    //Positive Gamma Control
    LCDWriteCMD(0xE0);   
    LCDWriteData(0x00); //P1
    LCDWriteData(0x0A); //P2
    LCDWriteData(0x12); //P3
    LCDWriteData(0x10); //P4
    LCDWriteData(0x0E); //P5
    LCDWriteData(0x20); //P6
    LCDWriteData(0xCC); //P7
    LCDWriteData(0x07); //P8
    LCDWriteData(0x06); //P9
    LCDWriteData(0x0B); //P10
    LCDWriteData(0x0E); //P11
    LCDWriteData(0x0F); //P12
    LCDWriteData(0x0D); //P13
    LCDWriteData(0x15); //P14
    LCDWriteData(0x10); //P15
    LCDWriteData(0x00); //P16
    
    //Negative Gamma Correction
    LCDWriteCMD(0xE1); 
    LCDWriteData(0x00); //P1
    LCDWriteData(0x0B); //P2
    LCDWriteData(0x13); //P3
    LCDWriteData(0x0D); //P4
    LCDWriteData(0x0E); //P5
    LCDWriteData(0x1B); //P6
    LCDWriteData(0x71); //P7
    LCDWriteData(0x06); //P8
    LCDWriteData(0x06); //P9
    LCDWriteData(0x0A); //P10
    LCDWriteData(0x0F); //P11
    LCDWriteData(0x0E); //P12
    LCDWriteData(0x0F); //P13
    LCDWriteData(0x15); //P14
    LCDWriteData(0x0C); //P15
    LCDWriteData(0x00); //P16
    
    LCDWriteCMD(0x2a);   
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x01);
    LCDWriteData(0xdf);
    
    LCDWriteCMD(0x2b);   
    LCDWriteData(0x00);
    LCDWriteData(0x00);
    LCDWriteData(0x03);
    LCDWriteData(0x1f);
    
    //Pixel Format
    LCDWriteCMD(0x3A); 
    LCDWriteData(0x55);

    //Memory Access Control
    LCDWriteCMD(0x36); 
    LCDWriteData(0x00); //02-180

    LCDWriteCMD(0x11);
    DelayNms(120);   
    LCDWriteCMD(0x29);  
    DelayNms(20);  
    LCDWriteCMD(0x2C);
  }

  LCDDisplayDir(0); //默认为竖屏
  LCD_LED_HIGH;     //点亮背光
  LCDClear(WHITE);  //清屏
}

/*********************************************************************************************************
* 函数名称：LCDClear
* 函数功能：清屏函数
* 输入参数：color:要清屏的填充色
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDClear(u16 color)
{
  u32 index = 0;
  u32 totalpoint = s_structLCDDev.width;
  totalpoint *= s_structLCDDev.height;   //得到总点数
  
  if(s_structLCDDev.id == 0x5510)
  {
    //设置光标位置
    LCDSetCursor(0x00, 0x0000);
  }
  else if(s_structLCDDev.id == 0x9806)
  {
    s_structLCDDev.setxcmd = 0X2A;
    s_structLCDDev.setycmd = 0X2B;
    
    //设置光标位置
    LCDSetCursor(0x00, 0x00);
    
    s_structLCDDev.wramcmd = 0X2C;
  }

  //开始写入GRAM
  LCDSendWriteGramCMD();
  for(index = 0; index < totalpoint; index++)
  {
    LCD->data = color;
  }
}

/*********************************************************************************************************
* 函数名称：LCDFill
* 函数功能：在指定区域内填充单个颜色
* 输入参数：(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)，color:要填充的颜色
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDFill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color)
{
  u16 i,j;
  u16 xlen = 0;

  xlen = ex - sx + 1;
  for(i=sy;i<=ey;i++)
  {
    LCDSetCursor(sx, i);      //设置光标位置
    LCDSendWriteGramCMD();    //开始写入GRAM
    for(j = 0; j < xlen; j++)
    {
      LCD->data = color;  //显示颜色
    }
  }
}

/*********************************************************************************************************
* 函数名称：LCDColorFill
* 函数功能：在指定区域内填充指定颜色块
* 输入参数：(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)，color:要填充的颜色
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDColorFill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color)
{  
  u16 height, width;
  u16 i, j;
  width = ex - sx + 1;   //得到填充的宽度
  height = ey - sy + 1;  //高度
  for(i = 0; i < height; i++)
  {
    LCDSetCursor(sx, sy + i);  //设置光标位置 
    LCDSendWriteGramCMD();     //开始写入GRAM

    //写入数据
    for(j = 0; j < width; j++)
    {
      LCD->data = color[i * width + j];
    }
  }
}

/*********************************************************************************************************
* 函数名称：LCDDrawLine
* 函数功能：画线
* 输入参数：x1,y1:起点坐标，x2,y2:终点坐标
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
  u16 t; 
  int xerr = 0, yerr = 0, delta_x, delta_y, distance; 
  int incx, incy, uRow, uCol; 
  delta_x = x2 - x1; //计算坐标增量 
  delta_y = y2 - y1; 
  uRow = x1;         //uRow 和 uCol 是当前的实时画笔坐标，从起点开始追踪。
  uCol = y1; 

  //设置单步方向 
  if(delta_x > 0)
  {
    incx=1;
  }

  //垂直线
  else if(delta_x==0)
  {
    incx=0;
  }
  else 
  {
    incx = -1;
    delta_x = -delta_x;
  } 

  if(delta_y > 0)
  {
    incy = 1;
  }

  //水平线
  else if(delta_y == 0)
  {
    incy = 0;
  }
  else
  {
    incy = -1;
    delta_y = -delta_y;
  } 

  //选取基本增量坐标轴
  if( delta_x>delta_y)
  {
    distance = delta_x;
  }
  else 
  {
    distance = delta_y;
  }
	
	xerr = distance / 2;
	yerr = distance /2;
	
  //画线输出
	
  for(t = 0; t <= distance ; t++ )
  {  
		LCDDrawPoint(uRow, uCol);//画点 
    xerr += delta_x ; 
    yerr += delta_y ; 
    if(xerr >= distance) 
    { 
      xerr -= distance; 
      uRow += incx; 
    } 
    if(yerr >= distance) 
    { 
      yerr -= distance; 
      uCol += incy; 
    }
  }
}

/*********************************************************************************************************
* 函数名称；LCDdashedLine
* 函数功能：画虚线
* 输入参数：(x1,y1),(x2,y2):虚线的起点到终点
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：画纯实线：传入 0xFFFF，画常规虚线：传入 0xF0F0，画稀疏虚线：传入 0xC000，画点划线（Dash-Dot）：传入 0xF888
*********************************************************************************************************/
void LCDdashedLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 style)
{
  u16 t; 
  int xerr = 0, yerr = 0, delta_x, delta_y, distance; 
  int incx, incy, uRow, uCol; 
  delta_x = x2 - x1; //计算坐标增量 
  delta_y = y2 - y1; 
  uRow = x1;         //uRow 和 uCol 是当前的实时画笔坐标，从起点开始追踪。
  uCol = y1; 

  //设置单步方向 
  if(delta_x > 0)
  {
    incx=1;
  }

  //垂直线
  else if(delta_x==0)
  {
    incx=0;
  }
  else 
  {
    incx = -1;
    delta_x = -delta_x;
  } 

  if(delta_y > 0)
  {
    incy = 1;
  }

  //水平线
  else if(delta_y == 0)
  {
    incy = 0;
  }
  else
  {
    incy = -1;
    delta_y = -delta_y;
  } 

  //选取基本增量坐标轴
  if( delta_x>delta_y)
  {
    distance = delta_x;
  }
  else 
  {
    distance = delta_y;
  }
	
	xerr = distance / 2;
	yerr = distance /2;
	
  //画线输出
	
  for(t = 0; t <= distance ; t++ )
  {  
		if(style & 0x8000)
		{
    LCDDrawPoint(uRow, uCol);//画点 
		}
		style = (style << 1) | (style >> 15);
		
		
    xerr += delta_x ; 
    yerr += delta_y ; 
    if(xerr >= distance) 
    { 
      xerr -= distance; 
      uRow += incx; 
    } 
    if(yerr >= distance) 
    { 
      yerr -= distance; 
      uCol += incy; 
    }
  }
}


/*********************************************************************************************************
* 函数名称：LCDDrawRectangle
* 函数功能：画矩形
* 输入参数：(x1,y1),(x2,y2):矩形的对角坐标
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
  LCDDrawLine(x1,y1,x2,y1);
  LCDDrawLine(x1,y1,x1,y2);
  LCDDrawLine(x1,y2,x2,y2);
  LCDDrawLine(x2,y1,x2,y2);
}

/*********************************************************************************************************
* 函数名称：LCDDrawCircle
* 函数功能：在指定位置画一个指定大小的圆
* 输入参数：(x,y):中心点，r:半径
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDDrawCircle(u16 x0, u16 y0, u8 r)
{
  int a, b;
  int di;
  a = 0; 
  b = r;	  
  di = 3 - (r << 1);                   //判断下个点位置的标志
  while(a <= b)
  {
    LCDDrawPoint(x0 + a, y0 - b);           //5
    LCDDrawPoint(x0 + b, y0 - a);           //0
    LCDDrawPoint(x0 + b, y0 + a);           //4
    LCDDrawPoint(x0 + a, y0 + b);           //6 
    LCDDrawPoint(x0 - a, y0 + b);           //1
    LCDDrawPoint(x0 - b, y0 + a);             
    LCDDrawPoint(x0 - a, y0 - b);           //2
    LCDDrawPoint(x0 - b, y0 - a);           //7
    a++;
    //使用Bresenham算法画圆
    if(di < 0)
    {
      di += 4 * a + 6;
    }
    else
    {
      di += 10 + 4 * (a - b);
      b--;
    }
  }
}

/*********************************************************************************************************
* 函数名称：LCDShowChar
* 函数功能：在指定位置显示一个字符
* 输入参数：x,y:起始坐标，num:要显示的字符:" "--->"~"，size:字体大小 12/16/24，mode:叠加方式(1)还是非叠加方式(0)
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode)
{
  u8 temp, t1, t;
  u16 y0 = y;
  u8 csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);  //得到字体一个字符对应点阵集所占的字节数
  num = num - ' '; //得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）

  for(t = 0; t < csize; t++)
  {
    //调用1206字体
    if(size == 12)
    {
      temp = asc2_1206[num][t];
    }

    //调用1608字体
    else if(size == 16)
    {
      temp = asc2_1608[num][t];
    }

    //调用2412字体
    else if(size == 24)
    {
      temp = asc2_2412[num][t];
    }

    //没有的字库
    else 
    {
      return;
    }

    for(t1 = 0; t1 < 8; t1++)
    {
      if(temp & 0x80)
      {
        LCDFastDrawPoint(x, y, s_iLCDPointColor);
      }
      else if(mode == 0)
      {
        LCDFastDrawPoint(x, y, s_iLCDBackColor);
      }

      temp <<= 1;
      y++;

      //超区域了
      if(y >= s_structLCDDev.height)
      {
        return;
      }
      if((y - y0) == size)
      {
        y = y0;
        x++;

        //超区域了
        if(x >= s_structLCDDev.width)
        {
          return;
        }
        break;
      }
    }
  }
}

/*********************************************************************************************************
* 函数名称：LCDPow
* 函数功能：m^n函数
* 输入参数：m^n
* 输出参数：void
* 返 回 值：m^n次方.
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
u32 LCDPow(u8 m, u8 n)
{
  u32 result = 1;
  while(n--)
  {
    result *= m;
  }
  return result;
}

/*********************************************************************************************************
* 函数名称：LCDShowNum
* 函数功能：显示数字,高位为0,则不显示
* 输入参数：x,y :起点坐标，len :数字的位数，num:数值(0~4294967295)，size:字体大小
* 输出参数：void
* 返 回 值：m^n次方.
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDShowNum(u16 x, u16 y, u32 num, u8 len, u8 size)
{
  u8 t, temp;
  u8 enshow = 0;
  for(t = 0; t < len; t++)
  {
    temp = (num / LCDPow(10, len - t - 1)) % 10;
    if(enshow == 0 && t < (len - 1))
    {
      if(temp == 0)
      {
        LCDShowChar(x + (size / 2) * t, y, ' ', size, 0);
        continue;
      }
      else 
      {
        enshow=1;
      }
    }
    LCDShowChar(x + (size / 2) * t, y, temp + '0', size, 0);
  }
}

/*********************************************************************************************************
* 函数名称：LCDShowxNum
* 函数功能：显示数字,高位为0,还是显示
* 输入参数：x,y :起点坐标，num:数值(0~999999999)，len:数字的位数，size:字体大小
*          mode:[7]:0,不填充;1,填充0，[6:1]:保留，[0]:0,非叠加显示;1,叠加显示
* 输出参数：void
* 返 回 值：m^n次方.
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode)
{
  u8 t, temp;
  u8 enshow = 0;
  for(t = 0; t < len; t++)
  {
    temp = (num / LCDPow(10 ,len - t - 1)) % 10;
    if(enshow == 0 && t < (len - 1))
    {
      if(temp == 0)
      {
        if(mode & 0x80)
        {
          LCDShowChar(x + (size / 2) * t, y, '0', size, mode & 0x01);
        }
        else 
        {
          LCDShowChar(x + (size / 2) * t, y, ' ', size, mode & 0x01);
        }
        continue;
      }
      else 
      {
        enshow = 1; 
      }
    }
    LCDShowChar(x + (size / 2) * t, y, temp + '0', size, mode & 0x01);
  }
} 

/*********************************************************************************************************
* 函数名称：LCDShowString
* 函数功能：显示字符串
* 输入参数：x,y:起点坐标，width,height:区域大小，size:字体大小，*p:字符串起始地址
* 输出参数：void
* 返 回 值：m^n次方.
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void LCDShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p)
{
  u8 x0 = x;
  width += x;
  height += y;
  while((*p <= '~') && (*p >= ' '))//判断是不是非法字符!
  {
    if(x >= width)
    {
      x = x0;
      y += size;
    }
    if(y >= height)
    {
      break;//退出
    }
    LCDShowChar(x, y, *p, size, 0);
    x += size / 2;
    p++;
  }
}
