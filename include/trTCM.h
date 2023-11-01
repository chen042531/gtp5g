#include <linux/ktime.h>
typedef enum {
    Green,
    Yellow,
    Red
} Color;

typedef struct {
    int cir;        // Committed Information Rate in Mbps
    int pir;        // Peak Information Rate in Mbps
    int cirBurst;   // Burst size for CIR in KB
    int pirBurst;   // Burst size for PIR in KB
    int tokenCIR;   // Tokens available for CIR
    int tokenPIR;   // Tokens available for PIR
    int tokenRate; 
    int tokenCRate;  // Token refill rate in Mbps
    int tokenPRate;  // Token refill rate in Mbps
    // int lastUpdate;
    u64 lastUpdate;
} TrafficPolicer;

extern TrafficPolicer* newTrafficPolicer(int, int, int, int, int);
extern void refillTokens(TrafficPolicer*); 
extern Color policePacket(TrafficPolicer*, int, int);
