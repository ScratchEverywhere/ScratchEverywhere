PLATFORM ?= ps4

ifeq ($(PLATFORM),ps4)
include make/Makefile_ps4
else
    $(error Unknown platform: $(PLATFORM))
endif
