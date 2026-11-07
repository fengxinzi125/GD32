/* GUI background backup experiment. KEY1=save, KEY2=test, KEY3=restore. */
#include "MallocTop.h"
#include "GUITop.h"
#include "GUIPlatform.h"
#include "Malloc.h"
#include "Timer.h"
#include "gd32f30x_conf.h"
#include <stdio.h>

#define BACKUP_X       288U
#define BACKUP_Y       48U
#define BACKUP_WIDTH   128U
#define BACKUP_HEIGHT  64U
#define BACKUP_BYTES   (BACKUP_WIDTH * BACKUP_HEIGHT * sizeof(u16))
#define KEY_DEBOUNCE_MS 30U

#if (GUI_COLOR_TYPE != GUI_COLOR_TYPE_RGB565)
#error This experiment requires the LCD RGB565 background format.
#endif

static StructGUIDev s_gui;
static u16 *s_background = NULL;
static u8 s_keyRaw[3];
static u8 s_keyStable[3];
static u64 s_keyChangedAt[3];

static void UpdateMemoryInfo(void)
{
  u32 used;
  used = MemoryUsedBytes(SRAMIN);
  s_gui.updateInSRAMInfo(used, (MEM1_MAX_SIZE) - used);
  used = MemoryUsedBytes(SRAMEX);
  s_gui.updateExSRAMInfo(used, (MEM2_MAX_SIZE) - used);
}

static void SaveBackground(void)
{
  /* Keep the original backup until it has been restored and freed. */
  if(s_background != NULL)
  {
    printf("KEY1: background already saved.\r\n");
    return;
  }
  s_background = (u16 *)MyMalloc(SRAMEX, BACKUP_BYTES);
  if(s_background == NULL)
  {
    UpdateMemoryInfo();
    printf("KEY1: external SRAM allocation failed.\r\n");
    return;
  }
  GUISaveBackground(BACKUP_X, BACKUP_Y, BACKUP_WIDTH, BACKUP_HEIGHT,
                    (u32)s_background);
  UpdateMemoryInfo();
  printf("KEY1: saved %lu bytes in external SRAM.\r\n",
         (unsigned long)BACKUP_BYTES);
}

static void ShowTest(void)
{
  /* Never overwrite the screen before there is a valid backup. */
  if(s_background == NULL)
  {
    printf("KEY2: press KEY1 to save the background first.\r\n");
    return;
  }
  GUIDrawTextLine(BACKUP_X + 40U, BACKUP_Y + 20U, (u32)"test",
                  GUI_FONT_ASCII_24, 0, GUI_COLOR_WHITE, 1, 4);
}

static void RestoreBackground(void)
{
  if(s_background != NULL)
  {
    /* Redraw BEFORE releasing the backing memory. */
    GUIDrawBackground(BACKUP_X, BACKUP_Y, BACKUP_WIDTH, BACKUP_HEIGHT,
                      (u32)s_background);
    MyFree(SRAMEX, s_background);
    s_background = NULL;
    printf("KEY3: background restored and SRAM released.\r\n");
  }
  UpdateMemoryInfo();
}

static u8 ReadKey(u8 key)
{
  if(key == 0U)
    return gpio_input_bit_get(GPIOA, GPIO_PIN_0) == SET;
  if(key == 1U)
    return gpio_input_bit_get(GPIOG, GPIO_PIN_13) == RESET;
  return gpio_input_bit_get(GPIOG, GPIO_PIN_14) == RESET;
}

static void InitKeys(void)
{
  u8 i;
  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOG);
  gpio_init(GPIOA, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
  gpio_init(GPIOG, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
            GPIO_PIN_13 | GPIO_PIN_14);
  for(i = 0; i < 3U; ++i)
  {
    /* A key held at power-on must be released before its first action. */
    s_keyRaw[i] = ReadKey(i);
    s_keyStable[i] = s_keyRaw[i];
    s_keyChangedAt[i] = GetSysTime();
  }
}

static void ScanKeys(void)
{
  u8 i, pressed, events = 0;
  u64 now = GetSysTime();
  for(i = 0; i < 3U; ++i)
  {
    pressed = ReadKey(i);
    if(pressed != s_keyRaw[i])
    {
      s_keyRaw[i] = pressed;
      s_keyChangedAt[i] = now;
    }
    else if((pressed != s_keyStable[i]) &&
            ((now - s_keyChangedAt[i]) >= KEY_DEBOUNCE_MS))
    {
      s_keyStable[i] = pressed;
      if(pressed)
        events |= (u8)(1U << i);
    }
  }
  /* Restore takes priority if multiple edges arrive in the same scan. */
  if(events & 4U) RestoreBackground();
  else if(events & 1U) SaveBackground();
  else if(events & 2U) ShowTest();
}

void InitMallocTop(void)
{
  s_gui.inMallocSize = MEM1_MAX_SIZE;
  s_gui.exMallocSize = MEM2_MAX_SIZE;
  /* The three touch buttons offer the same actions as the physical keys. */
  s_gui.mallocInButtonCallback = SaveBackground;
  s_gui.mallocExButtonCallback = ShowTest;
  s_gui.freeButtonCallback = RestoreBackground;
  InitGUI(&s_gui);
  GUIDrawTextLine(40, 376, (u32)"KEY1:Save   KEY2:test   KEY3:Restore/Free",
                  GUI_FONT_ASCII_24, 0, GUI_COLOR_WHITE, 1, 38);
  UpdateMemoryInfo();
  InitKeys();
}

void MallocTopTask(void)
{
  static u64 lastWaveTime = 0;
  u64 now;
  ScanKeys();
  now = GetSysTime();
  if((now - lastWaveTime) >= 250U)
  {
    lastWaveTime = now;
    s_gui.addMemoryWave(MemoryPerused(SRAMIN) / 10,
                        MemoryPerused(SRAMEX) / 10);
  }
  GUITask();
}
