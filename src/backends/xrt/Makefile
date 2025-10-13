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
VERSION ?=2023.2

COMMON_IMAGE_ZYNQMP ?=${PLATFORM_REPO_PATHS}/xilinx-zynqmp-common-v$(VERSION)/

MULTIPLATFORMRENDERER_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer
HLSLINEARALGEBRA_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer/HLSLinearAlgebra

PLATFORM= $(PLATFORMS_PATH)/$(PLATFORM_NAME)/step2_pfm/build/platform/$(PLATFORM_NAME)_custom/export/$(PLATFORM_NAME)_custom/$(PLATFORM_NAME)_custom.xpfm
#SW_COMP = $(ROOT_DIR)/../step2_pfm/sw_comp/
SYSROOTDIR= $(COMMON_IMAGE_ZYNQMP)/sysroots/cortexa72-cortexa53-xilinx-linux
#COMMON_IMAGE_ZYNQMP ?=${PLATFORM_REPO_PATHS}/sw/zynqmp/xilinx-zynqmp-common-v$(VERSION)/

BUILD_PATH=$(ROOT_DIR)/build
PLATFORM_INFO_PATH=$(BUILD_PATH)/platforminfo.txt
XO_PATH=$(BUILD_PATH)/xo
XCLBIN_PATH=$(BUILD_PATH)/xclbin
HOST_PATH=$(BUILD_PATH)/host
PACKAGE_PATH=$(BUILD_PATH)/package

#PL_EXAMPLE_PATH = $(BUILD_PATH)/Vitis_Accel_Examples/cpp_kernels/simple_vadd/

CXXFLAGS += -I$(SYSROOTDIR)/usr/include/xrt -I$(XILINX_VIVADO)/include -Wall -O0 -g -std=c++1y
CXXFLAGS += -I$(HLSLINEARALGEBRA_PATH)/include
CXXFLAGS += -I$(MULTIPLATFORMRENDERER_PATH)/src
CXXFLAGS += -I$(MULTIPLATFORMRENDERER_PATH)/tests
CXXFLAGS += -I$(SYSROOTDIR)/usr/include/opencv4
# CXXFLAGS += -fmessage-length=0

LDFLAGS += -L$(SYSROOTDIR)/usr/lib -pthread -lxilinxopencl
LDFLAGS += -lrt -lstdc++ 
LDFLAGS += -luuid -lxrt_coreutil
LDFLAGS += -lopencv_core -lopencv_highgui -lopencv_imgcodecs
LDFLAGS += --sysroot=$(SYSROOTDIR)

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

all: $(BUILD_PATH) $(PLATFORM_INFO_PATH) $(XO_PATH) $(XCLBIN_PATH) $(HOST_PATH) $(PACKAGE_PATH)

$(BUILD_PATH):
	@echo "Creating build dir"
	mkdir -p $(BUILD_PATH)

$(PLATFORM_INFO_PATH):
	@echo "Get platform info"
	platforminfo  $(PLATFORM) > $(BUILD_PATH)/platforminfo.txt

#Vitis_Accel_Examples:
#	git clone -b 2023.1 https://github.com/Xilinx/Vitis_Accel_Examples.git $(BUILD_PATH)/Vitis_Accel_Examples

# Quick command for generating vadd hw xclbin
#vadd_hw: Vitis_Accel_Examples
#	$(MAKE) -C $(PL_EXAMPLE_PATH) all TARGET=hw PLATFORM=$(PLATFORM) HOST_ARCH=aarch64 DEV_ARCH=zynquplus EDGE_COMMON_SW=$(COMMON_IMAGE_ZYNQMP) SYSROOT=$(SYSROOTDIR)/sysroots/cortexa72-cortexa53-xilinx-linux
# vadd_hw generates the files for running on hardware. User needs to pick the files required by KV260, rename thr file and transfer them to the board
# Do not use the generated sd_card.img
#$(PL_EXAMPLE_PATH)/simple_vadd:   Vitis_Accel_Examples $(PLATFORM)  
#	$(MAKE) -C $(PL_EXAMPLE_PATH) all TARGET=hw PLATFORM=$(PLATFORM) DEV_ARCH=zynquplus HOST_ARCH=aarch64 EDGE_COMMON_SW=$(COMMON_IMAGE_ZYNQMP) SYSROOT=$(SYSROOTDIR)/sysroots/cortexa72-cortexa53-xilinx-linux

#$(PL_EXAMPLE_PATH)/simple_vadd:
#	$(MAKE) -C src/ all SYSROOT=$(SYSROOTDIR) EDGE_COMMON_SW=$(COMMON_IMAGE_ZYNQMP) PLATFORM=$(PLATFORM)

$(XO_PATH):
	@echo "Making xo"
	mkdir -p $(XO_PATH)
#	v++ -c $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -k vadd --temp_dir $(XO_PATH) --log_dir $(XO_PATH) --report_dir $(XO_PATH) -o $(XO_PATH)/vadd.xo $(ROOT_DIR)/src/vadd.cpp
	cd $(XO_PATH) && v++ --jobs 12 -c $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -k $(TOP_NAME) -o $(XO_PATH)/$(NAME).xo $(MULTIPLATFORMRENDERER_PATH)/src/backends/xrt/hls/$(NAME).cpp

$(XCLBIN_PATH):
	@echo "Making xclbin"
	mkdir -p $(XCLBIN_PATH)
#	v++ -l $(VPP_FLAGS) $(VPP_LDFLAGS) -t $(TARGET) --platform $(PLATFORM) --temp_dir $(XCLBIN_PATH) --log_dir $(XCLBIN_PATH) --report_dir $(XCLBIN_PATH) -o $(XCLBIN_PATH)/vadd.xclbin $(XO_PATH)/vadd.xo
	cd $(XCLBIN_PATH) && v++ --jobs 12 -l $(VPP_FLAGS) $(VPP_LDFLAGS) -t $(TARGET) --platform $(PLATFORM) -o $(XCLBIN_PATH)/$(NAME).xclbin $(XO_PATH)/$(NAME).xo

$(HOST_PATH):
	@echo "Making host"
	mkdir -p $(HOST_PATH)
	cd $(HOST_PATH) && $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++ $(CXXFLAGS) $(LDFLAGS) -o $(HOST_PATH)/$(NAME)_host $(MULTIPLATFORMRENDERER_PATH)/src/backends/xrt/$(NAME)_test_xrt.cpp


emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(PLATFORM) --od $(EMCONFIG_DIR)

#ifeq ($(EMU_PS), X86)
#	v++ -p $(LINK_OUTPUT) $(VPP_FLAGS) -t $(TARGET) --platform $(PLATFORM) -o $(BUILD_DIR)/vadd.xclbin
#endif

create_json:
	cd $(BUILD_PATH) && printf '{\n  "shell_type" : "XRT_FLAT",\n  "num_slots": "1"\n}' > shell.json

$(PACKAGE_PATH): create_json
	@echo "INFO: Packaging Completed"
	mkdir -p $(PACKAGE_PATH)
	cp $(PLATFORMS_PATH)/$(PLATFORM_NAME)/step2_pfm/build/dtb/pl.dtbo $(PACKAGE_PATH)
	cp $(BUILD_PATH)/shell.json $(PACKAGE_PATH)
	cp $(XCLBIN_PATH)/$(NAME).xclbin $(PACKAGE_PATH)/$(NAME).bin
	cp $(HOST_PATH)/$(NAME)_host $(PACKAGE_PATH)
	cp -r $(MULTIPLATFORMRENDERER_PATH)/tests/data $(PACKAGE_PATH)
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
