#include <linux/rculist.h>

#include "common.h"
#include "dev.h"
#include "urr.h"
#include "pdr.h"
#include "far.h"
#include "seid.h"
#include "hash.h"
#include "log.h"

void seid_urr_id_to_hex_str(u64 seid_int, u32 urr_id, char *buff)
{
    seid_and_u32id_to_hex_str(seid_int, urr_id, buff);
}

static void urr_context_free(struct rcu_head *head)
{
    struct urr *urr = container_of(head, struct urr, rcu_head);
    if (!urr)
        return;

    kfree(urr);
}

void urr_context_delete(struct urr *urr)
{
    struct gtp5g_dev *gtp = netdev_priv(urr->dev);
    struct hlist_head *head;
    struct pdr_node *pdr_node;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    if (!urr)
        return;

    if (!hlist_unhashed(&urr->hlist_id))
        hlist_del_rcu(&urr->hlist_id);

    seid_urr_id_to_hex_str(urr->seid, urr->id, seid_urr_id_hexstr);
    head = &gtp->related_urr_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];
    hlist_for_each_entry_rcu(pdr_node, head, hlist) {
        if (pdr_node->pdr != NULL &&
            find_urr_id_in_pdr(pdr_node->pdr, urr->id)) {
            unix_sock_client_delete(pdr_node->pdr);
        }
    }

    call_rcu(&urr->rcu_head, urr_context_free);
}

struct urr *find_urr_by_id(struct gtp5g_dev *gtp, u64 seid, u32 urr_id)
{
    struct hlist_head *head;
    struct urr *urr;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};

    seid_urr_id_to_hex_str(seid, urr_id, seid_urr_id_hexstr);
    head = &gtp->urr_id_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];
    hlist_for_each_entry_rcu(urr, head, hlist_id) {
        if (urr->seid == seid && urr->id == urr_id)
            return urr;
    }

    return NULL;
}

void urr_update(struct urr *urr, struct gtp5g_dev *gtp)
{
    struct pdr_node *pdr_node;
    struct hlist_head *head;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};

    seid_urr_id_to_hex_str(urr->seid, urr->id, seid_urr_id_hexstr);
    head = &gtp->related_urr_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];
    hlist_for_each_entry_rcu(pdr_node, head, hlist) {
        if (pdr_node->pdr != NULL &&
            find_urr_id_in_pdr(pdr_node->pdr, urr->id)) {
            unix_sock_client_update(pdr_node->pdr, rcu_dereference(pdr_node->pdr->far));
        }
    }
}

// TODO: FAR ID for Quota Action IE for indicating the action while no quota is granted
void urr_quota_exhaust_action(struct urr *urr, struct gtp5g_dev *gtp)
{
    struct hlist_head *head;
    struct pdr_node *pdr_node;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    u16 *actions = NULL, *pdrids = NULL;
    struct far *far;

    if (urr->quota_exhausted) {
        GTP5G_WAR(NULL, "URR (%u) quota was already exhausted\n", urr->id);
        return;
    }

    // urr stop measurement
    urr->pdr_num = 0;

    pdrids = kzalloc(MAX_PDR_PER_SESSION * sizeof(u16), GFP_KERNEL);
    actions = kzalloc(MAX_PDR_PER_SESSION * sizeof(u16), GFP_KERNEL);
    if (!pdrids || !actions)
        goto fail;

    seid_urr_id_to_hex_str(urr->seid, urr->id, seid_urr_id_hexstr);
    head = &gtp->related_urr_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];
    //each pdr that associate with the urr drop pkt
    hlist_for_each_entry_rcu(pdr_node, head, hlist) {
        if (find_urr_id_in_pdr(pdr_node->pdr, urr->id)) {
            pdrids[urr->pdr_num] = pdr_node->pdr->id;
            far = rcu_dereference(pdr_node->pdr->far);
            if (far != NULL) {
                actions[urr->pdr_num++] = far->action;

                far->action = FAR_ACTION_DROP;
            }
        }
    }

    if (urr->pdr_num > 0) {
        urr->pdrids = kzalloc(urr->pdr_num * sizeof(u16), GFP_KERNEL);
        urr->actions = kzalloc(urr->pdr_num * sizeof(u16), GFP_KERNEL);

        memcpy(urr->pdrids, pdrids, urr->pdr_num * sizeof(u16));
        memcpy(urr->actions, actions, urr->pdr_num * sizeof(u16));
    }

    urr->info |= URR_INFO_INAM;
    urr->quota_exhausted = true;

fail:
    if (pdrids)
        kfree(pdrids);
    if (actions)
        kfree(actions);
}

void urr_reverse_quota_exhaust_action(struct urr *urr, struct gtp5g_dev *gtp)
{
    struct hlist_head *head;
    struct pdr_node *pdr_node;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    int i;
    struct far *far;

    if (!urr->quota_exhausted) {
        GTP5G_WAR(NULL, "URR (%u) quota is not exhausted; should not reverse\n", urr->id);
        return;
    }

    seid_urr_id_to_hex_str(urr->seid, urr->id, seid_urr_id_hexstr);
    head = &gtp->related_urr_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];

    //each pdr that associate with the urr resume it's normal action
    hlist_for_each_entry_rcu(pdr_node, head, hlist) {
        if (find_urr_id_in_pdr(pdr_node->pdr, urr->id)) {
            for (i = 0; i < urr->pdr_num; i++) {
                if (urr->pdrids[i] == pdr_node->pdr->id) {
                    far = rcu_dereference(pdr_node->pdr->far);
                    if (far != NULL) {
                        far->action = urr->actions[i];
                    }
                }
            }
        }
    }

    urr->quota_exhausted = false;

    if (urr->pdrids)
        kfree(urr->pdrids);
    if (urr->actions)
        kfree(urr->actions);
}

void urr_append(u64 seid, u32 urr_id, struct urr *urr, struct gtp5g_dev *gtp)
{
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    u32 i;

    seid_urr_id_to_hex_str(seid, urr_id, seid_urr_id_hexstr);
    i = str_hashfn(seid_urr_id_hexstr) % gtp->hash_size;
    hlist_add_head_rcu(&urr->hlist_id, &gtp->urr_id_hash[i]);
}

int urr_get_pdr_ids(u16 *ids, int n, struct urr *urr, struct gtp5g_dev *gtp)
{
    struct hlist_head *head;
    struct pdr_node *pdr_node;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    int i;

    seid_urr_id_to_hex_str(urr->seid, urr->id, seid_urr_id_hexstr);
    head = &gtp->related_urr_hash[str_hashfn(seid_urr_id_hexstr) % gtp->hash_size];
    i = 0;
    hlist_for_each_entry_rcu(pdr_node, head, hlist) {
        if (i >= n)
            break;
        if (pdr_node->pdr != NULL && find_urr_id_in_pdr(pdr_node->pdr, urr->id)) {
            ids[i++] = pdr_node->pdr->id;
        }
    }
    return i;
}

void del_related_urr_hash(struct gtp5g_dev *gtp, struct pdr *pdr)
{
    u32 i, j;
    struct pdr_node *pdr_node = NULL ;
    struct pdr_node *to_be_del = NULL ;
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};

    for (j = 0; j < pdr->urr_num; j++) {
        to_be_del = NULL;
        seid_urr_id_to_hex_str(pdr->seid, pdr->urr_ids[j], seid_urr_id_hexstr);
        i = str_hashfn(seid_urr_id_hexstr) % gtp->hash_size;
        hlist_for_each_entry_rcu(pdr_node, &gtp->related_urr_hash[i], hlist) {
            if (pdr_node->pdr != NULL &&
                pdr_node->pdr->seid == pdr->seid &&
                pdr_node->pdr->id == pdr->id) {
                to_be_del = pdr_node;
                break;
            }
        }
        if (to_be_del){
            hlist_del(&to_be_del->hlist);
            kfree(to_be_del);
        }
    }
}

int urr_set_pdr(struct pdr *pdr, struct gtp5g_dev *gtp)
{
    char seid_urr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};
    u32 i, j;
    struct pdr_node *pdr_node = NULL;

    if (!pdr)
        return -1;

    // clean old pdr_node
    del_related_urr_hash(gtp, pdr);

    for (j = 0; j < pdr->urr_num; j++) {
        seid_urr_id_to_hex_str(pdr->seid, pdr->urr_ids[j], seid_urr_id_hexstr);
        i = str_hashfn(seid_urr_id_hexstr) % gtp->hash_size;

        pdr_node = kzalloc(sizeof(*pdr_node), GFP_ATOMIC);
        if (!pdr_node) {
            return -ENOMEM;
        }
        pdr_node->pdr = pdr;
        hlist_add_head_rcu(&pdr_node->hlist, &gtp->related_urr_hash[i]);
    }
    return 0;
}

/* 
 * Update the accumulated counter with the current counter.
 * This function is called when the counter is changed.
 */
void urr_update_accumulated(struct VolumeMeasurement *accumulated, 
    struct VolumeMeasurement *bytes)
{
    accumulated->totalVolume += bytes->totalVolume;
    accumulated->uplinkVolume += bytes->uplinkVolume;
    accumulated->downlinkVolume += bytes->downlinkVolume;
    accumulated->totalPktNum += bytes->totalPktNum;
    accumulated->uplinkPktNum += bytes->uplinkPktNum;
    accumulated->downlinkPktNum += bytes->downlinkPktNum;
}

// Helper function to determine which counter to use based on the period
static struct VolumeMeasurement *select_counter(struct urr *urr, u32 current_period) 
{
    return (current_period == 1) ? &urr->bytes2 : &urr->bytes;
}

// Helper function to check if counter needs to be accumulated
static bool should_accumulate_counter(u32 now, u32 last_update, 
                                    u32 last_counter, u32 current_counter,
                                    u32 period_for_counter_change) 
{
    
    // the case that use the smae counter => A B A but B no packets/get report comes
    bool time_to_change = (now - last_update >  period_for_counter_change && 
                          last_counter == current_counter);

    // the case is every interval has packets/get report comes
    bool counter_changed = (last_counter != current_counter);
    
    return time_to_change || counter_changed;
}

struct VolumeMeasurement *get_and_update_usage_report_counter(struct urr *urr)
{
 
    const u8 NUM_OF_COUNTERS = 2;
    const u32 PERIOD_FOR_COUNTER_CHANGE = 1;
    
    u32 now = ktime_get_real() / NSEC_PER_SEC;
    u32 current_counter = (now / PERIOD_FOR_COUNTER_CHANGE) % NUM_OF_COUNTERS;
    u32 last_counter = (urr->last_update / PERIOD_FOR_COUNTER_CHANGE) % NUM_OF_COUNTERS;

    // Select which counter to return and which to accumulate
    struct VolumeMeasurement *counter_to_return = select_counter(urr, current_counter);
    struct VolumeMeasurement *counter_to_accumulate = select_counter(urr, !current_counter);

    // If period is zero, always use the first counter
    if (urr->period == 0) {
        return &urr->bytes;
    }

    // Update the accumulated counters if needed
    if (should_accumulate_counter(now, urr->last_update, last_counter, current_counter, PERIOD_FOR_COUNTER_CHANGE)) {
        urr_update_accumulated(&urr->accumulated, counter_to_accumulate);
        memset(counter_to_accumulate, 0, sizeof(struct VolumeMeasurement));
    }

    urr->last_update = now;
    return counter_to_return;
}