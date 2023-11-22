#include <linux/ktime.h>
typedef enum {
    Green,
    Yellow,
    Red
} Color;

typedef struct {
    u64 cbs;
    u64 ebs;
    u64 tc;
    u64 te;  
    u64 tokenRate; 
    u64 lastUpdate;
} TrafficPolicer;

extern TrafficPolicer* newTrafficPolicer(u64, u64, u64);
extern void refillTokens(TrafficPolicer*); 
extern Color policePacket(TrafficPolicer*, int);
