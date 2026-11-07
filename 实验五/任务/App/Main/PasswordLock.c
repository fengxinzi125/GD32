#include "PasswordLock.h"
#include "gd32f30x_conf.h"
#include "NandFlash.h"
#include <stdio.h>
#include <string.h>

/* Standalone raw-NAND experiment: do not run InitFTL / the NAND GUI. */
#define LOCK_PASSWORD       0xF3
#define LOCK_WRONG_PASSWORD 0x12
#define LOCK_SECTOR_SIZE    512
#define LOCK_FIRST_BLOCK    (NAND_BLOCK_COUNT - 16)
#define LOCK_NO_BLOCK       NAND_BLOCK_COUNT
#define LOCK_DEBOUNCE_MS    20

static u8 s_buffer[LOCK_SECTOR_SIZE];
/* Byte 0 is the password; the remaining header identifies our own record. */
static const u8 s_header[] = {LOCK_PASSWORD, 'L', 'O', 'C', 'K', 1, 0x0C, 0xA5};
static u32 s_passwordBlock;
static u8 s_ready;
static u8 s_inputPassword;
static u8 s_hasInput;
static u8 s_lastRawKey;
static u8 s_stableKey;
static u8 s_sameSamples;
static u8 s_keyArmed;

static void InitLockKeys(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOG);
    /* KEY1 closes towards 3V3; KEY2 and KEY3 close towards GND. */
    gpio_init(GPIOA, GPIO_MODE_IPD, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    gpio_init(GPIOG, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_13 | GPIO_PIN_14);
}

static u8 ReadRawKey(void)
{
    u8 mask = 0;
    if(gpio_input_bit_get(GPIOA, GPIO_PIN_0) == SET) {mask |= 1;}
    if(gpio_input_bit_get(GPIOG, GPIO_PIN_13) == RESET) {mask |= 2;}
    if(gpio_input_bit_get(GPIOG, GPIO_PIN_14) == RESET) {mask |= 4;}
    if(mask == 0) {return 0;}
    if(mask == 1) {return 1;}
    if(mask == 2) {return 2;}
    if(mask == 4) {return 3;}
    return 4; /* Multiple keys: ignore until all keys are released. */
}

static u8 ScanLockKey(void)
{
    u8 raw = ReadRawKey();
    if(raw != s_lastRawKey)
    {
        s_lastRawKey = raw;
        s_sameSamples = 1;
    }
    else if(s_sameSamples < LOCK_DEBOUNCE_MS)
    {
        s_sameSamples++;
    }

    if(s_sameSamples < LOCK_DEBOUNCE_MS) {return 0;}
    if(raw == 0)
    {
        s_stableKey = 0;
        s_keyArmed = 1;
        return 0;
    }
    if(raw == s_stableKey) {return 0;}
    s_stableKey = raw;
    if(raw == 4)
    {
        s_keyArmed = 0;
        return 0;
    }
    if(!s_keyArmed) {return 0;}
    s_keyArmed = 0;
    return raw;
}

static u8 IsPasswordRecord(void)
{
    u32 i;
    if(memcmp(s_buffer, s_header, sizeof(s_header)) != 0) {return 0;}
    for(i = sizeof(s_header); i < sizeof(s_buffer); i++)
    {
        if(s_buffer[i] != 0xFF) {return 0;}
    }
    return 1;
}

static u32 ReadStoredPassword(u8 *password)
{
    if(NandReadPage(s_passwordBlock, 0, 0, s_buffer,
                    sizeof(s_buffer)) != NAND_OK)
    {
        return NAND_FAIL;
    }
    if(!IsPasswordRecord()) {return NAND_FAIL;}
    *password = s_buffer[0];
    return NAND_OK;
}

static u32 PreparePassword(void)
{
    u32 block;
    u32 page;
    u32 blankBlock = LOCK_NO_BLOCK;
    u8 spare0[NAND_SPARE_AREA_SIZE];
    u8 spare1[NAND_SPARE_AREA_SIZE];
    u8 savedPassword;

    /* Search for an existing record BEFORE selecting a new blank block. */
    for(block = LOCK_FIRST_BLOCK; block < NAND_BLOCK_COUNT; block++)
    {
        if(NandReadSpare(block, 0, 0, spare0, sizeof(spare0)) != NAND_OK ||
           NandReadSpare(block, 1, 0, spare1, sizeof(spare1)) != NAND_OK)
        {
            continue;
        }
        /* Preserve factory/runtime bad-block marks and FTL metadata. */
        if(spare0[0] != 0xFF || spare1[0] != 0xFF) {continue;}
        if(spare0[1] != 0xFF || spare0[2] != 0xFF ||
           spare0[3] != 0xFF || spare0[4] != 0xFF ||
           spare0[5] != 0xFF) {continue;}

        if(NandCheckPage(block, 0, 0xFF, 1) == NAND_OK)
        {
            if(blankBlock != LOCK_NO_BLOCK) {continue;}
            for(page = 1; page < NAND_BLOCK_SIZE; page++)
            {
                if(NandCheckPage(block, page, 0xFF, 1) != NAND_OK) {break;}
            }
            if(page == NAND_BLOCK_SIZE) {blankBlock = block;}
        }
        else
        {
            s_passwordBlock = block;
            if(ReadStoredPassword(&savedPassword) == NAND_OK)
            {
                printf("Password loaded, physical block %u\r\n", block);
                return NAND_OK;
            }
        }
    }

    if(blankBlock == LOCK_NO_BLOCK)
    {
        printf("NAND error: no blank good block in %u..%u; no erase performed\r\n",
               (u32)LOCK_FIRST_BLOCK, (u32)NAND_BLOCK_COUNT - 1);
        return NAND_FAIL;
    }

    /* The entire block is already erased (Main + Spare all 0xFF).
       No block erase is needed, and existing data is never erased. */
    memset(s_buffer, 0xFF, sizeof(s_buffer));
    memcpy(s_buffer, s_header, sizeof(s_header));
    if(NandWritePage(blankBlock, 0, 0, s_buffer, sizeof(s_buffer)) != NAND_OK)
    {
        printf("NAND error: password write failed\r\n");
        return NAND_FAIL;
    }
    s_passwordBlock = blankBlock;
    if(ReadStoredPassword(&savedPassword) != NAND_OK)
    {
        printf("NAND error: password read-back failed\r\n");
        return NAND_FAIL;
    }
    printf("Password saved and verified, physical block %u\r\n", blankBlock);
    return NAND_OK;
}

void InitPasswordLock(void)
{
    u32 id;
    s_ready = 0;
    s_hasInput = 0;
    s_inputPassword = 0;
    s_passwordBlock = LOCK_NO_BLOCK;
    s_lastRawKey = 0;
    s_stableKey = 0;
    s_sameSamples = 0;
    s_keyArmed = 0;
    InitLockKeys();
    printf("\r\nNAND password lock\r\n");
    if(InitNandFlash() != NAND_OK)
    {
        printf("NAND error: initialization failed\r\n");
        return;
    }
    /* InitNandFlash currently returns 0 unconditionally; also reject no chip. */
    id = NandReadID();
    if(id == 0 || id == 0xFFFFFFFF)
    {
        printf("NAND error: invalid chip ID\r\n");
        return;
    }
    if(PreparePassword() != NAND_OK) {return;}
    s_ready = 1;
    printf("Ready: KEY1=input F3, KEY2=input 12, KEY3=check\r\n");
}

void PasswordLockTask1ms(void)
{
    u8 key = ScanLockKey();
    u8 savedPassword;
    if(key == 1 || key == 2)
    {
        s_inputPassword = (key == 1) ? LOCK_PASSWORD : LOCK_WRONG_PASSWORD;
        s_hasInput = 1;
        printf("input: 0x%02X\r\n", (unsigned int)s_inputPassword);
    }
    else if(key == 3)
    {
        if(!s_ready)
        {
            printf("NAND error: lock not ready\r\nfailure\r\n");
        }
        else if(!s_hasInput)
        {
            printf("failure\r\n");
        }
        else if(ReadStoredPassword(&savedPassword) != NAND_OK)
        {
            printf("NAND error: password read failed\r\nfailure\r\n");
        }
        else
        {
            printf(s_inputPassword == savedPassword ? "success\r\n" : "failure\r\n");
        }
        /* Each check consumes the input, so KEY3 alone cannot reuse it. */
        s_hasInput = 0;
    }
}
