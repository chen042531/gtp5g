#ifndef __HASH_H__
#define __HASH_H__

#include <linux/string.h>
#include <linux/jhash.h>
#include <linux/xxhash.h>
#include <linux/siphash.h>

extern u32 gtp5g_h_initval;

// 使用FNV-1a哈希算法
static inline u32 fnv1a_hash(u32 val)
{
    // FNV-1a 哈希的參數
    const u32 FNV_PRIME = 16777619;
    const u32 FNV_OFFSET = 2166136261;
    
    u32 hash = FNV_OFFSET;
    u8 *bytes = (u8 *)&val;
    int i;
    
    for (i = 0; i < 4; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

// 改進的IP地址哈希函數
static inline u32 ipv4_hashfn(__be32 ip)
{
    u32 hash;
    u32 ip_host = ntohl(ip);  // 轉換為主機字節序
    
    // 結合多個哈希方法
    hash = fnv1a_hash(ip_host);
    hash ^= jhash_1word(ip_host, gtp5g_h_initval);
    hash ^= xxh32(&ip_host, sizeof(ip_host), gtp5g_h_initval);
    
    return hash;
}

static inline u32 u32_hashfn(u32 val)
{
    return fnv1a_hash(val);
}

static inline u32 str_hashfn(const char *str)
{
    return xxh32(str, strlen(str), gtp5g_h_initval);
}

#endif // __HASH_H__