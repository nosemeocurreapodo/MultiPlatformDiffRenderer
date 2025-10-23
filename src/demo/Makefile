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
VERSION ?=2024.2

COMMON_IMAGE_ZYNQMP ?=${PLATFORM_REPO_PATHS}/xilinx-zynqmp-common-v$(VERSION)/

MULTIPLATFORMRENDERER_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer
HLSLINEARALGEBRA_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer/HLSLinearAlgebra

PLATFORM= $(PLATFORMS_PATH)/$(PLATFORM_NAME)/step2_pfm/build/platform/$(PLATFORM_NAME)_custom/export/$(PLATFORM_NAME)_custom/$(PLATFORM_NAME)_custom.xpfm
#SYSROOTDIR= $(COMMON_IMAGE_ZYNQMP)/sysroots/cortexa72-cortexa53-xilinx-linux
SYSROOTDIR= $(COMMON_IMAGE_ZYNQMP)/sysroots/aarch64-xilinx-linux
#

BUILD_PATH=$(ROOT_DIR)/build
PLATFORM_INFO_PATH=$(BUILD_PATH)/platforminfo.txt
XO_PATH=$(BUILD_PATH)/xo
XCLBIN_PATH=$(BUILD_PATH)/xclbin
HOST_PATH=$(BUILD_PATH)/host
PACKAGE_PATH=$(BUILD_PATH)/package

#PL_EXAMPLE_PATH = $(BUILD_PATH)/Vitis_Accel_Examples/cpp_kernels/simple_vadd/

SOURCES += $(MULTIPLATFORMRENDERER_PATH)/src/demo/demo_assimp.cpp
SOURCES += $(MULTIPLATFORMRENDERER_PATH)/thirdparty/glad2/src/egl.c
SOURCES += $(MULTIPLATFORMRENDERER_PATH)/thirdparty/glad2/src/gles2.c

CXXFLAGS += -I$(SYSROOTDIR)/usr/include/xrt -I$(XILINX_VIVADO)/include #-Wall -O0 -g -std=c++1y
CXXFLAGS += -I$(HLSLINEARALGEBRA_PATH)/include
CXXFLAGS += -I$(MULTIPLATFORMRENDERER_PATH)/src
CXXFLAGS += -I$(MULTIPLATFORMRENDERER_PATH)/thirdparty/glad2/include
CXXFLAGS += -I$(SYSROOTDIR)/usr/include/opencv4
CXXFLAGS += -I$(SYSROOTDIR)/usr/include/eigen3
CXXFLAGS += -DCOMPILE_GLES2
# CXXFLAGS += -DCOMPILE_XRT
# CXXFLAGS += -fmessage-length=0

LDFLAGS += -L$(SYSROOTDIR)/usr/lib -pthread -lxilinxopencl
LDFLAGS += -lrt -lstdc++ 
LDFLAGS += -luuid -lxrt_coreutil
LDFLAGS += -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc
LDFLAGS += -lassimp -lEGL
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

#all: $(BUILD_PATH) $(PLATFORM_INFO_PATH) $(XO_PATH) $(XCLBIN_PATH) $(HOST_PATH) $(PACKAGE_PATH)
all: $(BUILD_PATH) $(HOST_PATH)

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

$(HOST_PATH):
	@echo "Making host"
	mkdir -p $(HOST_PATH)
	cd $(HOST_PATH) && $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++ $(CXXFLAGS) $(LDFLAGS) -o $(HOST_PATH)/demo_host $(SOURCES)

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
	cp $(HOST_PATH)/$(NAME)_host $(PACKAGE_PATH)
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
