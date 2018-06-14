obj-m = usb.o
usb-objs := commands.o usb_driver.o

DIR = $(shell pwd)
EXTRA_CFLAGS := -I$(DIR)/

KVERSION = $(shell uname -r)

KER_DIR = /lib/modules/$(KVERSION)/build
all:
	make -C $(KER_DIR) M=$(PWD) modules
	gcc test_app.c -o test_app
clean:
	make -C $(KER_DIR) M=$(PWD) clean
	rm -rf test_app
install:
	sudo rmmod usb_storage
	sudo insmod usb.ko
