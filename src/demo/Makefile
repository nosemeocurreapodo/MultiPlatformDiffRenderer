#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*/

ROOT_DIR=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
# SRC_DIR=$(ROOT_DIR)/src

COMMON_IMAGE_ZYNQMP ?=${PLATFORM_REPO_PATHS}/xilinx-zynqmp-common-v$(VERSION)/

MULTIPLATFORMRENDERER_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer
HLSLINEARALGEBRA_PATH= /home/emanuel/workspace/MultiPlatformDiffRenderer/HLSLinearAlgebra

#SW_COMP = $(ROOT_DIR)/../step2_pfm/sw_comp/
SYSROOTDIR= $(COMMON_IMAGE_ZYNQMP)/sysroots/cortexa72-cortexa53-xilinx-linux
#COMMON_IMAGE_ZYNQMP ?=${PLATFORM_REPO_PATHS}/sw/zynqmp/xilinx-zynqmp-common-v$(VERSION)/

BUILD_PATH=$(ROOT_DIR)/build
HOST_PATH=$(BUILD_PATH)/host

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
# CXXFLAGS += -fmessage-length=0

LDFLAGS += -L$(SYSROOTDIR)/usr/lib -pthread -lxilinxopencl
LDFLAGS += -lrt -lstdc++ 
LDFLAGS += -luuid -lxrt_coreutil
LDFLAGS += -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc
LDFLAGS += -lassimp -lEGL
LDFLAGS += --sysroot=$(SYSROOTDIR)

.PHONY: help  

help:
	@echo "Makefile usage"
	@echo "make all 	## Generate and package application related files"

all: $(BUILD_PATH) $(HOST_PATH) 

$(BUILD_PATH):
	@echo "Creating build dir"
	mkdir -p $(BUILD_PATH)

$(HOST_PATH):
	@echo "Making host"
	mkdir -p $(HOST_PATH)
	cd $(HOST_PATH) && $(XILINX_VITIS)/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++ $(CXXFLAGS) $(LDFLAGS) -DCOMPILE_GLES2 -o $(HOST_PATH)/demo_host $(SOURCES)

clean:
	rm -rf build

ultraclean:clean
	rm -rf Vitis_Accel_Examples
