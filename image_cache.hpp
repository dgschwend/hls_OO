#ifndef IMAGE_CACHE_HPP_94D645A0
#define IMAGE_CACHE_HPP_94D645A0

// Data Types for FPGA Implementation
#include "fpga_top.hpp" 

#include "memory_controller.hpp"

// ===============
// = Image Cache =
// ===============
class ImageCache {
 public:
  ImageCache();
  void reset();
  void setNextChannel(data_t value);
  void preloadPixelFromDRAM(MemoryController *DRAM);
  void preloadRowFromDRAM(MemoryController *DRAM);
  void setLayerConfig(layer_t &layer);
  data_t getPixel(coordinate_t y, coordinate_t x, channel_t ci);

 private:
  data_t BRAM[MAX_IMAGE_CACHE_SIZE];
  cacheline_t curr_img_cache_line;
  imgcacheaddr_t next_img_cache_addr;
  imgcacheaddr_t line_width;
  imgdramoffset_t loads_left;
  dimension_t width_in;
  channel_t ch_in;
};

#endif /* end of include guard: IMAGE_CACHE_HPP_94D645A0 */
