############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2016 Xilinx, Inc. All Rights Reserved.
############################################################
open_project ZynqNet
set_top fpga_top
add_files fpga_top.cpp
add_files fpga_top.hpp
add_files image_cache.cpp
add_files image_cache.hpp
add_files memory_controller.cpp
add_files memory_controller.hpp
add_files netconfig.hpp
add_files network.hpp
add_files output_cache.cpp
add_files output_cache.hpp
add_files processing_element.cpp
add_files processing_element.hpp
add_files weights_cache.cpp
add_files weights_cache.hpp
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
#source "./ZynqNet/zynq/directives.tcl"
csim_design -clean -O
csynth_design
cosim_design -O -reduce_diskspace -rtl vhdl
export_design -format ip_catalog
