/*********************************************************************************************************
* 模块名称：LCD.h
* 摘    要：MCU显示屏模块
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
#ifndef _LCD_H_
#define _LCD_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "DataType.h"
#include "stdlib.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
//-----------------LCD端口定义---------------- 
#define LCD_LED_HIGH gpio_bit_set(GPIOB, GPIO_PIN_0) //LCD背光  PB0 
#define LCD_LED_LOW  gpio_bit_reset(GPIOB, GPIO_PIN_0)

//LCD地址结构体
typedef struct
{
  volatile u16 cmd;  //读写命令
  volatile u16 data; //读写数据
}StructLCDBase;

//使用NOR/PSRAM的 Bank0 Region1,地址位HADDR[27,26]=01 A0作为数据命令区分线 
//注意设置时STM32内部会右移一位对其!
#define LCD_BASE  ((u32)(0x60000000 | 0x04000000))
#define LCD       ((StructLCDBase *)LCD_BASE)

//扫描方向定义
#define L2R_U2D  0 //从左到右,从上到下
#define L2R_D2U  1 //从左到右,从下到上
#define R2L_U2D  2 //从右到左,从上到下
#define R2L_D2U  3 //从右到左,从下到上

#define U2D_L2R  4 //从上到下,从左到右
#define U2D_R2L  5 //从上到下,从右到左
#define D2U_L2R  6 //从下到上,从左到右
#define D2U_R2L  7 //从下到上,从右到左

#define DFT_SCAN_DIR  L2R_U2D  //默认的扫描方向

//画笔颜色
#define WHITE                 0xFFFF //白色
#define BLACK                 0x0000 //黑色
#define BLUE                  0x001F //蓝色
#define BRED                  0xF81F //红色
#define GRED                  0xFFE0 //红色
#define GBLUE                 0x07FF //蓝色
#define RED                   0xF800 //红色
#define MAGENTA               0xF81F //品红
#define GREEN                 0x07E0 //绿色
#define CYAN                  0x7FFF //青色
#define YELLOW                0xFFE0 //黄色
#define BROWN                 0xBC40 //棕色
#define BRRED                 0xFC07 //棕红色
#define GRAY                  0x8430 //灰色

//GUI颜色
#define DARKBLUE              0x01CF //深蓝色
#define LIGHTBLUE             0x7D7C //浅蓝色
#define GRAYBLUE              0x5458 //灰蓝色

//以上三色为PANEL的颜色 
#define LIGHTGREEN            0x841F //浅绿色
#define LIGHTGRAY             0xEF5B //浅灰色(PANNEL)
#define LGRAY                 0xC618 //浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE             0xA651 //浅灰蓝色(中间层颜色)
#define LBBLUE                0x2B12 //浅棕蓝色(选择条目的反色)

//LCD分辨率设置
#define SSD_HOR_RESOLUTION    800    //LCD水平分辨率
#define SSD_VER_RESOLUTION    480    //LCD垂直分辨率

//LCD驱动参数设置
#define SSD_HOR_PULSE_WIDTH   1      //水平脉宽
#define SSD_HOR_BACK_PORCH    46     //水平前廊
#define SSD_HOR_FRONT_PORCH   210    //水平后廊
#define SSD_VER_PULSE_WIDTH   1      //垂直脉宽
#define SSD_VER_BACK_PORCH    23     //垂直前廊
#define SSD_VER_FRONT_PORCH   22     //垂直前廊

//如下几个参数，自动计算
#define SSD_HT    (SSD_HOR_RESOLUTION + SSD_HOR_BACK_PORCH + SSD_HOR_FRONT_PORCH)
#define SSD_HPS   (SSD_HOR_BACK_PORCH)
#define SSD_VT    (SSD_VER_RESOLUTION + SSD_VER_BACK_PORCH + SSD_VER_FRONT_PORCH)
#define SSD_VPS   (SSD_VER_BACK_PORCH)

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/
//LCD重要参数集
typedef struct  
{
  u16 width;   //LCD 宽度
  u16 height;  //LCD 高度
  u16 id;      //LCD ID
  u8  dir;     //横屏还是竖屏控制：0，竖屏；1，横屏。
  u16 wramcmd; //开始写gram指令
  u16 setxcmd; //设置x坐标指令
  u16 setycmd; //设置y坐标指令
}StructLCDDev;

//LCD参数
extern StructLCDDev s_structLCDDev;  //管理LCD重要参数

//LCD的画笔颜色和背景色
extern u16  s_iLCDPointColor; //默认红色
extern u16  s_iLCDBackColor;  //背景颜色.默认为白色

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/
void LCDWriteCMD(u16 cmd);                                          //向LCD写命令
void LCDWriteData(u16 data);                                        //向LCD写数据
u16  LCDReadData(void);                                             //从LCD读数据
void LCDWriteReg(u16 reg,u16 value);                                //写寄存器
u16  LCDReadReg(u16 reg);                                           //读寄存器
void LCDSendWriteGramCMD(void);                                     //发送开始写GRAM命令
void LCDWriteRAM(u16 rgb);                                          //写GRAM
u16  LCDBGRToRGB(u16 bgr);                                          //BGR转RGB
u16  LCDReadPoint(u16 x,u16 y);                                     //读点
void LCDDisplayOn(void);                                            //开显示
void LCDDisplayOff(void);                                           //关显示
void LCDSetCursor(u16 x, u16 y);                                    //设置光标
void LCDScanDir(u8 dir);                                            //设置屏扫描方向
void LCDDrawPoint(u16 x,u16 y);                                     //画点
void LCDFastDrawPoint(u16 x,u16 y,u16 color);                       //快速画点
void LCDSSDBackLightSet(u8 pwm);                                    //SSD1963 背光控制
void LCDDisplayDir(u8 dir);                                         //设置屏幕显示方向
void LCDSetWindow(u16 sx,u16 sy,u16 width,u16 height);              //设置窗口
void InitLCD(void);                                                 //初始化
void LCDClear(u16 Color);                                           //清屏
void LCDFill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color);                //填充单色
void LCDColorFill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color);          //填充指定颜色
void LCDDrawLine(u16 x1, u16 y1, u16 x2, u16 y2);                   //画线
void LCDDrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);              //画矩形
void LCDDrawCircle(u16 x0,u16 y0,u8 r);                             //画圆
void LCDShowChar(u16 x,u16 y,u8 num,u8 size,u8 mode);               //显示一个字符
u32  LCDPow(u8 m, u8 n);                                            //m^n函数
void LCDShowNum(u16 x,u16 y,u32 num,u8 len,u8 size);                //显示一个数字
void LCDShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode);       //显示 数字
void LCDShowString(u16 x,u16 y,u16 width,u16 height,u8 size,u8 *p); //显示一个字符串,12/16字体

#endif
