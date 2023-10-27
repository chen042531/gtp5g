#include <linux/time.h>
#include <linux/slab.h>

#include "trTCM.h"

TrafficPolicer* newTrafficPolicer(int cir, int pir, int cirBurst, int pirBurst, int tokenRate) {
    TrafficPolicer* p = (TrafficPolicer*)kmalloc(sizeof(TrafficPolicer), GFP_KERNEL);
    if (p == NULL) {
        printk("Memory allocation error\n");
    }
    
    p->cir = cir;
    p->pir = pir;
    p->cirBurst = cirBurst;
    p->pirBurst = pirBurst;
    p->tokenRate = tokenRate;
    p->tokenCIR = 0;
    p->tokenPIR = 0;
    // p->lastUpdate = current_kernel_time().tv_nsec;
    p->lastUpdate = ktime_get_ns();

    return p;
}

void refillTokens(TrafficPolicer* p) {
    time_t now =  ktime_get_ns();
    int elapsed = ktime_sub(now, p->lastUpdate)/100000;
    int tokensToAdd = elapsed * p->tokenRate;
    p->tokenCIR = (p->tokenCIR + tokensToAdd) < p->cirBurst ? (p->tokenCIR + tokensToAdd) : p->cirBurst;
    p->tokenPIR = (p->tokenPIR + tokensToAdd) < p->pirBurst ? (p->tokenPIR + tokensToAdd) : p->pirBurst;
    p->lastUpdate = now;
    printk("now: %ld, elapsed: %d, tokensToAdd: %d", now, elapsed, tokensToAdd);
}

Color policePacket(TrafficPolicer* p, int rate, int burst) {
    refillTokens(p);
    
    if (rate <= p->cir && burst <= p->tokenCIR) {
        p->tokenCIR -= burst;
        return Green;
    } else if (rate <= p->pir && burst <= p->tokenPIR) {
        p->tokenPIR -= burst;
        return Yellow;
    } else {
        return Red;
    }
}