#ifndef GTP5G_LINK_H__
#define GTP5G_LINK_H__

#include <net/rtnetlink.h>

/* Maybe add this part to if_link.h */
enum ifla_gtp5g_role {
    GTP5G_ROLE_UPF = 0,
    GTP5G_ROLE_RAN,
};

enum {
    IFLA_GTP5G_UNSPEC,

    IFLA_GTP5G_FD1,
    IFLA_GTP5G_PDR_HASHSIZE,
    IFLA_GTP5G_ROLE,

    __IFLA_GTP5G_MAX,
};
#define IFLA_GTP5G_MAX (__IFLA_GTP5G_MAX - 1)
/* end of part */

extern struct rtnl_link_ops gtp5g_link_ops;

#endif