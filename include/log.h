#ifndef __GTP5G_LOG_H__
#define __GTP5G_LOG_H__

#include <linux/netdevice.h>
#include <linux/rtc.h>

#define DBG(level, dev, fmt, args...) do {      \
    if (level <= get_dbg_lvl()) {               \
        if (dev)                                \
            printk_ratelimited("%s:[gtp5g] %s: "fmt, netdev_name(dev), __func__, ##args);   \
        else                                    \
            printk_ratelimited("[gtp5g] %s: " fmt, __func__, ##args);       \
    } \
} while(0)

#define PRINTK_TIME(fmt, ...) do { \
    struct timespec64 ts; \
    struct rtc_time tm; \
    ktime_get_real_ts64(&ts); \
    rtc_time64_to_tm(ts.tv_sec, &tm); \
    printk(KERN_INFO "[%04d-%02d-%02d %02d:%02d:%02d] " fmt "\n", \
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
           tm.tm_hour, tm.tm_min, tm.tm_sec, ##__VA_ARGS__); \
} while(0)

#define GTP5G_LOG(dev, fmt, args...) DBG(0, dev, fmt, ##args)
#define GTP5G_ERR(dev, fmt, args...) DBG(1, dev, fmt, ##args)
#define GTP5G_WAR(dev, fmt, args...) DBG(2, dev, fmt, ##args)
#define GTP5G_INF(dev, fmt, args...) DBG(3, dev, fmt, ##args)
#define GTP5G_TRC(dev, fmt, args...) DBG(4, dev, fmt, ##args)

int get_dbg_lvl(void);
void set_dbg_lvl(int);

#endif // __GTP5G_LOG_H__
