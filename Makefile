PLATFORM ?= 3ds

ifeq ($(PLATFORM),3ds)
    include Makefile_3ds
else ifeq ($(PLATFORM),pc)
    include Makefile_pc
else ifeq ($(PLATFORM),wiiu)
    include Makefile_wiiu
else ifeq ($(PLATFORM),wii)
    include Makefile_wii
else ifeq ($(PLATFORM),gamecube)
    include Makefile_gamecube
else
    $(error Unknown platform: $(PLATFORM))
endif
