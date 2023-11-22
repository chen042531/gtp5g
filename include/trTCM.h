#include <linux/ktime.h>
typedef enum {
    Green,
    Yellow,
    Red
} Color;

typedef struct {
    int cbs;
    int ebs;
    int tc;
    int te;  
    int tokenRate; 
    u64 lastUpdate;
} TrafficPolicer;

extern TrafficPolicer* newTrafficPolicer(int, int, int);
extern void refillTokens(TrafficPolicer*); 
extern Color policePacket(TrafficPolicer*, int);
