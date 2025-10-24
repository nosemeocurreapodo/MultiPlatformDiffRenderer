#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*/

ROOT_DIR=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
# SRC_DIR=$(ROOT_DIR)/src

#NAME = depthrendererhls
#TOP_NAME = DepthRenderHLS
NAME = imagerendererhls
TOP_NAME = ImageRenderHLS

PLATFORMS_PATH = /home/emanuel/workspace/visual_odometry_vitis/platform
PLATFORM_NAME=kv260

MULTIPLATFORMRENDERER_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer
HLSLINEARALGEBRA_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer/HLSLinearAlgebra

PLATFORM= $(PLATFORMS_PATH)/$(PLATFORM_NAME)/step2_pfm/build/platform/$(PLATFORM_NAME)_custom/export/$(PLATFORM_NAME)_custom/$(PLATFORM_NAME)_custom.xpfm

BUILD_PATH=$(ROOT_DIR)/build
PLATFORM_INFO_PATH=$(BUILD_PATH)/platforminfo.txt
XO_PATH=$(BUILD_PATH)/xo
XCLBIN_PATH=$(BUILD_PATH)/xclbin
PACKAGE_PATH=$(BUILD_PATH)/package

VPP_FLAGS += -I$(HLSLINEARALGEBRA_PATH)/include
VPP_FLAGS += -I$(MULTIPLATFORMRENDERER_PATH)/src
VPP_FLAGS += -DUSE_VITIS
#VPP_FLAGS += --save-temps 
VPP_LDFLAGS :=

TARGET := hw

.PHONY: help  

help:
	@echo "Makefile usage"
	@echo "make all 	## Generate and package application related files"

all: $(BUILD_PATH) $(PLATFORM_INFO_PATH) $(XO_PATH) $(XCLBIN_PATH) $(PACKAGE_PATH)
#all: $(BUILD_PATH) $(HOST_PATH)

$(BUILD_PATH):
	@echo "Creating build dir"
	mkdir -p $(BUILD_PATH)

$(PLATFORM_INFO_PATH):
	@echo "Get platform info"
	platforminfo  $(PLATFORM) > $(BUILD_PATH)/platforminfo.txt

$(XO_PATH):
	@echo "Making xo"
	mkdir -p $(XO_PATH)
#	v++ -c $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -k vadd --temp_dir $(XO_PATH) --log_dir $(XO_PATH) --report_dir $(XO_PATH) -o $(XO_PATH)/vadd.xo $(ROOT_DIR)/src/vadd.cpp
	cd $(XO_PATH) && v++ --jobs 12 -c $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -k $(TOP_NAME) -o $(XO_PATH)/$(NAME).xo $(MULTIPLATFORMRENDERER_PATH)/src/backends/xrt/hls/rendererhls_kernels.cpp

$(XCLBIN_PATH):
	@echo "Making xclbin"
	mkdir -p $(XCLBIN_PATH)
#	v++ -l $(VPP_FLAGS) $(VPP_LDFLAGS) -t $(TARGET) --platform $(PLATFORM) --temp_dir $(XCLBIN_PATH) --log_dir $(XCLBIN_PATH) --report_dir $(XCLBIN_PATH) -o $(XCLBIN_PATH)/vadd.xclbin $(XO_PATH)/vadd.xo
	cd $(XCLBIN_PATH) && v++ --jobs 12 -l $(VPP_FLAGS) $(VPP_LDFLAGS) -t $(TARGET) --platform $(PLATFORM) -o $(XCLBIN_PATH)/$(NAME).xclbin $(XO_PATH)/$(NAME).xo

emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(PLATFORM) --od $(EMCONFIG_DIR)

create_json:
	cd $(BUILD_PATH) && printf '{\n  "shell_type" : "XRT_FLAT",\n  "num_slots": "1"\n}' > shell.json

$(PACKAGE_PATH): create_json
	@echo "INFO: Packaging Completed"
	mkdir -p $(PACKAGE_PATH)
	cp $(PLATFORMS_PATH)/$(PLATFORM_NAME)/step2_pfm/build/dtb/pl.dtbo $(PACKAGE_PATH)
	cp $(BUILD_PATH)/shell.json $(PACKAGE_PATH)
	cp $(XCLBIN_PATH)/$(NAME).xclbin $(PACKAGE_PATH)/$(NAME).bin
# cp -r $(MULTIPLATFORMRENDERER_PATH)/tests/data $(PACKAGE_PATH)
	@echo "Please  copy $(PACKAGE_PATH) to /lib/firmware/xilinx/ on target board"
	@echo "then use xmutil to load hardware."
	@echo "xmutil unloadapp"
	@echo "xmutil loadapp $(NAME)"
	@echo "Go to /lib/firmware/xilinx/$(NAME) directory and run application"
	@echo "$ ./$(NAME)_host ./$(NAME).xclbin" 0 /data

clean:
	rm -rf build

ultraclean:clean
	rm -rf Vitis_Accel_Examples
