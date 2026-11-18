/*********************************************************************************************************
* 模块名称：DM9000.h
* 摘    要：DM9000驱动模块
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
#ifndef _DM9000_H_
#define _DM9000_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "DataType.h"
#include "lwip/pbuf.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
#define DM9000_RST_HIGH gpio_bit_set(GPIOG, GPIO_PIN_7)       //DM9000复位引脚
#define DM9000_RST_LOW  gpio_bit_reset(GPIOG, GPIO_PIN_7)
#define DM9000_INT      gpio_input_bit_get(GPIOG, GPIO_PIN_6) //DM9000中断引脚

//DM9000地址结构体
typedef struct
{
  volatile u16 reg;
  volatile u16 data;
}StructDM9000Base;

//使用NOR/SRAM的 Bank0.sector2,地址位HADDR[27,26]=01 A7作为数据命令区分线 
//注意设置时STM32内部会右移一位对其!
#define DM9000_BASE        ((u32)(0x68000000 | 0x000000FE))
#define DM9000             ((StructDM9000Base *) DM9000_BASE)

#define DM9000_ID          0x90000A46  //DM9000 ID
#define DM9000_PKT_MAX     1536        //DM9000最大接收包长度

#define DM9000_PHY         0x40        //DM9000 PHY寄存器访问标志

//DM9000寄存器
#define DM9000_NCR         0x00
#define DM9000_NSR         0x01
#define DM9000_TCR         0x02
#define DM9000_TSRI        0x03
#define DM9000_TSRII       0x04
#define DM9000_RCR         0x05
#define DM9000_RSR         0x06
#define DM9000_ROCR        0x07
#define DM9000_BPTR        0x08
#define DM9000_FCTR        0x09
#define DM9000_FCR         0x0A
#define DM9000_EPCR        0x0B
#define DM9000_EPAR        0x0C
#define DM9000_EPDRL       0x0D
#define DM9000_EPDRH       0x0E
#define DM9000_WCR         0x0F
#define DM9000_PAR         0x10        //物理地址0x10~0x15
#define DM9000_MAR         0x16        //多播地址0x16~0x1D
#define DM9000_GPCR        0x1E
#define DM9000_GPR         0x1F
#define DM9000_TRPAL       0x22
#define DM9000_TRPAH       0x23
#define DM9000_RWPAL       0x24
#define DM9000_RWPAH       0x25

#define DM9000_VIDL        0x28
#define DM9000_VIDH        0x29
#define DM9000_PIDL        0x2A
#define DM9000_PIDH        0x2B

#define DM9000_CHIPR       0x2C
#define DM9000_TCR2        0x2D
#define DM9000_OCR         0x2E
#define DM9000_SMCR        0x2F
#define DM9000_ETXCSR      0x30
#define DM9000_TCSCR       0x31
#define DM9000_RCSCSR      0x32
#define DM9000_MRCMDX      0xF0
#define DM9000_MRCMDX1     0xF1
#define DM9000_MRCMD       0xF2
#define DM9000_MRRL        0xF4
#define DM9000_MRRH        0xF5
#define DM9000_MWCMDX      0xF6
#define DM9000_MWCMD       0xF8
#define DM9000_MWRL        0xFA
#define DM9000_MWRH        0xFB
#define DM9000_TXPLL       0xFC
#define DM9000_TXPLH       0xFD
#define DM9000_ISR         0xFE
#define DM9000_IMR         0xFF

#define NCR_RST            0x01
#define NSR_SPEED          0x80
#define NSR_LINKST         0x40
#define NSR_WAKEST         0x20
#define NSR_TX2END         0x08
#define NSR_TX1END         0x04
#define NSR_RXOV           0x02

#define RCR_DIS_LONG       0x20
#define RCR_DIS_CRC        0x10
#define RCR_ALL            0x08
#define RCR_RXEN           0x01

#define IMR_PAR            0x80    
#define IMR_ROOI           0x08    
#define IMR_POI            0x04        //使能接收溢出中断
#define IMR_PTI            0x02        //使能发送中断
#define IMR_PRI            0x01        //使能接收中断

#define ISR_LNKCHGS        (1 << 5)
#define ISR_ROOS           (1 << 3)
#define ISR_ROS            (1 << 2)
#define ISR_PTS            (1 << 1)
#define ISR_PRS            (1 << 0)
#define ISR_CLR_STATUS     (ISR_ROOS | ISR_ROS | ISR_PTS | ISR_PRS)

//DM9000内部PHY寄存器
#define DM9000_PHY_BMCR    0x00
#define DM9000_PHY_BMSR    0x01
#define DM9000_PHY_PHYID1  0x02
#define DM9000_PHY_PHYID2  0x03
#define DM9000_PHY_ANAR    0x04
#define DM9000_PHY_ANLPAR  0x05
#define DM9000_PHY_ANER    0x06
#define DM9000_PHY_DSCR    0x10
#define DM9000_PHY_DSCSR   0x11
#define DM9000_PHY_10BTCSR 0x12
#define DM9000_PHY_PWDOR   0x13
#define DM9000_PHY_SCR     0x14

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/
//DM9000工作模式定义
typedef enum 
{
  DM9000_10MHD  = 0, //10M半双工
  DM9000_100MHD = 1, //100M半双工
  DM9000_10MFD  = 4, //10M全双工
  DM9000_100MFD = 5, //100M全双工
  DM9000_AUTO   = 8, //自动协商
}EnumDM9000PhyMode;


//DM9000配置结构体
typedef struct
{
  EnumDM9000PhyMode mode; //工作模式
  u8  imrAll;             //中断类型 
  u16 queuePacketLen;     //每个数据包大小
  u8  macAddr[6];         //MAC地址
  u8  multicaseAddr[8];   //组播地址
}StructDM9000Config;

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/
u8                  InitDM9000(void);                 //初始化DM9000驱动模块
void                DM9000SendPacket(struct pbuf *p); //通过DM9000发送数据包
struct pbuf*        DM9000ReceivePacket(void);        //DM9000接收数据包
StructDM9000Config* GetDM9000Dev(void);               //获取DM9000配置结构体首地址

#endif
