#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>

static int __init varlink_init(void)
{
	pr_info("initialized\n");
	return 0;
}

static void __exit varlink_exit(void)
{
}

module_init(varlink_init);
module_exit(varlink_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Varlink Protocol Driver");
