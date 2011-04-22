
obj-m := aes2501.o

KERNELDIR	?= /lib/modules/$(shell uname -r)/build

PWD			:= $(shell pwd)

all: usertest
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

usertest:
	gcc -I. -o usertest usertest.c

clean:
	rm -f *.o *~ aes2501.ko aes2501.mod.c Module.symvers usertest
	rm -rf .tmp_versions
	rm -f .*.cmd


