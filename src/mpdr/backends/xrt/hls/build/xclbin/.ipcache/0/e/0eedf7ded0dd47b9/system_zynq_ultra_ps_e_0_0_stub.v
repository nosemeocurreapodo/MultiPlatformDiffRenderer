// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2024.2 (lin64) Build 5239630 Fri Nov 08 22:34:34 MST 2024
// Date        : Thu Jan 22 12:33:42 2026
// Host        : HP-ProBook-445-G7 running 64-bit Ubuntu 24.04.3 LTS
// Command     : write_verilog -force -mode synth_stub -rename_top decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix -prefix
//               decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix_ system_zynq_ultra_ps_e_0_0_stub.v
// Design      : system_zynq_ultra_ps_e_0_0
// Purpose     : Stub declaration of top-level module interface
// Device      : xck26-sfvc784-2LV-c
// --------------------------------------------------------------------------------

// This empty module with port declaration file causes synthesis tools to infer a black box for IP.
// The synthesis directives are for Synopsys Synplify support to prevent IO buffer insertion.
// Please paste the declaration into a Verilog source file or add the file as an additional source.
(* CHECK_LICENSE_TYPE = "system_zynq_ultra_ps_e_0_0,zynq_ultra_ps_e_v3_5_5_zynq_ultra_ps_e,{}" *) (* CORE_GENERATION_INFO = "system_zynq_ultra_ps_e_0_0,zynq_ultra_ps_e_v3_5_5_zynq_ultra_ps_e,{x_ipProduct=Vivado 2024.2,x_ipVendor=xilinx.com,x_ipLibrary=ip,x_ipName=zynq_ultra_ps_e,x_ipVersion=3.5,x_ipCoreRevision=5,x_ipLanguage=VERILOG,x_ipSimLanguage=MIXED,C_DP_USE_AUDIO=0,C_DP_USE_VIDEO=0,C_MAXIGP0_DATA_WIDTH=128,C_MAXIGP1_DATA_WIDTH=128,C_MAXIGP2_DATA_WIDTH=32,C_SAXIGP0_DATA_WIDTH=128,C_SAXIGP1_DATA_WIDTH=128,C_SAXIGP2_DATA_WIDTH=128,C_SAXIGP3_DATA_WIDTH=128,C_SAXIGP4_DATA_WIDTH=128,C_SAXIGP5_DATA_WIDTH=128,C_SAXIGP6_DATA_WIDTH=128,C_USE_DIFF_RW_CLK_GP0=0,C_USE_DIFF_RW_CLK_GP1=0,C_USE_DIFF_RW_CLK_GP2=0,C_USE_DIFF_RW_CLK_GP3=0,C_USE_DIFF_RW_CLK_GP4=0,C_USE_DIFF_RW_CLK_GP5=0,C_USE_DIFF_RW_CLK_GP6=0,C_EN_FIFO_ENET0=0,C_EN_FIFO_ENET1=0,C_EN_FIFO_ENET2=0,C_EN_FIFO_ENET3=0,C_PL_CLK0_BUF=TRUE,C_PL_CLK1_BUF=TRUE,C_PL_CLK2_BUF=FALSE,C_PL_CLK3_BUF=FALSE,C_TRACE_PIPELINE_WIDTH=8,C_EN_EMIO_TRACE=0,C_TRACE_DATA_WIDTH=32,C_USE_DEBUG_TEST=0,C_SD0_INTERNAL_BUS_WIDTH=5,C_SD1_INTERNAL_BUS_WIDTH=8,C_NUM_F2P_0_INTR_INPUTS=1,C_NUM_F2P_1_INTR_INPUTS=1,C_EMIO_GPIO_WIDTH=1,C_NUM_FABRIC_RESETS=1}" *) (* DowngradeIPIdentifiedWarnings = "yes" *) 
(* X_CORE_INFO = "zynq_ultra_ps_e_v3_5_5_zynq_ultra_ps_e,Vivado 2024.2" *) 
module decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix(maxihpm0_lpd_aclk, maxigp2_awid, 
  maxigp2_awaddr, maxigp2_awlen, maxigp2_awsize, maxigp2_awburst, maxigp2_awlock, 
  maxigp2_awcache, maxigp2_awprot, maxigp2_awvalid, maxigp2_awuser, maxigp2_awready, 
  maxigp2_wdata, maxigp2_wstrb, maxigp2_wlast, maxigp2_wvalid, maxigp2_wready, maxigp2_bid, 
  maxigp2_bresp, maxigp2_bvalid, maxigp2_bready, maxigp2_arid, maxigp2_araddr, maxigp2_arlen, 
  maxigp2_arsize, maxigp2_arburst, maxigp2_arlock, maxigp2_arcache, maxigp2_arprot, 
  maxigp2_arvalid, maxigp2_aruser, maxigp2_arready, maxigp2_rid, maxigp2_rdata, 
  maxigp2_rresp, maxigp2_rlast, maxigp2_rvalid, maxigp2_rready, maxigp2_awqos, maxigp2_arqos, 
  saxihp2_fpd_aclk, saxigp4_aruser, saxigp4_awuser, saxigp4_awid, saxigp4_awaddr, 
  saxigp4_awlen, saxigp4_awsize, saxigp4_awburst, saxigp4_awlock, saxigp4_awcache, 
  saxigp4_awprot, saxigp4_awvalid, saxigp4_awready, saxigp4_wdata, saxigp4_wstrb, 
  saxigp4_wlast, saxigp4_wvalid, saxigp4_wready, saxigp4_bid, saxigp4_bresp, saxigp4_bvalid, 
  saxigp4_bready, saxigp4_arid, saxigp4_araddr, saxigp4_arlen, saxigp4_arsize, 
  saxigp4_arburst, saxigp4_arlock, saxigp4_arcache, saxigp4_arprot, saxigp4_arvalid, 
  saxigp4_arready, saxigp4_rid, saxigp4_rdata, saxigp4_rresp, saxigp4_rlast, saxigp4_rvalid, 
  saxigp4_rready, saxigp4_awqos, saxigp4_arqos, pl_ps_irq0, pl_resetn0, pl_clk0, pl_clk1)
/* synthesis syn_black_box black_box_pad_pin="maxigp2_awid[15:0],maxigp2_awaddr[39:0],maxigp2_awlen[7:0],maxigp2_awsize[2:0],maxigp2_awburst[1:0],maxigp2_awlock,maxigp2_awcache[3:0],maxigp2_awprot[2:0],maxigp2_awvalid,maxigp2_awuser[15:0],maxigp2_awready,maxigp2_wdata[31:0],maxigp2_wstrb[3:0],maxigp2_wlast,maxigp2_wvalid,maxigp2_wready,maxigp2_bid[15:0],maxigp2_bresp[1:0],maxigp2_bvalid,maxigp2_bready,maxigp2_arid[15:0],maxigp2_araddr[39:0],maxigp2_arlen[7:0],maxigp2_arsize[2:0],maxigp2_arburst[1:0],maxigp2_arlock,maxigp2_arcache[3:0],maxigp2_arprot[2:0],maxigp2_arvalid,maxigp2_aruser[15:0],maxigp2_arready,maxigp2_rid[15:0],maxigp2_rdata[31:0],maxigp2_rresp[1:0],maxigp2_rlast,maxigp2_rvalid,maxigp2_rready,maxigp2_awqos[3:0],maxigp2_arqos[3:0],saxigp4_aruser,saxigp4_awuser,saxigp4_awid[5:0],saxigp4_awaddr[48:0],saxigp4_awlen[7:0],saxigp4_awsize[2:0],saxigp4_awburst[1:0],saxigp4_awlock,saxigp4_awcache[3:0],saxigp4_awprot[2:0],saxigp4_awvalid,saxigp4_awready,saxigp4_wdata[127:0],saxigp4_wstrb[15:0],saxigp4_wlast,saxigp4_wvalid,saxigp4_wready,saxigp4_bid[5:0],saxigp4_bresp[1:0],saxigp4_bvalid,saxigp4_bready,saxigp4_arid[5:0],saxigp4_araddr[48:0],saxigp4_arlen[7:0],saxigp4_arsize[2:0],saxigp4_arburst[1:0],saxigp4_arlock,saxigp4_arcache[3:0],saxigp4_arprot[2:0],saxigp4_arvalid,saxigp4_arready,saxigp4_rid[5:0],saxigp4_rdata[127:0],saxigp4_rresp[1:0],saxigp4_rlast,saxigp4_rvalid,saxigp4_rready,saxigp4_awqos[3:0],saxigp4_arqos[3:0],pl_ps_irq0[0:0],pl_resetn0" */
/* synthesis syn_force_seq_prim="maxihpm0_lpd_aclk" */
/* synthesis syn_force_seq_prim="saxihp2_fpd_aclk" */
/* synthesis syn_force_seq_prim="pl_clk0" */
/* synthesis syn_force_seq_prim="pl_clk1" */;
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 M_AXI_HPM0_LPD_ACLK CLK" *) (* X_INTERFACE_MODE = "slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME M_AXI_HPM0_LPD_ACLK, ASSOCIATED_BUSIF M_AXI_HPM0_LPD, FREQ_HZ 199998000, FREQ_TOLERANCE_HZ 0, PHASE 0.0, CLK_DOMAIN system_clk_wiz_0_0_clk_out1, INSERT_VIP 0" *) input maxihpm0_lpd_aclk /* synthesis syn_isclock = 1 */;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWID" *) (* X_INTERFACE_MODE = "master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME M_AXI_HPM0_LPD, NUM_WRITE_OUTSTANDING 8, NUM_READ_OUTSTANDING 8, DATA_WIDTH 32, PROTOCOL AXI4, FREQ_HZ 199998000, ID_WIDTH 16, ADDR_WIDTH 40, AWUSER_WIDTH 16, ARUSER_WIDTH 16, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 1, HAS_PROT 1, HAS_CACHE 1, HAS_QOS 1, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 1, MAX_BURST_LENGTH 256, PHASE 0.0, CLK_DOMAIN system_clk_wiz_0_0_clk_out1, NUM_READ_THREADS 4, NUM_WRITE_THREADS 4, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *) output [15:0]maxigp2_awid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWADDR" *) output [39:0]maxigp2_awaddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWLEN" *) output [7:0]maxigp2_awlen;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWSIZE" *) output [2:0]maxigp2_awsize;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWBURST" *) output [1:0]maxigp2_awburst;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWLOCK" *) output maxigp2_awlock;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWCACHE" *) output [3:0]maxigp2_awcache;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWPROT" *) output [2:0]maxigp2_awprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWVALID" *) output maxigp2_awvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWUSER" *) output [15:0]maxigp2_awuser;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWREADY" *) input maxigp2_awready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD WDATA" *) output [31:0]maxigp2_wdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD WSTRB" *) output [3:0]maxigp2_wstrb;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD WLAST" *) output maxigp2_wlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD WVALID" *) output maxigp2_wvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD WREADY" *) input maxigp2_wready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD BID" *) input [15:0]maxigp2_bid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD BRESP" *) input [1:0]maxigp2_bresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD BVALID" *) input maxigp2_bvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD BREADY" *) output maxigp2_bready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARID" *) output [15:0]maxigp2_arid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARADDR" *) output [39:0]maxigp2_araddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARLEN" *) output [7:0]maxigp2_arlen;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARSIZE" *) output [2:0]maxigp2_arsize;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARBURST" *) output [1:0]maxigp2_arburst;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARLOCK" *) output maxigp2_arlock;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARCACHE" *) output [3:0]maxigp2_arcache;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARPROT" *) output [2:0]maxigp2_arprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARVALID" *) output maxigp2_arvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARUSER" *) output [15:0]maxigp2_aruser;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARREADY" *) input maxigp2_arready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RID" *) input [15:0]maxigp2_rid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RDATA" *) input [31:0]maxigp2_rdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RRESP" *) input [1:0]maxigp2_rresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RLAST" *) input maxigp2_rlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RVALID" *) input maxigp2_rvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD RREADY" *) output maxigp2_rready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD AWQOS" *) output [3:0]maxigp2_awqos;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI_HPM0_LPD ARQOS" *) output [3:0]maxigp2_arqos;
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 S_AXI_HP2_FPD_ACLK CLK" *) (* X_INTERFACE_MODE = "slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_HP2_FPD_ACLK, ASSOCIATED_BUSIF S_AXI_HP2_FPD, FREQ_HZ 199998000, FREQ_TOLERANCE_HZ 0, PHASE 0.0, CLK_DOMAIN system_clk_wiz_0_0_clk_out1, INSERT_VIP 0" *) input saxihp2_fpd_aclk /* synthesis syn_isclock = 1 */;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARUSER" *) (* X_INTERFACE_MODE = "slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_HP2_FPD, NUM_WRITE_OUTSTANDING 16, NUM_READ_OUTSTANDING 16, DATA_WIDTH 128, PROTOCOL AXI4, FREQ_HZ 199998000, ID_WIDTH 6, ADDR_WIDTH 49, AWUSER_WIDTH 1, ARUSER_WIDTH 1, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 1, HAS_PROT 1, HAS_CACHE 1, HAS_QOS 1, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, MAX_BURST_LENGTH 64, PHASE 0.0, CLK_DOMAIN system_clk_wiz_0_0_clk_out1, NUM_READ_THREADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *) input saxigp4_aruser;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWUSER" *) input saxigp4_awuser;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWID" *) input [5:0]saxigp4_awid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWADDR" *) input [48:0]saxigp4_awaddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWLEN" *) input [7:0]saxigp4_awlen;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWSIZE" *) input [2:0]saxigp4_awsize;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWBURST" *) input [1:0]saxigp4_awburst;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWLOCK" *) input saxigp4_awlock;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWCACHE" *) input [3:0]saxigp4_awcache;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWPROT" *) input [2:0]saxigp4_awprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWVALID" *) input saxigp4_awvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWREADY" *) output saxigp4_awready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD WDATA" *) input [127:0]saxigp4_wdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD WSTRB" *) input [15:0]saxigp4_wstrb;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD WLAST" *) input saxigp4_wlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD WVALID" *) input saxigp4_wvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD WREADY" *) output saxigp4_wready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD BID" *) output [5:0]saxigp4_bid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD BRESP" *) output [1:0]saxigp4_bresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD BVALID" *) output saxigp4_bvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD BREADY" *) input saxigp4_bready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARID" *) input [5:0]saxigp4_arid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARADDR" *) input [48:0]saxigp4_araddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARLEN" *) input [7:0]saxigp4_arlen;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARSIZE" *) input [2:0]saxigp4_arsize;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARBURST" *) input [1:0]saxigp4_arburst;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARLOCK" *) input saxigp4_arlock;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARCACHE" *) input [3:0]saxigp4_arcache;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARPROT" *) input [2:0]saxigp4_arprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARVALID" *) input saxigp4_arvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARREADY" *) output saxigp4_arready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RID" *) output [5:0]saxigp4_rid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RDATA" *) output [127:0]saxigp4_rdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RRESP" *) output [1:0]saxigp4_rresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RLAST" *) output saxigp4_rlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RVALID" *) output saxigp4_rvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD RREADY" *) input saxigp4_rready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD AWQOS" *) input [3:0]saxigp4_awqos;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI_HP2_FPD ARQOS" *) input [3:0]saxigp4_arqos;
  (* X_INTERFACE_INFO = "xilinx.com:signal:interrupt:1.0 PL_PS_IRQ0 INTERRUPT" *) (* X_INTERFACE_MODE = "slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME PL_PS_IRQ0, SENSITIVITY LEVEL_HIGH, PortWidth 1" *) input [0:0]pl_ps_irq0;
  (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 PL_RESETN0 RST" *) (* X_INTERFACE_MODE = "master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME PL_RESETN0, POLARITY ACTIVE_LOW, INSERT_VIP 0" *) output pl_resetn0;
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 PL_CLK0 CLK" *) (* X_INTERFACE_MODE = "master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME PL_CLK0, FREQ_HZ 99999001, FREQ_TOLERANCE_HZ 0, PHASE 0.0, CLK_DOMAIN system_zynq_ultra_ps_e_0_0_pl_clk0, INSERT_VIP 0" *) output pl_clk0 /* synthesis syn_isclock = 1 */;
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 PL_CLK1 CLK" *) (* X_INTERFACE_MODE = "master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME PL_CLK1, FREQ_HZ 99999001, FREQ_TOLERANCE_HZ 0, PHASE 0.0, CLK_DOMAIN system_zynq_ultra_ps_e_0_0_pl_clk1, INSERT_VIP 0" *) output pl_clk1 /* synthesis syn_isclock = 1 */;
endmodule
