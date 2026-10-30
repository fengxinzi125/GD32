# GT1151Q
GT1151Q是一款专为5至6英寸触控屏设计的新一代10点电容触控芯片。该芯片提供16个驱动通道和29个感应通道，支持手套操作及全屏多笔自定义手势唤醒功能。它内置了1.8V LDO，并采用Flash工艺制程，支持在线烧录固件。该芯片具备Normal（正常）、Green（休眠扫描）、Gesture（手势）和Sleep（深度睡眠）四种工作模式以平衡性能与功耗。     

## 硬件接口与通信协议
GT1151Q通过标准的I2C接口与主控进行通信，最高支持400Kbps的通信速率。它的7位I2C从设备地址为**0x14**，对应的8位写操作地址为**0x28**，读操作地址为0x29。除了电源和地线，GT1151Q还需要连接主控的SCL、SDA（I2C通信）、INT（中断信号）和RESET（复位信号）引脚。当有触控事件发生时，INT引脚会输出下降沿脉冲信号通知主控

参考“GD32F30x用户手册（英文版）.pdf”，GD32微控制器集成了I2C外设（如I2C0、I2C1）和外部中断/事件控制器（EXTI）。   
- I2C引脚：将GD32的I2C_SCL和I2C_SDA引脚配置为复用开漏输出（Alternate function open-drain），并连接至GT1151Q的对应引脚。若使用高于200Kbps的速率，需确保外部上拉电阻阻值合适以保证边沿陡峭。   
- INT引脚：将连接INT的GD32 GPIO配置为输入模式，并开启内部上拉或外部上拉。在EXTI模块中，将该引脚对应的中断线配置为下降沿触发，以响应GT1151Q的触控中断。   
- RESET引脚：将连接RESET的GD32 GPIO配置为推挽输出模式（Push-pull output），用于控制触控芯片的硬件复位。   

# iic协议
半双工，双线制，SCL，SDA
- 配置 挂在总线上面的引脚配置为开漏输出，配置为开漏是因为可以实现“线与”功能，遇到任何一个设备主动拉低电平，总线拉低。要所有设备拉低总线，总线才能被上拉电阻拉高。
## 数据有效性
- 在SCL为高电平期间，SDA的数据必须保持稳定，SDA的电平状态只能在SCL为低的时候才能发生改变
- 起始条件 SCL为高电平的时候，SDA由高电平向低电平变化，标志着一次信号的开始
- 结束 SCL为高电平的时候，SDA由低电平向高电平变化，标志一次信号的结束
SCL: ─────┐         ┌───┐   ┌───┐         ┌──────
          │         │   │   │   │         │
SDA: ──┐  └─────────┘   └───┘   └─────────┘  ┌───
       │                                     │
       └─────────────────────────────────────┘
     START (S)      Data Change        STOP (P)
(SCL高时SDA下降)  (SCL低时SDA变化)  (SCL高时SDA上升)

## 主机向从机写数据的时序图
1. 主机发起起始条件
2. 主机发送从机地址外加一位读写控制位 0为写，1为读
3. 从机匹配地址，拉低SDA应答响应
4. 主机发送8位寄存器地址或者命令代码
5. 从机再次应答
6. 主机发送八位数据
7. 从机应答
8. 主机发送停止信号，即SCL高电平的时候，SDA由低转高，从机释放总线


## 关于如何在LCD屏幕上面实现按键功能检测
```c
//首先定义结构体
typedef struct{
	u16 x0, y0;         //起始点
	u16 width, height;  //向外衍生
	u16 colorNormal;    //正常颜色
	u16 colorPress;     //按下颜色
	u8 state;           //按键状态 0 为正常，1 为按下
	} StructButton;

 //个性化你的按键 
 static StructButton s_arrButtons[2] = {
 {100, 150, 120, 60, WHITE, GREEN, 0}, 
 {300, 150, 120, 60, BLUE, GREEN, 0}, 

//手指是否在按键内部判断函数
static u8 IsPointInButton(u16 x, u16 y, StructButton* btn)
{
	return (x >= btn->x0 && x <= btn->x0 + btn->width &&
            y >= btn->y0 && y <= btn->y0 + btn->height);
}

//手指扫描函数
for(j = 0; j < 2; j++)                                //遍历两个个性化键 |

{ 
 u8 press = 0; 

 for(k = 0; k < 5; k++)                              //这是检查五个槽位否有手指 
{ 
if(s_pTouchDev->pointFlag[k] == 1) |
{ 
if(IsPointInButton(s_pTouchDev->point[k].x, s_pTouchDev->point[k].y,&s_arrButtons[j])) 
{ 
press = 1; 
break; 
} 
} 
} 
 s_arrButtons[j].state = press; 
} 
DrawButtons();                   //更新按键状态 

//应用层
static void DrawButtons(void)
{
    u8 j;
	  u16 color;
    for(j = 0; j < 2; j++)        /* ← 改成 2，和数组大小一致 */
  {
    if(1 == s_arrButtons[j].state)
    {
      color = s_arrButtons[j].colorPress;
    }
    else
    {
      color = s_arrButtons[j].colorNormal;
    }

    LCDFill(s_arrButtons[j].x0,
            s_arrButtons[j].y0,
            s_arrButtons[j].x0 + s_arrButtons[j].width,
            s_arrButtons[j].y0 + s_arrButtons[j].height,
            color);              
  }
}
```

