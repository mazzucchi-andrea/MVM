#APPLICATION SPECIFIC STUFF
#the below specify the directory where the user application is located, the name of the executable to be generated, the name of the object file of the program and the names of the target functions to instrument 
#IMPORTANT: a Makefile in the APP dir is responsible for generating the .o file specified by APP_OBJ
APP = $(PWD)/application
EXECUTABLE = $(APP)/prog
APP_OBJ = $(APP)/prog.o
#TARGET_MODULES= a.c  sub-dir/b.c this is an example usage
TARGET_MODULES= prog.c
#TARGET_FUNCTIONS="<a>:","<b>:" this is an example usage
TARGET_FUNCTIONS="<test_checkpoint>:","<test_checkpoint_random>:","<test_fill_area>:"
LIBS = -lm

INCLUDE = $(PWD)/include
OBJ = $(PWD)/obj
LIB = $(PWD)/lib
TEMP = $(PWD)/temp
DISASSEMBLY = $(TEMP)/disassembly
DISASSEMBLY_FILE = -Ddisassembly_file=\"$(DISASSEMBLY)\"
TEMPDIR = -Dtemp_dir=$(TEMP)
SHADOW = shadow
INTERMEDIATEFILE = intermediate.S
INTERMEDIATEOBJFILE = intermediate.o
TEMPFILE = -Dtemp_file=\"$(TEMP)/$(INTERMEDIATEFILE)\"
TEMPOBJ = -Dtemp_obj_file=\"$(TEMP)/$(INTERMEDIATEOBJFILE)\"
TF=-Dtarget_functions=\"$(TARGET_FUNCTIONS)\"

#USER DEFINED STUFF - PLEASE CHANGE WHATEVER YOU NEED
USER_DEFINED = $(PWD)/user-defined
USER_DEFINED_FILE = intermediate.S
USER_DEFINED_OBJ_FILE = intermediate.o
UDTEMPDIR = -Duser_defined_dir=\"$(USER_DEFINED)\"
UDTEMPFILE = -Duser_defined_temp_file=\"$(USER_DEFINED)/$(USER_DEFINED_FILE)\"
UDTEMPOBJ = -Duser_defined_temp_obj_file=\"$(USER_DEFINED)/$(USER_DEFINED_OBJ_FILE)\"

CC = gcc

SIMD_FLAGS = 

SECURITY_FLAGS = -pie -fPIE -fstack-protector-all -fno-toplevel-reorder -fno-reorder-functions -fno-align-functions

ADDITIONAL_FLAGS = -O3 -DCKPT -DAPPLY_PATCHES -DVERBOSE

#NOTE: 
#the ASM_PREAMBLE macro enables building a demo patch for each memory access instruction
#which passess control to a trampoline that calls the the_patch(...) fuction
#offered by the MVM package (see patches/patches.c)
#the APPLY_PATCHES macro actually leads to apply the patches thar are build by the instrumentation process
#the VERBOSE macro simply leads to the massive production of output messages 

#CKPT FLAGS
ALLOCATOR_AREA_SIZE=0x100000
MOD=8

THE_VM = -DVM_NAME=\"MVM\"

all: checks backup-files file-rewriting compile-and-link restore-files

compile-and-link: movm
	export C_INCLUDE_PATH=$(PWD)/include; cd $(APP) ; make ALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) MOD=$(MOD) ; $(CC) $(APP_OBJ) $(LIB)/movm.o -o $(EXECUTABLE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) $(LIBS) -Xlinker --wrap=main
	objdump -Dw $(EXECUTABLE) > $(DISASSEMBLY) 

checks:
	@if [ -d $(APP) ]; then echo ""; else echo "application directory does not exist" ; exit 1; fi
	@./scripts/file-existence.sh $(APP) $(TARGET_MODULES)
	@echo "PLEASE for any compiling error run 'make restore-files'\n"

	@if [ ! -d $(OBJ) ]; then mkdir -p $(OBJ); fi
	@if [ ! -d $(LIB) ]; then mkdir -p $(LIB); fi
	@if [ ! -d $(TEMP) ]; then mkdir -p $(TEMP); fi
	@if [ ! -d $(TEMP)/$(SHADOW) ]; then mkdir -p $(TEMP)/$(SHADOW); fi

base:
	./scripts/temp-dir.sh $(TEMP) $(TEMP)/$(INTERMEDIATEFILE)

head:
	cd ./src; $(CC) _head.c -c -I$(INCLUDE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -o $(OBJ)/_head.o

startup:
	cd ./src; $(CC) _early_start.c -c -I$(INCLUDE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -o $(OBJ)/_early_start.o

ckpt:
	@if [ -z "$(MOD)" ]; then \
		echo "Error: MOD is not defined"; \
		$(MAKE) restore-files; \
		exit 1; \
	fi; \
	if [ $(MOD) -lt 8 ]; then \
		echo "Error: MOD must be equal or greater than 8 (got $(MOD))"; \
		$(MAKE) restore-files; \
		exit 1; \
	fi; \
	if [ $$(($(MOD) & ($(MOD) - 1))) -ne 0 ]; then \
		echo "Error: MOD must be a power of 2 (got $(MOD))"; \
		$(MAKE) restore-files; \
		exit 1; \
	fi; \
	if [ -z "$(ALLOCATOR_AREA_SIZE)" ]; then \
	    echo "Error: ALLOCATOR_AREA_SIZE is not defined"; \
	    $(MAKE) restore-files; \
	    exit 1; \
	fi; \
	PAGE_SIZE=$$(getconf PAGESIZE); \
	if [ $$(($(ALLOCATOR_AREA_SIZE) % $$PAGE_SIZE)) -ne 0 ]; then \
	    echo "Error: ALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) is not a multiple of page size ($$PAGE_SIZE)"; \
	    $(MAKE) restore-files; \
	    exit 1; \
	fi; \
	SIZE_IN_PAGES=$$(($(ALLOCATOR_AREA_SIZE) / $$PAGE_SIZE)); \
	if [ $$(($$SIZE_IN_PAGES & ($$SIZE_IN_PAGES - 1))) -ne 0 ]; then \
	    echo "Error: ALLOCATOR_AREA_SIZE must be 2^N pages (got $$SIZE_IN_PAGES pages, not a power of 2)"; \
	    $(MAKE) restore-files; \
	    exit 1; \
	fi; \
	$(eval CPU_FLAGS := $(shell grep -m1 '^flags' /proc/cpuinfo 2>/dev/null | cut -d: -f2))
ifeq ($(MOD),16)
	$(eval SIMD_FLAGS += $(if $(findstring sse2,$(CPU_FLAGS)),-msse2 -DSSE2))
else ifeq ($(MOD),32)
	$(eval SIMD_FLAGS += $(if $(findstring avx,$(CPU_FLAGS)),-msse2 -mavx -DSSE2 -DAVX))
else ifeq ($(MOD),64)
	$(eval SIMD_FLAGS += $(if $(findstring avx512f,$(CPU_FLAGS)),-msse2 -mavx -mavx512f -DSSE2 -DAVX -DAVX512))
endif
	cd ./src; $(CC) _ckpt_setup.c -c $(SIMD_FLAGS) -I$(INCLUDE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -DALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) -DMOD=$(MOD) -o $(OBJ)/_ckpt_setup.o

asm-patch:
	cd ./src; $(CC) _asm_patches.S -c -I$(INCLUDE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -DALLOCATOR_AREA_SIZE=$(ALLOCATOR_AREA_SIZE) -DMOD=$(MOD) -o $(OBJ)/_asm_patches.o
	objdump -Dw $(OBJ)/_asm_patches.o > $(TEMP)/disassembly_asm_patch                   

patch:
	cd ./patches; $(CC) patches.c -c -I$(INCLUDE) $(THE_VM) $(UDTEMPDIR) $(UDTEMPFILE) $(UDTEMPOBJ) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -o $(OBJ)/_patches.o
	cd $(USER_DEFINED); make

movm: base head startup asm-patch patch ckpt
	cd ./src; $(CC) _elf_parse.c -c $(TEMPDIR) $(TEMPFILE) $(DISASSEMBLY_FILE) $(TEMPOBJ) $(TF) $(THE_VM) -I$(INCLUDE) $(SECURITY_FLAGS) $(ADDITIONAL_FLAGS) -DMOD=$(MOD) -o $(OBJ)/_elf_parse.o
	cd $(OBJ) ; ld -i *.o -o $(LIB)/movm.o

backup-files: checks
	cd $(APP) ; touch $(EXECUTABLE) ; rm $(EXECUTABLE)
	cd $(APP) ; tar -cvf $(TEMP)/$(SHADOW)/backup.tar *

restore-files: 
	cp $(TEMP)/$(SHADOW)/backup.tar $(APP) ; cd $(APP) ; tar -xvf backup.tar ; rm backup.tar

file-rewriting:
	export MVM_TEMP_FILE=$(TEMP)/__temp_file ; ./scripts/file-processor.sh $(APP) $(TARGET_MODULES)

clean:
	find . -name "*.o" -type f -delete ; rm -f $(EXECUTABLE) ; find $(TEMP) -type f -exec rm {} +
