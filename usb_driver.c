#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include "commands.h"
#include "common.h"
#include "scsi_codes.h"



struct usb_device *device;
EXPORT_SYMBOL(device);

static struct usb_class_driver class;


static int usb_open(struct inode *inode, struct file *f)
{
    return 0;
}

static int usb_close(struct inode *inode, struct file *f)
{
    return 0;
}

static ssize_t usb_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
    int retval;
    unsigned char read_buf[MAX_PKT_SIZE] = {0, };

    retval = MassStore_ReadDeviceBlock(BULK_ONLY_DEFAULT_LUN_NUMBER, DEFAULT_TEST_BLOCK, 1, MAX_PKT_SIZE, read_buf);
    if(retval > 0)
    {
        printk("ERROR: Read func!");
        return -EFAULT;
    }
    if(copy_to_user(buf, read_buf, MIN(cnt, MAX_PKT_SIZE)))
    {
        return -EFAULT;
    }
    return MIN(cnt, MAX_PKT_SIZE);
}


static ssize_t usb_write(struct file *f, const char __user *buf, size_t cnt, loff_t *off)
{
    int ret_val;
    unsigned char write_buf[MAX_PKT_SIZE] = {0, };
    if(copy_from_user(write_buf, buf, MIN(cnt, MAX_PKT_SIZE)))
    {
        printk(KERN_ERR "Cannot copy from user \n");
        return -EFAULT;
    }

    ret_val = MassStore_WriteDeviceBlock(BULK_ONLY_DEFAULT_LUN_NUMBER, DEFAULT_TEST_BLOCK, 1, MAX_PKT_SIZE, write_buf);
    if(ret_val > 0)
    {
        printk("ERROR: Write func!");
        return -EFAULT;
    }
    return MIN(cnt, MAX_PKT_SIZE);
}

static struct file_operations fops =
{
    .open = usb_open,
    .release = usb_close,
    .read = usb_read,
    .write = usb_write,
};

static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{

    //printk(KERN_INFO "USB drive (%04X:%04X) plugged\n", id->idVendor, id->idProduct);

    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;

    SCSI_Capacity_t capacity;
    SCSI_Inquiry_Response_t req_sense_res;
    int ret_val = 0;

    iface_desc = interface->cur_altsetting;
    printk(KERN_INFO "USB i/f %d is now probed (%04X:%04X) \n", iface_desc->desc.bInterfaceNumber, id->idVendor, id->idProduct);

    printk(KERN_INFO "ID->bNumberEndpoints: %02X\n",
            iface_desc->desc.bNumEndpoints);
    printk(KERN_INFO "ID->bInterfaceClass: %02X\n",
            iface_desc->desc.bInterfaceClass);

     for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)

    {

        endpoint = &iface_desc->endpoint[i].desc;

        printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n",

                i, endpoint->bEndpointAddress);

        printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n",

                i, endpoint->bmAttributes);

        printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n",

                i, endpoint->wMaxPacketSize, endpoint->wMaxPacketSize);

    }

    device = interface_to_usbdev(interface);

    class.name = "usb/myusb%d";
    class.fops = &fops;
    if(usb_register_dev(interface, &class) < 0)
    {
        printk(KERN_ERR "Not able to get minor for this device \n");

    }
    else
    {
        printk(KERN_INFO "Minor obtained: %d\n", interface->minor);
    }

    //reset device for SCSI Command
    if(usb_reset_device(device) < 0)
    {
        printk(KERN_ERR "Cannot reset device \n");
    }

    printk("Reset USB completed \n");
    // Test UNIT Ready


    if(!usb_interface_claimed(interface))
    {
        printk("Interface is not claimed -> Exit\n");
        return -EFAULT;
    }
    ret_val = MassStore_Inquiry( BULK_ONLY_DEFAULT_LUN_NUMBER, &req_sense_res );
    if(ret_val > 0)
    {
        printk("MassStore_Inquiry Fail\n");
        return -1;
    }
    else
    {


        ret_val=MassStore_TestUnitReady(BULK_ONLY_DEFAULT_LUN_NUMBER);
        if(ret_val > 0)
        {
            printk("Mass Storage Not Ready. Do a Request Sense.\n");
            return -1;
        }
        printk("Mass Storage Ready.\n");
        ret_val=MassStore_ReadCapacity( BULK_ONLY_DEFAULT_LUN_NUMBER, &capacity);
        if(ret_val > 0)
        {
            printk("Mass Storage Read Capacity failed.\n");
            return -1;
        }
        else
        {
            printk("\nMass Storage Read Capacity success.\n");
            printk("No of Blocks: 0x%04X, %d\n",capacity.Blocks, capacity.Blocks);
            printk("BlockSize: 0x%04X, %d bytes\n",capacity.BlockSize, capacity.BlockSize);
        }
    }
    return 0;
}
 


static void usb_disconnect(struct usb_interface *interface)
{
    printk(KERN_INFO "USB i/f %d now disconnected\n",

            interface->cur_altsetting->desc.bInterfaceNumber);
    //printk(KERN_INFO "USB drive removed\n");
    usb_deregister_dev(interface, &class);

}
 
static struct usb_device_id usb_table[] =
{
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, usb_table);
 
static struct usb_driver my_usb_driver =
{
    .name = "usb_driver",
    .id_table = usb_table,
    .probe = usb_probe,
    .disconnect = usb_disconnect,
};
 
static int __init usb_init(void)
{
    return usb_register(&my_usb_driver);
}
 
static void __exit usb_exit(void)
{
    usb_deregister(&my_usb_driver);
}
 
module_init(usb_init);
module_exit(usb_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter-vdthe");
MODULE_DESCRIPTION("USB Storage Registration Driver");