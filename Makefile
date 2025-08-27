PLATFORM ?= 3ds

ifeq ($(PLATFORM),3ds)
include make/Makefile_3ds
else ifeq ($(PLATFORM),pc)
include make/Makefile_pc
else ifeq ($(PLATFORM),wiiu)
include make/Makefile_wiiu
else ifeq ($(PLATFORM),wii)
include make/Makefile_wii
else ifeq ($(PLATFORM),gamecube)
include make/Makefile_gamecube
else ifeq ($(PLATFORM),switch)
include make/Makefile_switch
else ifeq ($(PLATFORM),vita)
include make/Makefile_vita
else
    $(error Unknown platform: $(PLATFORM))
endif
