#include <linux/module.h>

#include "genl.h"
#include "link.h"
#include "net.h"

#define DRV_VERSION "0.5.4"

static int __init gtp5g_init(void)
{
    int err;

    // GTP5G_LOG(NULL, "Gtp5g Module initialization Ver: %s\n", DRV_VERSION);

    // INIT_LIST_HEAD(&proc_gtp5g_dev);

    // get_random_bytes(&gtp5g_h_initval, sizeof(gtp5g_h_initval));

    err = rtnl_link_register(&gtp5g_link_ops);
    if (err < 0) {
        // GTP5G_ERR(NULL, "Failed to register rtnl\n");
        goto error_out;
    }

    err = genl_register_family(&gtp5g_genl_family);
    if (err < 0) {
        // GTP5G_ERR(NULL, "Failed to register generic\n");
        goto unreg_rtnl_link;
    }

    err = register_pernet_subsys(&gtp5g_net_ops);
    if (err < 0) {
        // GTP5G_ERR(NULL, "Failed to register namespace\n");
        goto unreg_genl_family;
    }

    // proc_gtp5g = proc_mkdir("gtp5g", NULL);
    // if (!proc_gtp5g) {
    //     GTP5G_ERR(NULL, "Failed to create /proc/gtp5g\n");
    //     goto unreg_pernet;
    // }

    // proc_gtp5g_dbg = proc_create("dbg", (S_IFREG | S_IRUGO | S_IWUGO),
    //     proc_gtp5g, &proc_gtp5g_dbg_ops);
    // if (!proc_gtp5g_dbg) {
    //     GTP5G_ERR(NULL, "Failed to create /proc/gtp5g/dbg\n");
    //     goto remove_gtp5g_proc;
    // }

    // proc_gtp5g_pdr = proc_create("pdr", (S_IFREG | S_IRUGO | S_IWUGO),
    //     proc_gtp5g, &proc_gtp5g_pdr_ops);
    // if (!proc_gtp5g_pdr) {
    //     GTP5G_ERR(NULL, "Failed to create /proc/gtp5g/pdr\n");
    //     goto remove_dbg_proc;
    // }

    // proc_gtp5g_far = proc_create("far", (S_IFREG | S_IRUGO | S_IWUGO),
    //     proc_gtp5g, &proc_gtp5g_far_ops);
    // if (!proc_gtp5g_far) {
    //     GTP5G_ERR(NULL, "Failed to create /proc/gtp5g/far\n");
    //     goto remove_pdr_proc;
    // }

    // proc_gtp5g_qer = proc_create("qer", (S_IFREG | S_IRUGO | S_IWUGO), 
    //     proc_gtp5g, &proc_gtp5g_qer_ops);
    // if (!proc_gtp5g_qer) {
    //     GTP5G_ERR(NULL, "Failed to create /proc/gtp5g/qer\n");
    //     goto remove_far_proc;
    // }

    // GTP5G_LOG(NULL, "5G GTP module loaded\n");

    return 0;
// remove_far_proc:
//     remove_proc_entry("far", proc_gtp5g);
// remove_pdr_proc:
//     remove_proc_entry("pdr", proc_gtp5g);
// remove_dbg_proc:
//     remove_proc_entry("dbg", proc_gtp5g);
// remove_gtp5g_proc:
//     remove_proc_entry("gtp5g", NULL);
// unreg_pernet:
//     unregister_pernet_subsys(&gtp5g_net_ops);
unreg_genl_family:
    genl_unregister_family(&gtp5g_genl_family);
unreg_rtnl_link:
    rtnl_link_unregister(&gtp5g_link_ops);
error_out:
    return err;
}

static void __exit gtp5g_fini(void)
{
    printk("<%s: %d> start\n", __func__, __LINE__);

    genl_unregister_family(&gtp5g_genl_family);
    rtnl_link_unregister(&gtp5g_link_ops);
    unregister_pernet_subsys(&gtp5g_net_ops);

    // remove_proc_entry("qer", proc_gtp5g);
    // remove_proc_entry("far", proc_gtp5g);
    // remove_proc_entry("pdr", proc_gtp5g);
    // remove_proc_entry("dbg", proc_gtp5g);
    // remove_proc_entry("gtp5g", NULL);

    // GTP5G_LOG(NULL, "5G GTP module unloaded\n");
}

late_initcall(gtp5g_init);
module_exit(gtp5g_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yao-Wen Chang <yaowenowo@gmail.com>");
MODULE_AUTHOR("Muthuraman <muthuramane.cs03g@g2.nctu.edu.tw>");
MODULE_DESCRIPTION("Interface for 5G GTP encapsulated traffic");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS_RTNL_LINK("gtp5g");
MODULE_ALIAS_GENL_FAMILY("gtp5g");