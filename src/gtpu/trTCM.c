#include <linux/time.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "trTCM.h"

TrafficPolicer* newTrafficPolicer(u64 cbs, u64 ebs, u64 tokenRate) {
    TrafficPolicer* p = (TrafficPolicer*)kmalloc(sizeof(TrafficPolicer), GFP_KERNEL);
    if (p == NULL) {
        printk("Memory allocation error\n");
    }
    
    p->tokenRate = tokenRate * 125 ; // Kbit/s to byte/s (*1000/8)
    printk(">>>> p->tokenRate:%lld", p->tokenRate);

    // 4ms as burst size
    p->cbs = p->tokenRate / 250; // bytes
    p->ebs = p->cbs * 4; // bytes

    printk(">>>> cbs:%lld", cbs);
    // fill buckets at the begining
    p->tc = cbs; 
    p->te = ebs; 

    
    // p->lastUpdate = current_kernel_time().tv_nsec;
    p->lastUpdate = ktime_get_ns();
    // p->lastUpdate = ktime_get_seconds();
    // p->lastUpdate = ktime_get();

    return p;
} 


Color policePacket(TrafficPolicer* p, int pktLen) {
    u64 tokensToAdd;
    u64 tc, te;
    u64 elapsed;
    // ktime_t now =  ktime_get();
    u64 now = ktime_get_ns();
    // u64 now =  ktime_get_seconds();

    spin_lock(&p->lock); 

    elapsed = now - p->lastUpdate;
    p->lastUpdate = now;
    // printk("elapsed:%lld", elapsed);
    tokensToAdd = elapsed * p->tokenRate / 1000000000;
    // printk("elapsed:%lld", elapsed);
    // printk("tokensToAdd:%lld", tokensToAdd);
    tc = p->tc + tokensToAdd;
    te = p->te;
    if (tc > p->cbs){
        te += (tc - p->cbs);
        if (te > p->ebs){
            te = p->ebs;
        }
        tc = p->cbs; 
    }
   
    
    if (p->tc >= pktLen) {
        p->tc = tc - pktLen;
        p->te = te;
        spin_unlock(&p->lock); 
        return Green;
    }

    if (p->te >= pktLen) {
        p->tc = tc;
        p->te = te - pktLen;
        spin_unlock(&p->lock); 
        return Yellow;
    }

    p->tc = tc;
    p->te = te;
    // printk(">>> Red ");
    spin_unlock(&p->lock); 
    return Red;
}