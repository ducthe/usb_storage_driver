# usb_storage_driver
Simple linux module explanation for usb storage with SCSI protocol
Pre-requisite:
    You have to ensure that your Produce ID & Vendor ID is correct by command "lsusb" -> check info in "common.h"
How to run
1. make
2. make install
3. sudo chmod 777 /dev/myusb0
4. Plug usb flash device
5. ./test_app
