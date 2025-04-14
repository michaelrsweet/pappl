Using PAPPL on Embedded Linux
=============================

PAPPL has been developed for use on embedded Linux as well as traditional
desktop/server environments.  Beyond the reduced resource usage, PAPPL also
supports providing USB access to a printer via the Linux USB gadget driver.


Minimum Hardware Requirements
-----------------------------

Based on my own testing, the bare minimum hardware requirements for an embedded
Linux solution are on the order of a 150MHz 32-bit ARM CPU with 64MB RAM.

I regularly test PAPPL on a [Raspberry Pi][RP] Zero W which has fairly generous
resources for an embedded controller with a 1GHz 32-bit ARM CPU and 512MB RAM.
The Raspberry Pi Zero 2, 2A/B, 3A/B, 4B, 400, 5B, 500, and compute modules all
work, however the Raspberry Pi Pico products lack sufficient memory (only 264 to
520k) to run PAPPL.

All of the [BeagleBoard][BB], [Banana Pi][BP], [Orange Pi][OP], and
[Rock Pi][RK] boards will work, and [Microchip][MC] has a complete range of
32-bit ARM and 64-bit RISC-V microprocessors (dev kits available to play around
with them) that will likewise all work.

None of the current Arduino boards have sufficient memory to host PAPPL.


[BB]: https://www.beagleboard.org/
[BP]: https://www.banana-pi.org/
[MC]: https://www.microchip.com/en-us/products/microprocessors
[OP]: https://www.orangepi.org/
[RK]: https://www.rockpi.org/
[RP]: https://www.raspberrypi.com/


RaspbianOS
----------

The Raspberry Pi Zero, Zero2, 4B, and 5 single board computers all support USB
gadget mode.  To develop using the stock RaspbianOS distribution you'll need to
make some configuration changes:

1. In "/boot/config.txt", comment out the line reading `otg_mode=1` and add a
   line at the end reading `dtoverlay=dwc2`.
2. In "/boot/cmdline.txt", add `modules-load=dwc2,libcomposite` between the
   `rootwait` and `quiet` options.
3. Run `systemctl disable usb-gadget.target` to disable systemd's incomplete,
   incompatible, and barely documented [USB gadget support][SYSTEMD].
4. Reboot.

[SYSTEMD]: https://github.com/systemd/systemd/issues/32250

I also find it useful to enable ssh by creating an empty file named "/boot/ssh".

To try gadget mode with the PAPPL test suite, connect your computer to the
Raspberry Pi's USB Micro B data port (Zero/Zero2) or USB-C data/power port,
build the PAPPL software, and run the following command:

    sudo testsuite/testpappl -U -c -1 -L debug -l -

The `-U` option enables USB gadget mode.  Once the program is running, your
computer will see a composite USB device that offers a legacy USB printer
interface (7-1-2) and three IPP-USB (7-1-4) interfaces.

You can enable additional gadgets with various `--usb-xxx` options:

- `--usb-ethernet`: Enable an Ethernet gadget.
- `--usb-product-id PRODUCT-ID`: Set the USB product ID - default is 0x8011.
- `--usb-readonly DISK-IMAGE`: Enable the storage gadget with a read-only disk
  image.
- `--usb-removable DISK-IMAGE`: Enable the storage gadget with a read-write and
  removable disk image.
- `--usb-serial`: Enable a serial gadget.
- `--usb-storage DISK-IMAGE`: Enable the storage gadget with a read-write disk
  image.
- `--usb-vendor-id VENDOR-ID`: Set the USB vendor ID - default is 0x1209.


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
