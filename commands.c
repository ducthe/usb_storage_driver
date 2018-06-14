#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include "commands.h"
#include "common.h"
#include "scsi_codes.h"
/* Tag value */
static uint32_t MassStore_Tag = 1;
extern struct usb_device *device;

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
} 
