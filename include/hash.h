#ifndef __HASH_H__
#define __HASH_H__

#include <linux/string.h>
#include <linux/jhash.h>
#include <linux/xxhash.h>
#include <linux/siphash.h>

extern u32 gtp5g_h_initval;

static inline u32 u32_hashfn(u32 val)
{
    return jhash_1word(val, gtp5g_h_initval);
}

static inline u32 str_hashfn(const char *str)
{
    return xxh32(str, strlen(str), gtp5g_h_initval);
}

static inline u32 ipv4_hashfn(__be32 ip)
{
    return jhash_1word((__force u32)ip, gtp5g_h_initval);
}

#endif // __HASH_H__