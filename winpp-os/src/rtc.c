#include "rtc.h"

static u8 cmos_read(u8 reg)
{
    outb(0x70, (u8)(reg | 0x80));   /* NMI 关 */
    return inb(0x71);
}

static u8 bcd2bin(u8 v) { return (u8)((v & 0x0F) + ((v >> 4) * 10)); }

void rtc_get_datetime(u16 *year, u8 *month, u8 *day, u8 *hour, u8 *min, u8 *sec)
{
    u8 st;
    do { st = cmos_read(0x0A); } while (st & 0x80);   /* 等待更新完成 */

    if (sec)  *sec  = bcd2bin(cmos_read(0x00));
    if (min)  *min  = bcd2bin(cmos_read(0x02));
    if (hour) *hour = bcd2bin(cmos_read(0x04));
    if (day)  *day  = bcd2bin(cmos_read(0x07));
    if (month)*month= bcd2bin(cmos_read(0x08));
    if (year) {
        u16 c = bcd2bin(cmos_read(0x32));       /* 世纪寄存器（QEMU 有效） */
        u16 y = bcd2bin(cmos_read(0x09));
        *year = c ? (u16)(c * 100 + y) : (u16)(y < 80 ? 2000 + y : 1900 + y);
    }
}
