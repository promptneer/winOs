#ifndef SERIAL_H
#define SERIAL_H

#include "io.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_printf(const char *fmt, ...);
int serial_data_available(void);
int serial_getc(void);

#endif
