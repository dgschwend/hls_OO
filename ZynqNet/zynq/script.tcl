############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2016 Xilinx, Inc. All Rights Reserved.
############################################################
open_project ZynqNet
set_top fpga_top
add_files weights_cache.hpp
add_files weights_cache.cpp
add_files processing_element.hpp
add_files processing_element.cpp
add_files output_cache.hpp
add_files output_cache.cpp
add_files network.hpp
add_files netconfig.hpp
add_files memory_controller.hpp
add_files memory_controller.cpp
add_files image_cache.hpp
add_files image_cache.cpp
add_files fpga_top.hpp
add_files fpga_top.cpp
add_files -tb cpu_top.cpp
add_files -tb cpu_top.hpp
add_files -tb indata.bin
add_files -tb netconfig.cpp
add_files -tb netconfig.hpp
add_files -tb network.cpp
add_files -tb network.hpp
add_files -tb unittests.cpp
add_files -tb unittests.hpp
add_files -tb weights.bin
open_solution "zynq"
set_part {xc7z045fbg676-3} -tool vivado
create_clock -period 5 -name default
config_rtl -encoding auto -reset control -reset_async -reset_level low
config_compile -name_max_length 60 -no_signed_zeros -pipeline_loops 0 -unsafe_math_optimizations
config_dataflow -default_channel pingpong -fifo_depth 0
config_interface -m_axi_offset direct -register_io scalar_all -trim_dangling_port
source "./ZynqNet/zynq/directives.tcl"
csim_design -clean -O
csynth_design
cosim_design -O -trace_level port -rtl vhdl
export_design -format ip_catalog
