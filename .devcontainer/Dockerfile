FROM ubuntu:22.04

RUN apt-get update &&  DEBIAN_FRONTEND=noninteractive apt-get install -y -q \
  build-essential \
  cmake \
  gdb \
  # libsuitesparse-dev \
  libeigen3-dev \
  # libboost-all-dev \
  libopencv-dev \
  # libglew-dev \
  # libgl1-mesa-dri \
  # libegl1-mesa-dev \
  # libgbm-dev \ 
  # libglfw3-dev \
  git \
  libepoxy-dev \
  python3-pip \
  libgtest-dev \
  locales \
  libtinfo5

COPY packages/xrt_202320.2.16.204_22.04-amd64-xrt.deb /tmp
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y /tmp/xrt_202320.2.16.204_22.04-amd64-xrt.deb
RUN rm /tmp/xrt_202320.2.16.204_22.04-amd64-xrt.deb

COPY packages/xilinx-u280-gen3x16-xdma-1-202211-1-dev_1-3585755_all.deb /tmp
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y /tmp/xilinx-u280-gen3x16-xdma-1-202211-1-dev_1-3585755_all.deb
RUN rm /tmp/xilinx-u280-gen3x16-xdma-1-202211-1-dev_1-3585755_all.deb

RUN locale-gen en_US.UTF-8 && update-locale
ENV LANG en_US.UTF-8

# RUN git clone --recursive https://github.com/stevenlovegrove/Pangolin.git && cd Pangolin && mkdir build && cd build && cmake ../ && make -j4 && make install && ldconfig
# RUN unset DISPLAY
# RUN unset WAYLAND_DISPLAY
# RUN set EGL_PLATFORM=gbm
# ENV LIBGL_ALWAYS_SOFTWARE=1
# ENV EGL_PLATFORM=surfaceless