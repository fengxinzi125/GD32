/*********************************************************************************************************
* 模块名称：NT35510.c
* 摘    要：NT35510显示屏模块
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
#include "NT35510.h"
#include "LCD.h"
#include "SysTick.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量
*********************************************************************************************************/

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
* 函数名称：InitNT35510
* 函数功能：初始化NT35510显示屏模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void InitNT35510(void)
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

/*********************************************************************************************************
* 函数名称：NT35510DisplayOn
* 函数功能：NT35510显示屏开启显示
* 输入参数：void 
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void NT35510DisplayOn(void)
{
  LCDWriteCMD(0x2900);
}

/*********************************************************************************************************
* 函数名称：NT35510DisplayOff
* 函数功能：NT35510显示屏关闭显示
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void NT35510DisplayOff(void)
{
  LCDWriteCMD(0x2800);
}

/*********************************************************************************************************
* 函数名称：NT35510SetCursor
* 函数功能：NT35510显示屏设置光标位置
* 输入参数：x：横坐标，y：纵坐标
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void NT35510SetCursor(u16 x, u16 y)
{
  LCDWriteCMD(s_structLCDDev.setxcmd); LCDWriteData(x >> 8);
  LCDWriteCMD(s_structLCDDev.setxcmd + 1); LCDWriteData(x & 0xFF);
  LCDWriteCMD(s_structLCDDev.setycmd); LCDWriteData(y >> 8);
  LCDWriteCMD(s_structLCDDev.setycmd + 1); LCDWriteData(y & 0xFF);
}















