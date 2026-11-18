/*********************************************************************************************************
* 模块名称：DM9000.c
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

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "DM9000.h"
#include "gd32f30x_conf.h"
#include "SysTick.h"
#include "stdio.h"

/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/

/*********************************************************************************************************
*                                              枚举结构体
*********************************************************************************************************/

/*********************************************************************************************************
*                                              内部变量定义
*********************************************************************************************************/
static StructDM9000Config s_structDM9000Cfg;     //DM9000配置结构体

/*********************************************************************************************************
*                                              内部函数声明
*********************************************************************************************************/
static u16  ReadReg(u16 reg);                    //读取DM9000指定寄存器的值
static void WriteReg(u16 reg, u16 data);         //向DM9000指定寄存器中写入指定值
static u16  DM9000ReadPhyReg(u16 reg);           //读取DM9000的PHY的指定寄存器
static void DM9000WritePhyReg(u16 reg,u16 data); //读取DM9000的PHY的指定寄存器
static void Reset(void);                         //复位DM9000
static u32  GetDeiviceID(void);                  //获取DM9000的芯片ID
static u8   GetSpeedAndDuplex(void);             //获取网速
static void SetPHYMode(u8 mode);                 //设置DM900的PHY工作模式
static void SetMACAddress(u8 *macaddr);          //设置DM9000的MAC地址
static void SetMulticast(u8 *multicastaddr);     //设置DM9000的组播地址
static void ISRHandler(void);                    //中断处理函数

/*********************************************************************************************************
*                                              内部函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：ReadReg
* 函数功能：读取DM9000指定寄存器的值
* 输入参数：reg:寄存器地址
* 输出参数：void
* 返 回 值：DM9000指定寄存器的值
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static u16 ReadReg(u16 reg)
{
  DM9000->reg = reg;
  return DM9000->data; 
}

/*********************************************************************************************************
* 函数名称：WriteReg
* 函数功能：向DM9000指定寄存器中写入指定值
* 输入参数：reg:要写入的寄存器，data:要写入的值
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void WriteReg(u16 reg, u16 data)
{
  DM9000->reg = reg;
  DM9000->data = data;
}

/*********************************************************************************************************
* 函数名称：DM9000ReadPhyReg
* 函数功能：读取DM9000的PHY的指定寄存器
* 输入参数：reg:要读的PHY寄存器
* 输出参数：void
* 返 回 值：读取到的PHY寄存器值
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static u16 DM9000ReadPhyReg(u16 reg)
{
  u16 temp;
  WriteReg(DM9000_EPAR, DM9000_PHY | reg);
  WriteReg(DM9000_EPCR, 0x0C);  //选中PHY，发送读命令
  DelayNms(10);
  WriteReg(DM9000_EPCR,0x00);   //清除读命令
  temp = (ReadReg(DM9000_EPDRH) << 8) | (ReadReg(DM9000_EPDRL));
  return temp;
}

/*********************************************************************************************************
* 函数名称：DM9000WritePhyReg
* 函数功能：向DM9000的PHY寄存器写入指定值
* 输入参数：reg:PHY寄存器，data:要写入的值
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void DM9000WritePhyReg(u16 reg,u16 data)
{
  WriteReg(DM9000_EPAR,DM9000_PHY|reg);
  WriteReg(DM9000_EPDRL,(data & 0xFF));        //写入低字节
  WriteReg(DM9000_EPDRH,((data >> 8) & 0xFF)); //写入高字节
  WriteReg(DM9000_EPCR,0x0A);                  //选中PHY,发送写命令
  DelayNms(50);                                //延时50ms
  WriteReg(DM9000_EPCR,0x00);                  //清除写命令
}

/*********************************************************************************************************
* 函数名称：Reset
* 函数功能：复位DM9000
* 输入参数：reg:要写入的寄存器，data:要写入的值
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void Reset(void)
{
  //复位DM9000,复位步骤参考<DM9000 Application Notes V1.22>手册29页
  DM9000_RST_LOW;   //DM9000硬件复位
  DelayNms(10);     //延时10ms
  DM9000_RST_HIGH;  //DM9000硬件复位结束
  DelayNms(100);    //一定要有这个延时，让DM9000准备就绪！

  //第一次软复位
  WriteReg(DM9000_GPCR, 0x01);            //第一步:设置GPCR寄存器(0x1E)的bit0为1 
  WriteReg(DM9000_GPR, 0);                //第二步:设置GPR寄存器(0x1F)的bit1为0，DM9000内部的PHY上电
  WriteReg(DM9000_NCR, (0x02 | NCR_RST)); //第三步:软件复位DM9000 
  
  //等待DM9000软复位完成
  do
  {
    DelayNms(25);
  }while(ReadReg(DM9000_NCR) & 1);


  //DM9000第二次软复位
  WriteReg(DM9000_NCR,0);
  WriteReg(DM9000_NCR,(0x02|NCR_RST));
  do 
  {
    DelayNms(25);
  }while(ReadReg(DM9000_NCR) & 1);
}

/*********************************************************************************************************
* 函数名称：GetDeiviceID
* 函数功能：获取DM9000的芯片ID
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static u32 GetDeiviceID(void)
{
  u32 value;
  value  = ReadReg(DM9000_VIDL);
  value |= ReadReg(DM9000_VIDH) << 8;
  value |= ReadReg(DM9000_PIDL) << 16;
  value |= ReadReg(DM9000_PIDH) << 24;
  return value;
}

/*********************************************************************************************************
* 函数名称：GetSpeedAndDuplex
* 函数功能：获取网速
* 输入参数：void
* 输出参数：void
* 返 回 值：
*         0,100M半双工
*         1,100M全双工
*         2,10M半双工
*         3,10M全双工
*         0xFF,连接失败！
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static u8 GetSpeedAndDuplex(void)
{
  u8 temp;
  u8 i = 0;

  //如果开启了自动协商模式一定要等待协商完成
  if(s_structDM9000Cfg.mode == DM9000_AUTO) 
  {
    //等待自动协商完成
    while(!(DM9000ReadPhyReg(0x01) & 0x0020))
    {
      DelayNms(100);
      i++;

      //自动协商失败
      if(i > 100)
      {
        return 0xFF;
      }
    }
  }
  
  //自定义模式,一定要等待连接成功
  else
  {
    //等待连接成功
    while(!(ReadReg(DM9000_NSR) & 0x40))
    {
      DelayNms(100);
      i++;

      //连接失败
      if(i > 100)
      {
        return 0xFF;
      }
    }
  }

  temp  = ((ReadReg(DM9000_NSR) >> 6) & 0x02); //获取DM9000的连接速度
  temp |= ((ReadReg(DM9000_NCR) >> 3) & 0x01); //获取DM9000的双工状态

  return temp;
}

/*********************************************************************************************************
* 函数名称：SetPHYMode
* 函数功能：设置DM900的PHY工作模式
* 输入参数：mode:PHY模式
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void SetPHYMode(u8 mode)
{
  u16 BMCR_Value,ANAR_Value;
  switch(mode)
  {
    //10M半双工
    case DM9000_10MHD:
      BMCR_Value = 0x0000;
      ANAR_Value = 0x21;
      break;

    //10M全双工
    case DM9000_10MFD:
      BMCR_Value = 0x0100;
      ANAR_Value = 0x41;
      break;

    //100M半双工
    case DM9000_100MHD:
      BMCR_Value = 0x2000;
      ANAR_Value = 0x81;
      break;

    //100M全双工
    case DM9000_100MFD:
      BMCR_Value = 0x2100;
      ANAR_Value = 0x101;
      break;

    //自动协商模式
    case DM9000_AUTO:
      BMCR_Value = 0x1000;
      ANAR_Value = 0x01E1;
      break;
  }
  DM9000WritePhyReg(DM9000_PHY_BMCR, BMCR_Value);
  DM9000WritePhyReg(DM9000_PHY_ANAR, ANAR_Value);
  WriteReg(DM9000_GPR, 0x00); //使能PHY
}

/*********************************************************************************************************
* 函数名称：SetMACAddress
* 函数功能：设置DM9000的MAC地址
* 输入参数：macaddr:指向MAC地址
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void SetMACAddress(u8 *macaddr)
{
  u8 i;
  for(i = 0; i < 6; i++)
  {
    WriteReg(DM9000_PAR + i, macaddr[i]);
  }
}

/*********************************************************************************************************
* 函数名称：SetMulticast
* 函数功能：设置DM9000的组播地址
* 输入参数：multicastaddr:指向多播地址
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void SetMulticast(u8 *multicastaddr)
{
  u8 i;
  for(i = 0; i < 8; i++)
  {
    WriteReg(DM9000_MAR + i, multicastaddr[i]);
  }
}

/*********************************************************************************************************
* 函数名称：ISRHandler
* 函数功能：中断处理函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void ISRHandler(void)
{
  u16 int_status;
  u16 last_io;
  last_io = DM9000->reg;
  int_status = ReadReg(DM9000_ISR); 
  WriteReg(DM9000_ISR,int_status); //清除中断标志位，DM9000的ISR寄存器的bit0~bit5写1清零
  if(int_status & ISR_ROS)
  {
    printf("overflow \r\n");
  }
  if(int_status & ISR_ROOS)
  {
    printf("overflow counter overflow \r\n");
  }
  if(int_status & ISR_PRS) //接收中断
  {
    //接收完成中断，用户自行添加所需代码
  }
  if(int_status&ISR_PTS) //发送中断
  { 
    //发送完成中断，用户自行添加所需代码
  }
  DM9000->reg = last_io;
}

/*********************************************************************************************************
* 函数名称：EXTI5_9_IRQHandler
* 函数功能：外部中断线6的中断服务函数
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void EXTI5_9_IRQHandler(void)
{
  //EXTI_ClearITPendingBit(EXTI_Line6); //清除中断线6挂起标志位
  while(DM9000_INT == 0)
  {
    ISRHandler();
  }
}

/*********************************************************************************************************
*                                              API函数实现
*********************************************************************************************************/
/*********************************************************************************************************
* 函数名称：InitDM9000
* 函数功能：初始化DM9000驱动模块
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
u8 InitDM9000(void)
{
  exmc_norsram_parameter_struct        sram_init_struct;
  exmc_norsram_timing_parameter_struct sram_timing_init_struct;
  u32 temp;

  //使能时钟
  rcu_periph_clock_enable(RCU_EXMC);
  rcu_periph_clock_enable(RCU_GPIOD);
  rcu_periph_clock_enable(RCU_GPIOE);
  rcu_periph_clock_enable(RCU_GPIOF);
  rcu_periph_clock_enable(RCU_GPIOG);

  /* GPIOD configuration */
  /* D0(PD14),D1(PD15),D2(PD0),D3(PD1),NOE(PD4),NWE(PD5) pin configuration */
  gpio_init(GPIOD, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_14 | GPIO_PIN_15 |
                                                       GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7);

  //A7
  gpio_init(GPIOF, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,  GPIO_PIN_13);

  /* GPIOE configuration */
  /* D4(PE7),D5(PE8),D6(PE9),D7(PE10) -  D12(PE15)pin configuration */
  gpio_init(GPIOE, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
                                                       GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
                                                       GPIO_PIN_15);

  //D13(PD8) - D15(PD10)
  gpio_init(GPIOD, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10); 

  //ne2
  gpio_init(GPIOG, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

  //RST
  gpio_init(GPIOG, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

  //INT
  gpio_init(GPIOG, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

  nvic_irq_enable(EXTI5_9_IRQn, 2U, 0U);  
  gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOG, GPIO_PIN_SOURCE_6);
  exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
  exti_interrupt_flag_clear(EXTI_6);

  //EXMC配置
  sram_timing_init_struct.asyn_access_mode = EXMC_ACCESS_MODE_A; //模式A，异步访问SRAM
  sram_timing_init_struct.asyn_address_setuptime = 0; //异步访问地址建立时间
  sram_timing_init_struct.asyn_address_holdtime  = 0; //异步访问地址保持时间
  sram_timing_init_struct.asyn_data_setuptime    = 3; //异步访问数据建立时间
  sram_timing_init_struct.bus_latency            = 0; //同步/异步访问总线延时时间
  sram_timing_init_struct.syn_clk_division       = 0; //同步访问时钟分频系数（从HCLK中分频）
  sram_timing_init_struct.syn_data_latency       = 0; //同步访问中获得第1个数据所需要的等待延迟

  sram_init_struct.norsram_region    = EXMC_BANK0_NORSRAM_REGION2; //Region2
  sram_init_struct.address_data_mux  = DISABLE;                    //禁用地址、数据总线多路复用
  sram_init_struct.memory_type       = EXMC_MEMORY_TYPE_SRAM;      //储存器类型为SRAM
  sram_init_struct.databus_width     = EXMC_NOR_DATABUS_WIDTH_16B; //数据宽度16位
  sram_init_struct.burst_mode        = DISABLE;                    //禁用突发访问
  sram_init_struct.nwait_config      = EXMC_NWAIT_CONFIG_BEFORE;   //等待输入配置
  sram_init_struct.nwait_polarity    = EXMC_NWAIT_POLARITY_LOW;    //等待输入信号低电平有效
  sram_init_struct.wrap_burst_mode   = DISABLE;                    //禁用包突发访问
  sram_init_struct.asyn_wait         = DISABLE;                    //禁用异步等待
  sram_init_struct.extended_mode     = DISABLE;                    //禁用扩展模式
  sram_init_struct.memory_write      = ENABLE;                     //使能写入外部存储器
  sram_init_struct.nwait_signal      = DISABLE;                    //禁用等待输入信号
  sram_init_struct.write_mode        = EXMC_ASYN_WRITE;            //写入模式为异步写入
  sram_init_struct.read_write_timing = &sram_timing_init_struct;   //读时序配置
  sram_init_struct.write_timing      = &sram_timing_init_struct;   //写时序配置
  exmc_norsram_init(&sram_init_struct);             //初始化Region2
  exmc_norsram_enable(EXMC_BANK0_NORSRAM_REGION2);  //使能Region2

  //DM9000的SRAM的发送和接收指针自动返回到开始地址，并且开启接收中断
  s_structDM9000Cfg.mode = DM9000_AUTO;
  s_structDM9000Cfg.queuePacketLen = 0;
  s_structDM9000Cfg.imrAll = IMR_PAR | IMR_PRI; 

  //初始化MAC地址
  temp = *(volatile u32*)(0x1FFFF7E8); //获唯一ID的前24位作为MAC地址后三字节
  s_structDM9000Cfg.macAddr[0] = 2;
  s_structDM9000Cfg.macAddr[1] = 0;
  s_structDM9000Cfg.macAddr[2] = 0;
  s_structDM9000Cfg.macAddr[3] = (temp >> 16) & 0xFF; 
  s_structDM9000Cfg.macAddr[4] = (temp >> 8) & 0xFFF;
  s_structDM9000Cfg.macAddr[5] = temp & 0xFF;

  //初始化组播地址
  s_structDM9000Cfg.multicaseAddr[0] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[1] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[2] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[3] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[4] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[5] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[6] = 0xFF;
  s_structDM9000Cfg.multicaseAddr[7] = 0xFF;

  //复位DM9000
  Reset();
  DelayNms(100);

  //获取DM9000ID
  temp = GetDeiviceID();
  printf("DM9000 ID:%#x\r\n",temp);
  if(temp!=DM9000_ID)
  {
    //读取ID错误
    return 1;
  }
  SetPHYMode(s_structDM9000Cfg.mode); //设置PHY工作模式

  WriteReg(DM9000_NCR, 0x00);
  WriteReg(DM9000_TCR, 0x00);  //发送控制寄存器清零
  WriteReg(DM9000_BPTR, 0x3F);
  WriteReg(DM9000_FCTR, 0x38);
  WriteReg(DM9000_FCR, 0x00);
  WriteReg(DM9000_SMCR, 0x00); //特殊模式
  WriteReg(DM9000_NSR, NSR_WAKEST | NSR_TX2END | NSR_TX1END); //清除发送状态
  WriteReg(DM9000_ISR, 0x0F);  //清除中断状态
  WriteReg(DM9000_TCR2, 0x80); //切换LED到mode1

  //设置MAC地址和组播地址
  SetMACAddress(s_structDM9000Cfg.macAddr);      //设置MAC地址
  SetMulticast(s_structDM9000Cfg.multicaseAddr); //设置组播地址
  WriteReg(DM9000_RCR, RCR_DIS_LONG | RCR_DIS_CRC | RCR_RXEN);
  WriteReg(DM9000_IMR, IMR_PAR);
  temp = GetSpeedAndDuplex(); //获取DM9000的连接速度和双工状态
  if(temp != 0xFF)            //连接成功，通过串口显示连接速度和双工状态
  {
    printf("DM9000 Speed:%dMbps,Duplex:%s duplex mode\r\n",(temp&0x02)?10:100,(temp&0x01)?"Full":"Half");
  }
  else 
  {
    printf("DM9000 Establish Link Failed!\r\n");
  }
  WriteReg(DM9000_IMR,s_structDM9000Cfg.imrAll); //设置中断
  return 0;
}

/*********************************************************************************************************
* 函数名称：DM9000SendPacket
* 函数功能：通过DM9000发送数据包
* 输入参数：p:pbuf结构体指针
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void DM9000SendPacket(struct pbuf *p)
{
  struct pbuf *q;
  u16 pbuf_index = 0;
  u8 word[2], word_index = 0;

  WriteReg(DM9000_IMR, IMR_PAR); //关闭网卡中断 
  DM9000->reg = DM9000_MWCMD;    //发送此命令后就可以将要发送的数据搬到DM9000 TX SRAM中
  q = p;

  //向DM9000的TX SRAM中写入数据，一次写入两个字节数据
  //当要发送的数据长度为奇数的时候，我们需要将最后一个字节单独写入DM9000的TX SRAM中
  while(q)
  {
    if (pbuf_index < q->len)
    {
      word[word_index++] = ((u8_t*)q->payload)[pbuf_index++];
      if (word_index == 2)
      {
        DM9000->data = ((u16)word[1] << 8) | word[0];
        word_index = 0;
      }
    }
    else
    {
      q = q->next;
      pbuf_index = 0;
    }
  }

  //还有一个字节未写入TX SRAM
  if(word_index == 1)
  {
    DM9000->data = word[0];
  }

  //向DM9000写入发送长度
  WriteReg(DM9000_TXPLL, p->tot_len & 0xFF);
  WriteReg(DM9000_TXPLH, (p->tot_len >> 8) & 0xFF);

  //启动发送
  WriteReg(DM9000_TCR, 0x01);

  //等待发送完成
  while((ReadReg(DM9000_ISR) & 0x02) == 0);

  //清除发送完成中断
  WriteReg(DM9000_ISR, 0x02);

  //DM9000网卡接收中断使能
  WriteReg(DM9000_IMR, s_structDM9000Cfg.imrAll);
}

/*********************************************************************************************************
* 函数名称：DM9000ReceivePacket
* 函数功能：DM9000接收数据包
* 输入参数：void
* 输出参数：void
* 返 回 值：pbuf格式的接收到的数据包
* 创建日期：2021年07月01日
* 注    意：
*         接收到的数据包存放在DM9000的RX FIFO中，地址为0x0C00~0x3FFF
*         接收到的数据包的前四个字节并不是真实的数据，而是有特定含义的
*         byte1:表明是否接收到数据，为0x00或者0x01，如果两个都不是的话一定要软件复位DM9000
*               0x01，接收到数据
*               0x00，未接收到数据
*         byte2:第二个字节表示一些状态信息，和DM9000的RSR(0x06)寄存器一致的
*         byte3:本帧数据长度的低字节
*         byte4:本帧数据长度的高字节
*********************************************************************************************************/
struct pbuf* DM9000ReceivePacket(void)
{
  struct pbuf* p;
  struct pbuf* q;
  u32 rxbyte;
  volatile u16 rx_status, rx_length;
  u16* data;
  u16  dummy;
  int  len;

  p = NULL;

__error_retry:

  ReadReg(DM9000_MRCMDX);       //假读
  rxbyte = (u8)DM9000->data;    //进行第二次读取
  if(rxbyte)                    //接收到数据
  {
    if(rxbyte > 1)              //rxbyte大于1，接收到的数据错误
    {
      printf("dm9000 rx: rx error, stop device\r\n");
      WriteReg(DM9000_RCR, 0x00);
      WriteReg(DM9000_ISR, 0x80);
      return (struct pbuf*)p;
    }
    DM9000->reg = DM9000_MRCMD;
    rx_status = DM9000->data;
    rx_length = DM9000->data;
    //if(rx_length>512)printf("rxlen:%d\r\n",rx_length);
    p = pbuf_alloc(PBUF_RAW, rx_length, PBUF_POOL); //pbufs内存池分配pbuf
    if(p != NULL)  //内存申请成功
    {
      for(q = p; q != NULL; q = q->next)
      {
        data = (u16*)q->payload;
        len = q->len;
        while(len > 0)
        {
          *data = DM9000->data;
          data++;
          len -= 2;
        }
      }
    }

    //内存申请失败
    else
    {
      printf("pbuf内存申请失败:%d\r\n", rx_length);
      data = &dummy;
      len = rx_length;
      while(len)
      {
        *data = DM9000->data;
        len -= 2;
      }
    }

    //根据rx_status判断接收数据是否出现如下错误：FIFO溢出、CRC错误
    //对齐错误、物理层错误，如果有任何一个出现的话丢弃该数据帧，
    //当rx_length小于64或者大于最大数据长度的时候也丢弃该数据帧
    if((rx_status & 0xBF00) || (rx_length < 0x40) || (rx_length > DM9000_PKT_MAX))
    {
      printf("rx_status:%#x\r\n",rx_status);
      if (rx_status & 0x100)
      {
        printf("rx fifo error\r\n");
      }
      if (rx_status & 0x200)
      {
        printf("rx crc error\r\n");
      }
      if (rx_status & 0x8000)
      {
        printf("rx length error\r\n");
      }
      if (rx_length>DM9000_PKT_MAX)
      {
        printf("rx length too big\r\n");
        WriteReg(DM9000_NCR, NCR_RST);  //复位DM9000
        DelayNms(5);
      }
      if(p != NULL)
      {
        pbuf_free((struct pbuf*)p); //释放内存
      }
      p=NULL;
      goto __error_retry;
    }
  }
  else
  {
    //清除所有中断标志位
    WriteReg(DM9000_ISR, ISR_PTS);

    //重新接收中断
    s_structDM9000Cfg.imrAll = IMR_PAR | IMR_PRI;
    WriteReg(DM9000_IMR, s_structDM9000Cfg.imrAll);
  }
  return (struct pbuf*)p; 
}
/*********************************************************************************************************
* 函数名称：GetDM9000Dev
* 函数功能：获取DM9000配置结构体首地址
* 输入参数：void
* 输出参数：void
* 返 回 值：DM9000配置结构体首地址
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
StructDM9000Config *GetDM9000Dev(void)
{
  return &s_structDM9000Cfg;
}
