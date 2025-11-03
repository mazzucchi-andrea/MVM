ALLOCATOR_AREA_SIZE=0x200000UL
MOD=64

all:
	gcc -c prog.c -mavx -mavx2 -mavx512f -pie -fPIE -I ../include -DALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) -DMOD=$(MOD)