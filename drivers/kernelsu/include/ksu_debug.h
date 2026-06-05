#ifndef __KSU_H_KSU_DEBUG
#define __KSU_H_KSU_DEBUG

#include <linux/init.h>

void ksu_debug_init(void);
void ksu_debug_exit(void);

void ksu_debug_printf(const char *fmt, ...);

#endif
