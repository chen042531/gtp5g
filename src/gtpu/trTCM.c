#include <linux/time.h>
#include <linux/slab.h>
#include <linux/random.h>

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
    p->tokenCIR = cir;
    p->tokenPIR = pir;
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
    int tokensToAdd = elapsed * p->tokenRate / 1000000000;
    p->tokenCIR = (p->tokenCIR + tokensToAdd) < p->cir ? (p->tokenCIR + tokensToAdd) : p->cir;
    p->tokenPIR = (p->tokenPIR + tokensToAdd) < p->pir ? (p->tokenPIR + tokensToAdd) : p->pir;
    p->lastUpdate = now;
    printk("@now: %lld, elapsed: %lld, tokensToAdd: %d", now, elapsed, tokensToAdd);
}

Color policePacket(TrafficPolicer* p, int rate, int burst) {
    int probability, random_number;
    refillTokens(p);
    printk("burst: %d", burst);

    // probability = (p->cir - p->tokenCIR) / (p->cir - 10) * p->cir;
    probability = (p->cir - p->tokenCIR)/1000;
    printk("p->cir:%d, p->tokenCIR:%d", p->cir, p->tokenCIR);
    random_number = prandom_u32() % p->cir;
    printk("random_number:%d, probability:%d", random_number, probability);
    if (random_number < probability) {
        // Drop the packet
        printk(">>>> ^^drop packet Red");
        return Red;
    }


    if (rate <= p->cir && rate <= p->tokenCIR) {
        p->tokenCIR -= rate;
        printk("tokenCIR: %d, rate: %d", p->tokenCIR, rate);
        return Green;
    } else if (rate <= p->pir && rate <= p->tokenPIR) {
        printk("tokenPIR: %d, rate: %d", p->tokenPIR, rate);
        p->tokenPIR -= rate;
        return Yellow;
    } else {
        return Red;
    }
}