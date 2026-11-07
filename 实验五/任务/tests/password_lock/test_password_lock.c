/* Host behavioral tests against the actual firmware module, with mocked I/O.
   gcc -std=c99 -Wall -Wextra -Werror -I tests/password_lock
       tests/password_lock/test_password_lock.c -o tmp/password-lock/tests.exe */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char output[32768];
static int captured_printf(const char *format, ...)
{
    int result;
    size_t used = strlen(output);
    va_list args;
    va_start(args, format);
    result = vsnprintf(output + used, sizeof(output) - used, format, args);
    va_end(args);
    assert(result >= 0 && (size_t)result < sizeof(output) - used);
    return result;
}
#define printf captured_printf
#include "../../App/Main/PasswordLock.c"
#undef printf

static u8 flash[16][64][2112];
static u8 keyMask;
static u32 writes;
static u32 reads;
static u32 chipId;
static int failRead;
static int failWrite;
static int corruptWrite;
static int failSpare;

void rcu_periph_clock_enable(u32 clock) {(void)clock;}
void gpio_init(u32 p, u32 m, u32 s, u32 pin)
{(void)p; (void)m; (void)s; (void)pin;}
u8 gpio_input_bit_get(u32 port, u32 pin)
{
    if(port == GPIOA && pin == GPIO_PIN_0) {return !!(keyMask & 1);}
    if(port == GPIOG && pin == GPIO_PIN_13) {return !(keyMask & 2);}
    assert(port == GPIOG && pin == GPIO_PIN_14);
    return !(keyMask & 4);
}
static u8 *page_data(u32 block, u32 page)
{
    assert(block >= LOCK_FIRST_BLOCK && block < NAND_BLOCK_COUNT && page < 64);
    return flash[block - LOCK_FIRST_BLOCK][page];
}
u32 InitNandFlash(void) {return NAND_OK;}
u32 NandReadID(void) {return chipId;}
u32 NandReadSpare(u32 b, u32 p, u32 c, u8 *buf, u32 len)
{
    assert(c + len <= 64);
    if(failSpare) {return NAND_FAIL;}
    memcpy(buf, page_data(b, p) + 2048 + c, len);
    return NAND_OK;
}
u32 NandReadPage(u32 b, u32 p, u32 c, u8 *buf, u32 len)
{
    reads++;
    assert(c == 0 && len == 512);
    if(failRead) {return NAND_FAIL;}
    memcpy(buf, page_data(b, p) + c, len);
    return NAND_OK;
}
u32 NandWritePage(u32 b, u32 p, u32 c, u8 *buf, u32 len)
{
    u32 i;
    u8 *dest = page_data(b, p);
    writes++;
    assert(c == 0 && len == 512);
    for(i = 0; i < 2112; i++) {assert(dest[i] == 0xFF);}
    if(failWrite) {return NAND_FAIL;}
    memcpy(dest, buf, len);
    dest[2048 + 16] = 0; /* Simulated ECC, not a bad-block mark. */
    if(corruptWrite) {dest[0] = 0;}
    return NAND_OK;
}
u32 NandCheckPage(u32 b, u32 p, u32 value, u32 mode)
{
    u32 i;
    u8 *data = page_data(b, p);
    assert(mode == 1);
    for(i = 0; i < 2112; i++) {if(data[i] != value) {return NAND_FAIL;}}
    return NAND_OK;
}
static void samples(u8 mask, int count)
{
    int i;
    keyMask = mask;
    for(i = 0; i < count; i++) {PasswordLockTask1ms();}
}
static void press(u8 mask)
{
    samples(0, 20);
    samples(mask, 20);
    samples(0, 20);
}
static void reset_fixture(void)
{
    memset(flash, 0xFF, sizeof(flash));
    output[0] = 0;
    writes = reads = keyMask = 0;
    failRead = failWrite = corruptWrite = failSpare = 0;
    chipId = 0x12345678;
}

int main(void)
{
    u32 before;
    reset_fixture();
    InitPasswordLock();
    assert(s_ready && writes == 1 && flash[0][0][0] == 0xF3);
    InitPasswordLock(); /* Reboot reuses the record, no repeated programming. */
    assert(s_ready && writes == 1);
    output[0] = 0;
    press(1); before = reads; press(4);
    assert(reads == before + 1 && strstr(output, "success\r\n"));
    output[0] = 0;
    press(2); press(4);
    assert(strstr(output, "failure\r\n") && !strstr(output, "success"));
    output[0] = 0;
    press(4); /* No input after a completed check. */
    assert(strcmp(output, "failure\r\n") == 0);
    output[0] = 0;
    press(1); failRead = 1; press(4); failRead = 0;
    assert(strstr(output, "password read failed") && !strstr(output, "success"));
    output[0] = 0;
    samples(0, 20); samples(1, 19);
    assert(!output[0]);
    samples(1, 1); before = (u32)strlen(output); samples(1, 300);
    assert(strlen(output) == before); /* Holding does not retrigger. */
    output[0] = 0;
    samples(0, 20); samples(3, 20); samples(4, 20);
    assert(!output[0]); /* Chords cannot turn into a check until release. */
    samples(0, 20); press(4);
    assert(strstr(output, "success\r\n"));

    reset_fixture();
    flash[0][0][2048] = 0; /* First-page bad mark. */
    flash[1][1][2048] = 0; /* Second-page bad mark. */
    flash[2][0][2050] = 7; /* Existing FTL mapping. */
    flash[3][63][2111] = 0; /* Nonempty tail despite blank first page. */
    InitPasswordLock();
    assert(s_ready && s_passwordBlock == LOCK_FIRST_BLOCK + 4 && writes == 1);
    assert(flash[0][0][2048] == 0 && flash[1][1][2048] == 0);
    assert(flash[2][0][2050] == 7 && flash[3][63][2111] == 0);
    /* An earlier blank block must not hide a later saved record on reboot. */
    memset(flash[0], 0xFF, sizeof(flash[0]));
    InitPasswordLock();
    assert(s_ready && s_passwordBlock == LOCK_FIRST_BLOCK + 4 && writes == 1);

    reset_fixture(); memset(flash, 0, sizeof(flash)); InitPasswordLock();
    assert(!s_ready && writes == 0 && strstr(output, "no blank good block"));
    reset_fixture(); failWrite = 1; InitPasswordLock();
    assert(!s_ready && strstr(output, "write failed"));
    reset_fixture(); corruptWrite = 1; InitPasswordLock();
    assert(!s_ready && strstr(output, "read-back failed"));
    reset_fixture(); failSpare = 1; InitPasswordLock();
    assert(!s_ready && writes == 0);
    reset_fixture(); chipId = 0xFFFFFFFF; InitPasswordLock();
    assert(!s_ready && writes == 0 && strstr(output, "invalid chip ID"));
    printf("PASS: password persistence, comparison, debounce, I/O failures, block preservation\n");
    return 0;
}
