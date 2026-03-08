PLATFORM ?= 3ds

ifeq ($(PLATFORM),3ds)
include make/Makefile_3ds
else ifeq ($(PLATFORM),ps4)
include make/Makefile_ps4
else
    $(error Unknown platform: $(PLATFORM))
endif
