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

enum gtp5g_pdr_attrs {
    /* gtp5g_device_attrs in this part */

    GTP5G_PDR_ID = 3,
    GTP5G_PDR_PRECEDENCE,
    GTP5G_PDR_PDI,
    GTP5G_OUTER_HEADER_REMOVAL,
    GTP5G_PDR_FAR_ID,

    /* Not in 3GPP spec, just used for routing */
    GTP5G_PDR_ROLE_ADDR_IPV4,

    /* Not in 3GPP spec, just used for buffering */
    GTP5G_PDR_UNIX_SOCKET_PATH,

    GTP5G_PDR_QER_ID,

    GTP5G_PDR_SEID,
    GTP5G_PDR_URR_ID,
    /* Add newly supported feature ON ABOVE
     * for compatability with older version of
     * free5GC's UPF or libgtp5gnl
     * */

    __GTP5G_PDR_ATTR_MAX,
};
#define GTP5G_PDR_ATTR_MAX (__GTP5G_PDR_ATTR_MAX - 1)

/* Nest in GTP5G_PDR_PDI */
enum gtp5g_pdi_attrs {
    GTP5G_PDI_UNSPEC,
    GTP5G_PDI_UE_ADDR_IPV4,
    GTP5G_PDI_F_TEID,
    GTP5G_PDI_SDF_FILTER,

    __GTP5G_PDI_ATTR_MAX,
};
#define GTP5G_PDI_ATTR_MAX (__GTP5G_PDI_ATTR_MAX - 1)

/* Nest in GTP5G_PDI_F_TEID */
enum gtp5g_f_teid_attrs {
    GTP5G_F_TEID_UNSPEC,
    GTP5G_F_TEID_I_TEID,
    GTP5G_F_TEID_GTPU_ADDR_IPV4,

    __GTP5G_F_TEID_ATTR_MAX,
};
#define GTP5G_F_TEID_ATTR_MAX (__GTP5G_F_TEID_ATTR_MAX - 1)

enum {
    GTP5G_SDF_FILTER_ACTION_UNSPEC = 0,

    GTP5G_SDF_FILTER_PERMIT,

    __GTP5G_SDF_FILTER_ACTION_MAX,
};
#define GTP5G_SDF_FILTER_ACTION_MAX (__GTP5G_SDF_FILTER_ACTION_MAX - 1)

enum {
    GTP5G_SDF_FILTER_DIRECTION_UNSPEC = 0,

    GTP5G_SDF_FILTER_IN,
    GTP5G_SDF_FILTER_OUT,

    __GTP5G_SDF_FILTER_DIRECTION_MAX,
};
#define GTP5G_SDF_FILTER_DIRECTION_MAX (__GTP5G_SDF_FILTER_DIRECTION_MAX - 1)
/* ------------------------------------------------------------------
 *								FAR
 * ------------------------------------------------------------------
 * */
enum gtp5g_far_attrs {
    /* gtp5g_device_attrs in this part */

    GTP5G_FAR_ID = 3,
    GTP5G_FAR_APPLY_ACTION,
    GTP5G_FAR_FORWARDING_PARAMETER,

    /* Not IEs in 3GPP Spec, for other purpose */
    GTP5G_FAR_RELATED_TO_PDR,

    GTP5G_FAR_SEID,
    GTP5G_FAR_BAR_ID,
    __GTP5G_FAR_ATTR_MAX,
};
#define GTP5G_FAR_ATTR_MAX (__GTP5G_FAR_ATTR_MAX - 1)

#define FAR_ACTION_UPSPEC 0x00
#define FAR_ACTION_DROP   0x01
#define FAR_ACTION_FORW   0x02
#define FAR_ACTION_BUFF   0x04
#define FAR_ACTION_MASK   0x07
#define FAR_ACTION_NOCP   0x08
#define FAR_ACTION_DUPL   0x10

/* ------------------------------------------------------------------
 *								QER
 * ------------------------------------------------------------------
 * */
enum gtp5g_qer_attrs {
    /* gtp5g_device_attrs in this part */

    GTP5G_QER_ID = 3,
    GTP5G_QER_GATE,
    GTP5G_QER_MBR,
    GTP5G_QER_GBR,
    GTP5G_QER_CORR_ID,
    GTP5G_QER_RQI,
    GTP5G_QER_QFI,
    GTP5G_QER_PPI,
    GTP5G_QER_RCSR,
	

    /* Not IEs in 3GPP Spec, for other purpose */
    GTP5G_QER_RELATED_TO_PDR,

    GTP5G_QER_SEID,
    __GTP5G_QER_ATTR_MAX,
};
#define GTP5G_QER_ATTR_MAX (__GTP5G_QER_ATTR_MAX - 1)

/* Nest in GTP5G_QER_MBR */
enum gtp5g_mbr_attrs {
    GTP5G_QER_MBR_UL_HIGH32 = 1,
    GTP5G_QER_MBR_UL_LOW8,
    GTP5G_QER_MBR_DL_HIGH32,
    GTP5G_QER_MBR_DL_LOW8,

    __GTP5G_QER_MBR_ATTR_MAX,
};
#define GTP5G_QER_MBR_ATTR_MAX (__GTP5G_QER_MBR_ATTR_MAX - 1)

/* Nest in GTP5G_QER_QBR */
enum gtp5g_qer_gbr_attrs {
    GTP5G_QER_GBR_UL_HIGH32 = 1,
    GTP5G_QER_GBR_UL_LOW8,
    GTP5G_QER_GBR_DL_HIGH32,
    GTP5G_QER_GBR_DL_LOW8,

    __GTP5G_QER_GBR_ATTR_MAX,
};
#define GTP5G_QER_GBR_ATTR_MAX (__GTP5G_QER_GBR_ATTR_MAX - 1)

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