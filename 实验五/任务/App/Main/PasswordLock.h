#ifndef PASSWORD_LOCK_H
#define PASSWORD_LOCK_H

void InitPasswordLock(void);
/* Call from the main loop's 1 ms task, never from an interrupt. */
void PasswordLockTask1ms(void);

#endif
