#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include "commands.h"
#include "common.h"
#include "scsi_codes.h"



struct usb_device *device;
EXPORT_SYMBOL(device);

static struct usb_class_driver class;
static unsigned char bulk_buf[MAX_PKT_SIZE];
/* for SCSI command */
//static uint32_t MassStore_Tag = 1;

#if 0
uint8_t flash_drive_send_data(unsigned char *data_ptr, int no_of_bytes)
{
    int ret_val;
    int actual_length=123;  //garbage value

    // transfer the data to ENDPOINT OUT using USB Bulk Transfer.
    ret_val = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT),
                            data_ptr, no_of_bytes, &actual_length, 5000);   
    // The if condition checks whether ret_val equals 0 (libusb SUCCESS) and 
    // Number of bytes to send is equal to the number of bytes which are actually sent.
    if((ret_val == 0) && (actual_length == no_of_bytes))
    {
        printk("\nSend Complete. %d  %d\n",no_of_bytes, actual_length); 

    }
    else
    {       
        printk("\nSend Failure. %d\n",ret_val);
    }
    
    // if success then returns zero.
    return (uint8_t)(-ret_val);
}

uint8_t flash_drive_receive_data(unsigned char *data_ptr, int no_of_bytes, int *no_of_actually_received_bytes)
{
    int ret_val = 0;

    ret_val = usb_bulk_msg(device, usb_rcvbulkpipe(device, BULK_EP_IN),
                             data_ptr, no_of_bytes, no_of_actually_received_bytes, 5000); 
    if(ret_val==-9)
    {

        printk("PIPE HALT or PIPE STALL.  ERROR: -9\n");

    }
    else if(ret_val<0)  
    {   

        printk("\nReceive Failure.  ERROR: %d \n",ret_val);

    }
    else
    {

        printk("\nReceive Complete. \tReceivedBytes:%d\tActuallyReceivedBytes:%d\n",no_of_bytes,*no_of_actually_received_bytes);

    }

    return (uint8_t)(-ret_val);
} /* End flash_drive_receive_data() */




static uint8_t MassStore_SendReceiveData(CommandBlockWrapper_t* SCSICommandBlock, void* BufferPtr)
{
    uint8_t  ErrorCode;
    uint16_t BytesRem  = SCSICommandBlock->DataTransferLength;
    int actual_no_received = 0;

    // Check that direction of the SCSI command data stage
    // And take appropriate action as per the direction.
    if (SCSICommandBlock->Flags & COMMAND_DIRECTION_DATA_IN)
    {
        ErrorCode = flash_drive_receive_data(BufferPtr, BytesRem, &actual_no_received);
        if(ErrorCode > 0)
            return ErrorCode;       // return Errorcode 

        // Here we know that the received number of bytes must be BytesRem. Since it is sent in the CBW.
        if(BytesRem != actual_no_received)
            return 120;             // Error 120 --- ERROR CODE: actual_no_received not equal to desired BytesRem
    }
    else
    {
        ErrorCode = flash_drive_send_data(BufferPtr, BytesRem);
        if(ErrorCode > 0)
            return ErrorCode;
    }
    
    return 0;
}

static uint8_t MassStore_SendCommand(CommandBlockWrapper_t* SCSICommandBlock, void* BufferPtr)
{
    uint8_t ErrorCode;

    /* Each transmission should have a unique tag value, increment before use */
    SCSICommandBlock->Tag = ++MassStore_Tag;

    /* Wrap Tag value when invalid - MS class defines tag values of 0 and 0xFFFFFFFF to be invalid */
    if (MassStore_Tag == 0xFFFFFFFF)
      MassStore_Tag = 1;


    printk(KERN_INFO "Size of CommandBlockWrapper_t= %d\n",sizeof(CommandBlockWrapper_t));

    ErrorCode = flash_drive_send_data((unsigned char *)SCSICommandBlock, sizeof(CommandBlockWrapper_t));
    if(ErrorCode > 0)
    {
        printk("Error here: %d \n", __LINE__);
        return ErrorCode;
    }

    // Send data if any
    if ((BufferPtr != NULL) && ((ErrorCode = MassStore_SendReceiveData(SCSICommandBlock, BufferPtr)) != 0))
    {
        printk("Error here: %d \n", __LINE__);
        return ErrorCode;
    }
        
    return 0;
} 
static uint8_t MassStore_GetReturnedStatus(CommandStatusWrapper_t* SCSICommandStatus)
{
    uint8_t ErrorCode = 0;
    int actual_no_received = 0;

    ErrorCode = flash_drive_receive_data((unsigned char *)SCSICommandStatus, sizeof(CommandStatusWrapper_t) , &actual_no_received);
    // Success
    if((ErrorCode==0) && (actual_no_received==sizeof(CommandStatusWrapper_t)))
    {
        // Check to see if command failed
        if (SCSICommandStatus->Status != Command_Pass)
            ErrorCode = MASS_STORE_SCSI_COMMAND_FAILED;
    }
    
    // on success it returns 0 
    // on error returns error code
    return ErrorCode;
} /* End MassStore_GetReturnedStatus() */

uint8_t MassStore_Inquiry(const uint8_t LUNIndex, SCSI_Inquiry_Response_t* const InquiryPtr)
{
    uint8_t ErrorCode;

    // Create a CBW with a SCSI command to issue INQUIRY command
    CommandBlockWrapper_t SCSICommandBlock = (CommandBlockWrapper_t)
        {
            .Signature          = CBW_SIGNATURE,
            .DataTransferLength = sizeof(SCSI_Inquiry_Response_t),
            .Flags              = COMMAND_DIRECTION_DATA_IN,
            .LUN                = LUNIndex,
            .SCSICommandLength  = 6,
            .SCSICommandData    =
                {
                    SCSI_CMD_INQUIRY,
                    0x00,                               // Reserved
                    0x00,                               // Reserved
                    0x00,                               // Reserved
                    sizeof(SCSI_Inquiry_Response_t),    // Allocation Length
                    0x00                                // Unused (control)
                }
        };

    // Print the Command Block Wrapper for Inquiry
   // print_struct_CBW(&SCSICommandBlock);

    CommandStatusWrapper_t SCSICommandStatus;

    // Send the command and any data to the attached device.
    if ((ErrorCode = MassStore_SendCommand(&SCSICommandBlock, InquiryPtr)) != 0)
    {
        printk("INQUIRY send cmd failed \n");
        return ErrorCode;
    }
    
    // Retrieve status information from the attached device
    if ((ErrorCode = MassStore_GetReturnedStatus(&SCSICommandStatus)) != 0)
    {
        printk("INQUIRY get return status failed\n");
        return ErrorCode;
    }

    // Print the Command Status for Inquiry
    //print_struct_CSW(&SCSICommandStatus);

    // on success it returns 0 
    // on error returns error code
    return ErrorCode;
} /* End MassStore_Inquiry() */
uint8_t MassStore_TestUnitReady(const uint8_t LUNIndex)
{
    uint8_t ErrorCode;  

    /* Create a CBW with a SCSI command to issue TEST UNIT READY command */
    CommandBlockWrapper_t SCSICommandBlock = (CommandBlockWrapper_t)
        {
            .Signature          = CBW_SIGNATURE,
            .DataTransferLength = 0,
            .Flags              = COMMAND_DIRECTION_DATA_IN,
            .LUN                = LUNIndex,
            .SCSICommandLength  = 6,
            .SCSICommandData    =
                {
                    SCSI_CMD_TEST_UNIT_READY,
                    0x00,                   // Reserved
                    0x00,                   // Reserved
                    0x00,                   // Reserved
                    0x00,                   // Reserved
                    0x00                    // Unused (control)
                }
        };
    
    CommandStatusWrapper_t SCSICommandStatus;

    // Send the command and any data to the attached device
    if ((ErrorCode = MassStore_SendCommand(&SCSICommandBlock, NULL)) != 0)
    {
        printk("%s: send command error\n", __FUNCTION__);
        return ErrorCode;
    }
    
    // Retrieve status information from the attached device
    if ((ErrorCode = MassStore_GetReturnedStatus(&SCSICommandStatus)) != 0)
    {
        printk("%s: receive data error\n", __FUNCTION__);
        return ErrorCode;
    }
    
    // on success it returns 0 
    // on error returns error code
    return ErrorCode;
} /* End MassStore_TestUnitReady() */

uint8_t MassStore_ReadCapacity(const uint8_t LUNIndex, SCSI_Capacity_t* const CapacityPtr)
{
    uint8_t ErrorCode;

    // Create a CBW with a SCSI command to issue READ CAPACITY command
    CommandBlockWrapper_t SCSICommandBlock = (CommandBlockWrapper_t)
        {
            .Signature          = CBW_SIGNATURE,
            .DataTransferLength = sizeof(SCSI_Capacity_t),
            .Flags              = COMMAND_DIRECTION_DATA_IN,
            .LUN                = LUNIndex,
            .SCSICommandLength  = 10,
            .SCSICommandData    =
                {
                    SCSI_CMD_READ_CAPACITY_10,
                    0x00,                   // Reserved
                    0x00,                   // MSB of Logical block address
                    0x00,
                    0x00,
                    0x00,                   // LSB of Logical block address
                    0x00,                   // Reserved
                    0x00,                   // Reserved
                    0x00,                   // Partial Medium Indicator
                    0x00                    // Unused (control)
                }
        };
    
    CommandStatusWrapper_t SCSICommandStatus;

    // Send the command and any data to the attached device
    if ((ErrorCode = MassStore_SendCommand(&SCSICommandBlock, CapacityPtr)) != 0)
    {
        return ErrorCode;
    }
      
    // Endian-correct the read data
    CapacityPtr->Blocks    = SwapEndian_32(CapacityPtr->Blocks);
    CapacityPtr->BlockSize = SwapEndian_32(CapacityPtr->BlockSize);
    
    // Retrieve status information from the attached device
    if ((ErrorCode = MassStore_GetReturnedStatus(&SCSICommandStatus)) != 0)
    {
        return ErrorCode;
    }

    return ErrorCode;
} /* End MassStore_ReadCapacity() */

uint8_t MassStore_ReadDeviceBlock(const uint8_t LUNIndex, const uint32_t BlockAddress,
                                  const uint8_t Blocks, const uint16_t BlockSize, void* BufferPtr)
{
    uint8_t ErrorCode;

    // Create a CBW with a SCSI command to read in the given blocks from the device
    CommandBlockWrapper_t SCSICommandBlock = (CommandBlockWrapper_t)
        {
            .Signature          = CBW_SIGNATURE,
            .DataTransferLength = ((uint32_t)Blocks * BlockSize),
            .Flags              = COMMAND_DIRECTION_DATA_IN,
            .LUN                = LUNIndex,
            .SCSICommandLength  = 10,
            .SCSICommandData    =
                {
                    SCSI_CMD_READ_10,
                    0x00,                   // Unused (control bits, all off)
                    (BlockAddress >> 24),   // MSB of Block Address
                    (BlockAddress >> 16),
                    (BlockAddress >> 8),
                    (BlockAddress & 0xFF),  // LSB of Block Address
                    0x00,                   // Unused (reserved)
                    0x00,                   // MSB of Total Blocks to Read
                    Blocks,                 // LSB of Total Blocks to Read
                    0x00                    // Unused (control)
                }
        };
    
    CommandStatusWrapper_t SCSICommandStatus;

    // Send the command and any data to the attached device
    if ((ErrorCode = MassStore_SendCommand(&SCSICommandBlock, BufferPtr)) != 0)
    {
        return ErrorCode;
    }
    
    // Retrieve status information from the attached device
    if ((ErrorCode = MassStore_GetReturnedStatus(&SCSICommandStatus)) != 0)
    {
        return ErrorCode;
    }

    return ErrorCode;
} /* End MassStore_ReadDeviceBlock() */

uint8_t MassStore_WriteDeviceBlock(const uint8_t LUNIndex, const uint32_t BlockAddress,
                                   const uint8_t Blocks, const uint16_t BlockSize, void* BufferPtr)
{
    uint8_t ErrorCode ;

    // Create a CBW with a SCSI command to write the given blocks to the device
    CommandBlockWrapper_t SCSICommandBlock = (CommandBlockWrapper_t)
        {
            .Signature          = CBW_SIGNATURE,
            .DataTransferLength = ((uint32_t)Blocks * BlockSize),
            .Flags              = COMMAND_DIRECTION_DATA_OUT,
            .LUN                = LUNIndex,
            .SCSICommandLength  = 10,
            .SCSICommandData    =
                {
                    SCSI_CMD_WRITE_10,
                    0x00,                   // Unused (control bits, all off)
                    (BlockAddress >> 24),   // MSB of Block Address
                    (BlockAddress >> 16),
                    (BlockAddress >> 8),
                    (BlockAddress & 0xFF),  // LSB of Block Address
                    0x00,                   // Unused (reserved)
                    0x00,                   // MSB of Total Blocks to Write
                    Blocks,                 // LSB of Total Blocks to Write
                    0x00                    // Unused (control)
                }
        };
    
    CommandStatusWrapper_t SCSICommandStatus;

    // Send the command and any data to the attached device 
    if ((ErrorCode = MassStore_SendCommand(&SCSICommandBlock, BufferPtr)) != 0)
    {
        return ErrorCode;
    }
    
    // Retrieve status information from the attached device
    if ((ErrorCode = MassStore_GetReturnedStatus(&SCSICommandStatus)) != 0)
    {
        return ErrorCode;
    }

    return ErrorCode;
} /* End MassStore_WriteDeviceBlock() */
#endif
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
#if 0
    if(copy_from_user(bulk_buf, buf, MIN(cnt, MAX_PKT_SIZE)))
    {
        printk(KERN_ERR "Cannot copy from user \n");
        return -EFAULT;
    }
    printk(KERN_INFO "write data: %s \n", bulk_buf);
    retval = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT),
                            bulk_buf, MIN(cnt, MAX_PKT_SIZE), &wrote_cnt, 5000);
    if(retval)
    {
        printk(KERN_ERR "Bulk write msg returned: %d\n", retval);
        return retval;
    }

#endif
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