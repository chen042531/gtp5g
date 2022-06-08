#ifndef ENCAP_H__
#define ENCAP_H__

#include <linux/socket.h>

#include "dev.h"

extern struct sock *gtp5g_encap_enable(int, int, struct gtp5g_dev *);
extern void gtp5g_encap_disable(struct sock *);

#endif