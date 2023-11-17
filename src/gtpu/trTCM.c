#include <linux/time.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "trTCM.h"

TrafficPolicer* newTrafficPolicer(int cirBurst, int pirBurst, int tokenCRate, int tokenPRate) {
    TrafficPolicer* p = (TrafficPolicer*)kmalloc(sizeof(TrafficPolicer), GFP_KERNEL);
    if (p == NULL) {
        printk("Memory allocation error\n");
    }
    
    p->cirBurst = cirBurst; // gbr bucket size
    p->pirBurst = pirBurst; // mbr bucket size
    p->tokenCRate = tokenCRate; // gbr 
    p->tokenPRate = tokenPRate; // mbr
    p->tokenCIR = 0;
    p->tokenPIR = 0;
    // p->lastUpdate = current_kernel_time().tv_nsec;
    p->lastUpdate = ktime_get_ns();
    // p->lastUpdate = ktime_get_seconds();
    // p->lastUpdate = ktime_get();

    return p;
} 

void refillTokens(TrafficPolicer* p) {
    // ktime_t now =  ktime_get();
    u64 now = ktime_get_ns();
    // int now =  ktime_get_seconds();
    u64 elapsed = now - p->lastUpdate;
    int tokensCToAdd = elapsed * p->tokenCRate / 1000000000;
    int tokensPToAdd = elapsed * p->tokenPRate / 1000000000;
    p->tokenCIR = (p->tokenCIR + tokensCToAdd) < p->cirBurst ? (p->tokenCIR + tokensCToAdd) : p->cirBurst;
    p->tokenPIR = (p->tokenPIR + tokensPToAdd) < p->pirBurst ? (p->tokenPIR + tokensPToAdd) : p->pirBurst;
    p->lastUpdate = now;
    // printk("@now: %lld, elapsed: %lld, tokensPToAdd: %d", now, elapsed, tokensPToAdd);
    // printk("@now: %lld, elapsed: %lld, tokensCToAdd: %d", now, elapsed, tokensCToAdd);
}

Color policePacket(TrafficPolicer* p, int rate) {
    // printk(">>>>>## policePacket");
    refillTokens(p);
    
    if (p->tokenPIR < rate) {
        return Red;
    }

    if (p->tokenCIR < rate) {
        p->tokenCIR -= rate;
        return Yellow;
    }

    p->tokenCIR -= rate;
    p->tokenPIR -= rate;
    return Green;
}