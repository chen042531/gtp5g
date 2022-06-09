#ifndef GTP5G_GENL_H__
#define GTP5G_GENL_H__

#include <net/genetlink.h>

enum gtp5g_cmd {
    GTP5G_CMD_UNSPEC = 0,

    GTP5G_CMD_ADD_PDR,
    GTP5G_CMD_ADD_FAR,
    GTP5G_CMD_ADD_QER,

    GTP5G_CMD_DEL_PDR,
    GTP5G_CMD_DEL_FAR,
    GTP5G_CMD_DEL_QER,

    GTP5G_CMD_GET_PDR,
    GTP5G_CMD_GET_FAR,
    GTP5G_CMD_GET_QER,

    GTP5G_CMD_ADD_URR,
    GTP5G_CMD_ADD_BAR,
    GTP5G_CMD_DEL_URR,
    GTP5G_CMD_DEL_BAR,
    GTP5G_CMD_GET_URR,
    GTP5G_CMD_GET_BAR,
    /* Add newly supported feature ON ABOVE
     * for compatability with older version of
     * free5GC's UPF or libgtp5gnl
     * */

    __GTP5G_CMD_MAX,
};
#define GTP5G_CMD_MAX (__GTP5G_CMD_MAX - 1)

/* This const value need to bigger than the Layer 1 attr size,
 * like GTP5G_PDR_ATTR_MAX and GTP5G_FAR_ATTR_MAX
 */
#define GTP5G_ATTR_MAX 0x10

enum gtp5g_device_attrs {
    GTP5G_LINK = 1,
    GTP5G_NET_NS_FD,
};

/* ------------------------------------------------------------------
 *                              URR
 * ------------------------------------------------------------------
 * */
enum gtp5g_urr_attrs {
    GTP5G_URR_ID = 3,
    GTP5G_URR_MEASUREMENT_METHOD,
    GTP5G_URR_REPORTING_TRIGGER,
    GTP5G_URR_MEASUREMENT_PERIOD,
    GTP5G_URR_MEASUREMENT_INFO,
    GTP5G_URR_SEQ, // 3GPP TS 29.244 table 7.5.8.3-1 UR-SEQN
    GTP5G_URR_SEID,

    __GTP5G_URR_ATTR_MAX,
};
#define GTP5G_URR_ATTR_MAX (__GTP5G_URR_ATTR_MAX - 1)

struct user_report {
    u32 flag;
    u64 totalVolume;
    u64 uplinkVolume;
    u64 downlinkVolume;
    u64 totalPktNum;
    u64 uplinkPktNum;
    u64 downlinkPktNum;
} __attribute__((packed));

/* ------------------------------------------------------------------
 *	                            BAR
 * ------------------------------------------------------------------
 * */
enum gtp5g_bar_attrs {
    GTP5G_BAR_ID = 3,
    GTP5G_DOWNLINK_DATA_NOTIFICATION_DELAY,
    GTP5G_BUFFERING_PACKETS_COUNT,
    GTP5G_BAR_SEID,

    __GTP5G_BAR_ATTR_MAX,
};
#define GTP5G_BAR_ATTR_MAX (__GTP5G_BAR_ATTR_MAX - 1)

struct buffer_action {
    u64 seid;
    u16 notification_delay;
    u32 buffer_packet_count;
} __attribute__((packed));

extern struct genl_family gtp5g_genl_family;

#endif