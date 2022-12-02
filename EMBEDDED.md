Using PAPPL on Embedded Linux
=============================

PAPPL has been developed for use on embedded Linux as well as traditional
desktop/server environments.


Yocto Recipe
------------

The [recipes-pappl](https://github.com/michaelrsweet/recipes-pappl) project
provides a Yocto recipe for the current stable release of PAPPL.  You can add
it to your meta layer with:

```
git submodule add https://github.com/michaelrsweet/recipes-pappl.git
```


USB Printer Gadget Kernel Patch
-------------------------------

Until Linux kernel 5.12, there is a bug in the USB printer gadget functional
driver (the one that uses configfs) that can prevent the gadget from working
due to a lack of buffers with the default configuration.  Since udev is so
helpful in keeping kernel drivers loaded, it can become impossible to unload
the `f_printer` module in order to get a new configuration of the `q_len`
parameter applied.

The following kernel patch applies cleanly to 4.19 through 5.11, was submitted
to (and approved by) the Linux USB kernel developers, and is included in Linux
kernel 5.12 and later.  You'll need to use this patch if you want to use PAPPL
to provide a USB printer interface to your project with an version of Linux
kernel prior to 5.12.

```
diff --git a/drivers/usb/gadget/function/f_printer.c b/drivers/usb/gadget/function/f_printer.c
index 9c7ed2539ff7..4f3161005e4f 100644
--- a/drivers/usb/gadget/function/f_printer.c
+++ b/drivers/usb/gadget/function/f_printer.c
@@ -50,6 +50,8 @@
#define GET_PORT_STATUS		1
#define SOFT_RESET		2

+#define DEFAULT_Q_LEN		10 /* same as legacy g_printer gadget */
+
static int major, minors;
static struct class *usb_gadget_class;
static DEFINE_IDA(printer_ida);
@@ -1317,6 +1319,9 @@ static struct usb_function_instance *gprinter_alloc_inst(void)
	opts->func_inst.free_func_inst = gprinter_free_inst;
	ret = &opts->func_inst;

+	/* Make sure q_len is initialized, otherwise the bound device can't support read/write! */
+	opts->q_len = DEFAULT_Q_LEN;
+
	mutex_lock(&printer_ida_lock);

	if (ida_is_empty(&printer_ida)) {

```
