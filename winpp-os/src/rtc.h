#ifndef RTC_H
#define RTC_H

#include "io.h"

void rtc_get_datetime(u16 *year, u8 *month, u8 *day, u8 *hour, u8 *min, u8 *sec);

#endif
