# SKB Routing Mark Reading Functionality in GTP5G Module

## Overview

This document describes the newly added functionality for reading routing rule mark values from SKB (Socket Buffer) in the GTP5G module.

## New Functions

### 1. `gtp5g_get_skb_routing_mark()`

```c
u32 gtp5g_get_skb_routing_mark(struct sk_buff *skb);
```

**Function**: Read routing rule mark value from SKB.

**Parameters**: 
- `skb`: socket buffer pointer

**Return Value**: SKB mark value (u32 type)

**Features**:
- Simple and direct mark reading
- Safe NULL pointer checking
- Lightweight implementation, suitable for high-frequency calls

### 2. `gtp5g_get_skb_routing_mark_with_info()`

```c
u32 gtp5g_get_skb_routing_mark_with_info(struct sk_buff *skb, const char *context);
```

**Function**: Read routing rule mark value from SKB and log information.

**Parameters**:
- `skb`: socket buffer pointer
- `context`: context string for logging

**Return Value**: SKB mark value (u32 type)

**Features**:
- Includes detailed logging
- Only logs when mark value is non-zero
- Suitable for debugging and monitoring scenarios

## Use Cases

### 1. Traffic Classification and Priority Handling
```c
u32 mark = gtp5g_get_skb_routing_mark(skb);
if (mark == PRIORITY_HIGH_MARK) {
    // Handle high priority traffic
} else if (mark == PRIORITY_LOW_MARK) {
    // Handle low priority traffic
}
```

### 2. Routing Decision
```c
u32 routing_mark = gtp5g_get_skb_routing_mark(skb);
if (routing_mark & SPECIAL_ROUTING_FLAG) {
    // Use special routing path
}
```

### 3. Debugging and Monitoring
```c
u32 mark = gtp5g_get_skb_routing_mark_with_info(skb, "PDR processing");
// Automatically logs in format:
// "GTP5G: PDR processing - SKB routing mark: 0x100 (256)"
```

## Implementation Example

In the existing GTP5G code, we have added usage examples in `src/gtpu/encap.c`:

```c
if ((fwd_policy = fwd_param->fwd_policy)) {
    u32 existing_mark = gtp5g_get_skb_routing_mark_with_info(skb, "Before setting FAR policy mark");
    skb->mark = fwd_policy->mark;
    GTP5G_TRC(dev, "Updated SKB mark from 0x%x to 0x%x", existing_mark, fwd_policy->mark);
}
```

## Important Notes

1. **Performance Considerations**: `gtp5g_get_skb_routing_mark()` is a lightweight function that can be called frequently. `gtp5g_get_skb_routing_mark_with_info()` includes logging, so it's recommended for debugging or monitoring scenarios.

2. **Return Value**: When SKB is NULL, the function returns 0. This is a safe default value.

3. **Mark Value Sources**: Mark values may come from:
   - iptables MARK target
   - fwmark set by ip rule
   - Marks set by other network processing modules

4. **Log Level**: The function with info uses `KERN_INFO` level for logging.

## Compilation and Installation

The new functions are integrated into the existing Makefile, compile normally:

```bash
make clean
make
```

## Related Files

- **Header file**: `include/util.h`
- **Implementation file**: `src/util.c`
- **Usage example**: `src/gtpu/encap.c`
- **Example code**: `example_usage.c`
