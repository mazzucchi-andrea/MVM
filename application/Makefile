all:
	gcc -c prog.c -mavx -mavx2 -mavx512f -funroll-loops -pie -fPIE -I ../include -DALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) -DMOD=$(MOD)