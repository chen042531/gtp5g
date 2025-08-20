#ifndef __REPORT_H__
#define __REPORT_H__

#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/net.h>
#include <linux/atomic.h>

#include "dev.h"

#define USAR_TRIGGER_PERIO  (1 << 0)
#define USAR_TRIGGER_VOLTH  (1 << 1)
#define USAR_TRIGGER_TIMTH  (1 << 2)
#define USAR_TRIGGER_QUHTI  (1 << 3)
#define USAR_TRIGGER_START  (1 << 4)
#define USAR_TRIGGER_STOPT  (1 << 5)
#define USAR_TRIGGER_DROTH  (1 << 6)
#define USAR_TRIGGER_IMMER  (1 << 7)
#define USAR_TRIGGER_VOLQU  (1 << 8)
#define USAR_TRIGGER_TIMQU  (1 << 9)
#define USAR_TRIGGER_LIUSA (1 << 10)
#define USAR_TRIGGER_TERMR (1 << 11)
#define USAR_TRIGGER_MONIT (1 << 12)
#define USAR_TRIGGER_ENVCL (1 << 13)
#define USAR_TRIGGER_MACAR (1 << 14)
#define USAR_TRIGGER_EVETH (1 << 15)
#define USAR_TRIGGER_EVEQU (1 << 16)
#define USAR_TRIGGER_TEBUR (1 << 17)
#define USAR_TRIGGER_IPMJL (1 << 18)
#define USAR_TRIGGER_QUVTI (1 << 19)
#define USAR_TRIGGER_EMRRE (1 << 20)
#define USAR_TRIGGER_UPINT (1 << 21)

struct VolumeMeasurement{
    atomic64_t totalVolume;
    atomic64_t uplinkVolume;
    atomic64_t downlinkVolume;
    atomic64_t totalPktNum;
    atomic64_t uplinkPktNum;
    atomic64_t downlinkPktNum;
}__attribute__((packed));

struct usage_report {
    u32 urrid; 					/* 8.2.54 URR_ID */
    u32 trigger ;
    struct VolumeMeasurement volmeasurement;

    u32 queryUrrRef;
    ktime_t start_time;
    ktime_t end_time;

    u64 seid;
} __attribute__((packed));

#endif // __REPORT_H__
