#ifndef GENL_FAR_H__
#define GENL_FAR_H__

#include "genl.h"

// /* FAR attributes */
// enum gtp5g_far_attrs {
//     /* gtp5g_device_attrs in this part */

//     GTP5G_FAR_ID = 3,
//     GTP5G_FAR_APPLY_ACTION,
//     GTP5G_FAR_FORWARDING_PARAMETER,

//     /* Not IEs in 3GPP Spec, for other purpose */
//     GTP5G_FAR_RELATED_TO_PDR,

//     GTP5G_FAR_SEID,
//     GTP5G_FAR_BAR_ID,
//     __GTP5G_FAR_ATTR_MAX,
// };
// #define GTP5G_FAR_ATTR_MAX (__GTP5G_FAR_ATTR_MAX - 1)

// /* FAR.Forwarding Parameter attributes */
// /* Nest in GTP5G_FAR_FORWARDING_PARAMETER */
// enum gtp5g_forwarding_parameter_attrs {
//     GTP5G_FORWARDING_PARAMETER_OUTER_HEADER_CREATION = 1,
//     GTP5G_FORWARDING_PARAMETER_FORWARDING_POLICY,

//     __GTP5G_FORWARDING_PARAMETER_ATTR_MAX,
// };
// #define GTP5G_FORWARDING_PARAMETER_ATTR_MAX (__GTP5G_FORWARDING_PARAMETER_ATTR_MAX - 1)

// /* FAR.Forwarding Parameter.Outer Header Creation attributes */
// /* Nest in GTP5G_FORWARDING_PARAMETER_OUTER_HEADER_CREATION */
// enum gtp5g_outer_header_creation_attrs {
//     GTP5G_OUTER_HEADER_CREATION_DESCRIPTION = 1,
//     GTP5G_OUTER_HEADER_CREATION_O_TEID,
//     GTP5G_OUTER_HEADER_CREATION_PEER_ADDR_IPV4,
//     GTP5G_OUTER_HEADER_CREATION_PORT,

//     __GTP5G_OUTER_HEADER_CREATION_ATTR_MAX,
// };
// #define GTP5G_OUTER_HEADER_CREATION_ATTR_MAX (__GTP5G_OUTER_HEADER_CREATION_ATTR_MAX - 1)

// #define FAR_ACTION_UPSPEC 0x00
// #define FAR_ACTION_DROP   0x01
// #define FAR_ACTION_FORW   0x02
// #define FAR_ACTION_BUFF   0x04
// #define FAR_ACTION_MASK   0x07
// #define FAR_ACTION_NOCP   0x08
// #define FAR_ACTION_DUPL   0x10

/* for kernel */
// extern int upf_genl_add_far(struct sk_buff *, struct genl_info *);
// extern int upf_genl_del_far(struct sk_buff *, struct genl_info *);
// extern int upf_genl_get_far(struct sk_buff *, struct genl_info *);
// extern int upf_genl_dump_far(struct sk_buff *, struct netlink_callback *);

#endif
