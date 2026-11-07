#ifndef LOCK_TEST_GPIO_H
#define LOCK_TEST_GPIO_H
typedef unsigned char u8;
typedef unsigned int u32;
#define RCU_GPIOA 1
#define RCU_GPIOG 2
#define GPIOA 1
#define GPIOG 2
#define GPIO_PIN_0 1
#define GPIO_PIN_13 8192
#define GPIO_PIN_14 16384
#define GPIO_MODE_IPD 1
#define GPIO_MODE_IPU 2
#define GPIO_OSPEED_50MHZ 50
#define SET 1
#define RESET 0
void rcu_periph_clock_enable(u32 clock);
void gpio_init(u32 port, u32 mode, u32 speed, u32 pin);
u8 gpio_input_bit_get(u32 port, u32 pin);
#endif
