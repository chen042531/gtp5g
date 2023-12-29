#include <linux/time.h>
#include <linux/slab.h>
#include <linux/random.h>

#include "trTCM.h"
#include "log.h"

TrafficPolicer* newTrafficPolicer(u64 tokenRate) {
    TrafficPolicer* p = (TrafficPolicer*)kmalloc(sizeof(TrafficPolicer), GFP_KERNEL);
    if (p == NULL) {
        GTP5G_ERR(NULL, "traffic policer memory allocation error\n");
    }
    
    p->tokenRate = tokenRate * 125 ; // Kbit/s to byte/s (*1000/8)

    // 1ms as burst size
    p->cbs = p->tokenRate / 125; // bytes
    p->ebs = p->cbs * 4; // bytes

    // fill buckets at the begining
    p->tc = p->cbs; 
    p->te = p->ebs; 

    p->lastUpdate = ktime_get_ns();

    return p;
} 

void refillToken(TrafficPolicer* p) {
    u64 tokensToAdd;
    u64 tc, te;
    u64 elapsed;
    u64 now = ktime_get_ns();

    spin_lock(&p->lock); 

    elapsed = now - p->lastUpdate;
    p->lastUpdate = now;

    tokensToAdd = elapsed * p->tokenRate / 1000000000;
 
    tc = p->tc + tokensToAdd;
    te = p->te;
    if (tc > p->cbs){
        te += (tc - p->cbs);
        if (te > p->ebs){
            te = p->ebs;
        }
        tc = p->cbs; 
    }

    p->te = te;
    p->tc = tc;
    spin_unlock(&p->lock);
}

Color policePacket(TrafficPolicer* p, int pktLen) {
    u64 tc, te;

    refillToken(p);

    spin_lock(&p->lock); 

    tc = p->tc;
    te = p->te;   

    if (tc >= pktLen) {
        p->tmp_tc = tc - pktLen;
        p->tmp_te = te;
        spin_unlock(&p->lock); 
        return Green;
    }

    if (p->tmp_te >= pktLen) {
        p->tmp_tc = tc;
        p->tmp_te = te - pktLen;
        spin_unlock(&p->lock); 
        return Yellow;
    }

    p->tmp_tc = tc;
    p->tmp_te = te;

    spin_unlock(&p->lock); 
    return Red;
}

void update_policer_token(TrafficPolicer* p) {
    spin_lock(&p->lock); 

    p->tc = p->tmp_tc;
    p->te = p->tmp_te; 

    spin_unlock(&p->lock); 
}