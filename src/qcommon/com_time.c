#include "com_time.h"

#include <stddef.h>

/*
 * The Windows client and Linux dedicated server perform the same operation:
 * call time(NULL), optionally call localtime, and copy the nine consecutive
 * 32-bit struct tm fields through qtime_t +0x20 in declaration order.
 *
 *   CoDUOMP.exe    0x004354c0..0x0043551a
 *   coduo_lnxded   0x0806ba76..0x0806bb24
 *
 * The supporting Mac client exports the same canonical Com_RealTime name.
 * time_t remains the host ABI carrier; both authoritative i386 targets return
 * their 32-bit time_t in EAX.
 */
time_t Com_RealTime(qtime_t *qtime)
{
    const time_t now = time(NULL);

    if (qtime != NULL) {
        const struct tm *localTime = localtime(&now);
        if (localTime != NULL) {
            qtime->tm_sec = localTime->tm_sec;
            qtime->tm_min = localTime->tm_min;
            qtime->tm_hour = localTime->tm_hour;
            qtime->tm_mday = localTime->tm_mday;
            qtime->tm_mon = localTime->tm_mon;
            qtime->tm_year = localTime->tm_year;
            qtime->tm_wday = localTime->tm_wday;
            qtime->tm_yday = localTime->tm_yday;
            qtime->tm_isdst = localTime->tm_isdst;
        }
    }

    return now;
}
