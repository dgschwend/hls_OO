//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  fpga_top.cpp
//
//  Top-Level Module for SqueezeNetOnFGPA.
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "fpga_top.hpp"

bool LOG_DETAILS = false;
int LOG_LEVEL = 0;
void print_indent(int lvl) {
  while (lvl--) {
    putchar(' ');
    putchar(' ');
  }
}

// =====================
// = Memory Controller =
// =====================
MemoryController::MemoryController(data_t *mempointer,
                                   unsigned int weights_offset,
                                   unsigned int data_offset) {
  SHARED_DRAM = mempointer;
  DRAM_WEIGHTS = (SHARED_DRAM + weights_offset);
  DRAM_DATA = (SHARED_DRAM + data_offset);

  LOG("MemoryCtrl: Constructor.\n");
  LOG(" - SHARED_DRAM     = %lu\n", (long)SHARED_DRAM);
  LOG(" - DRAM_WEIGHTS    = @%luB\n", (long)DRAM_WEIGHTS - (long)SHARED_DRAM);
  LOG(" - DRAM_DATA       = @%luB\n", (long)DRAM_DATA - (long)SHARED_DRAM);
}

void MemoryController::loadConfig(int num_layers, layer_t *configBRAM) {
  loadConfigViaFloatUnion(num_layers, configBRAM);
  LOG("MemoryCtrl: loadConfig (%d layers).\n", (int)num_layers);
}

void MemoryController::loadConfigViaFloatUnion(int num_layers,
                                               layer_t *configBRAM) {
  // Fetch Layer Configuration by Transferring floats + Converting to layer_t
  float floats[NUM_FLOATS_PER_LAYER];
  layer_t layer;
  for (numlayers_t l = 0; l < num_layers; l++) {
    memcpy(floats, &SHARED_DRAM[l * NUM_FLOATS_PER_LAYER],
           NUM_FLOATS_PER_LAYER * sizeof(float));
    floatsToLayerT(floats, layer);
    configBRAM[l] = layer;
  }
}

void MemoryController::floatsToLayerT(float floats[NUM_FLOATS_PER_LAYER],
                                      layer_t &layer) {
  // Extract layer_t hidden in fake floats (which are actually uint32s)

  // clang-format not used on this section to keep it compact
  // clang-format off
  union { float f; unsigned int i;} u;
  //layer.type = LAYER_NONE;  // type not used
  layer.name[0] = '\0';     // name not used in FPGA
  u.f = floats[0]; layer.width = u.i;
  u.f = floats[1]; layer.height = u.i;
  u.f = floats[2]; layer.channels_in = u.i;
  u.f = floats[3]; layer.channels_out = u.i;
  u.f = floats[4]; layer.kernel = u.i;
  u.f = floats[5]; layer.stride = u.i;
  u.f = floats[6]; layer.pad = u.i;
  u.f = floats[7]; layer.mem_addr_input = u.i;
  u.f = floats[8]; layer.mem_addr_output = u.i;
  u.f = floats[9]; layer.mem_addr_weights = u.i;
  u.f = floats[10]; layer.is_expand_layer = u.i;
  u.f = floats[11]; layer.pool = (u.i == 1 ? POOL_GLOBAL : POOL_NONE);
  // clang-format on
}

void MemoryController::setLayerConfig(layer_t &layer) {
  dram_weights_offset = layer.mem_addr_weights;
  dram_input_offset = layer.mem_addr_input;
  dram_output_offset = layer.mem_addr_output;
  pixels_per_row = layer.width * layer.channels_in;
  ch_out = layer.channels_out;
  width_out = (layer.stride == 2) ? (layer.width / 2) : (layer.width / 1);
  is_expand_layer = layer.is_expand_layer;

  LOG("MemoryCtrl: setLayerConfig.\n");
  LOG(" - weights offset  = %6d Elements, DRAM @%8luB\n",
      (int)dram_weights_offset, (int)dram_weights_offset * sizeof(data_t));
  LOG(" - input offset    = %6d Elements, DRAM @%8luB\n",
      (int)dram_input_offset, (int)dram_input_offset * sizeof(data_t));
  LOG(" - output offset   = %6d Elements, DRAM @%8luB\n",
      (int)dram_output_offset, (int)dram_output_offset * sizeof(data_t));
  LOG(" - pixels per row  = %d\n", (int)pixels_per_row);
  LOG(" - ch_out          = %d\n", (int)ch_out);
  LOG(" - width_out       = %d\n", (int)width_out);
  LOG(" - is_expand_layer = %d\n", (int)is_expand_layer);
}

data_t MemoryController::loadNextWeight() {
  if (LOG_DETAILS)
    LOG("MemoryCtrl: loadNextWeight  (#%4d from DRAM @%4luB): %6.2f\n",
        (int)dram_weights_offset,
        (long)&DRAM_WEIGHTS[dram_weights_offset] - (long)DRAM_DATA,
        DRAM_WEIGHTS[dram_weights_offset]);
  return DRAM_WEIGHTS[dram_weights_offset++];
}

void MemoryController::setPixelLoadRow(coordinate_t y) {
  LOG("MemoryCtrl: setPixelLoadRow (row %2d).\n", (int)y);
  dram_pixel_offset = dram_input_offset + pixels_per_row * y;
}

data_t MemoryController::loadNextChannel() {
  data_t pixel_from_ram = DRAM_DATA[dram_pixel_offset];
  if (LOG_DETAILS)
    LOG("MemoryCtrl: loadNextChannel (from DRAM @%4luB) -> %.2f\n",
        (int)dram_pixel_offset * sizeof(data_t), pixel_from_ram);
  dram_pixel_offset++;  // increment address for next fetch
  return pixel_from_ram;
};

void MemoryController::writeBackOutputPixel(coordinate_t y_out,
                                            coordinate_t x_out,
                                            data_t *outputCache) {
  // Calculate Output Memory Address
  memaddr_t px_offset = ch_out * (width_out * y_out + x_out);
  data_t *DRAM_LAYER_OUT, *DRAM_PX_OUT;
  DRAM_LAYER_OUT = (DRAM_DATA + dram_output_offset);
  // Leave double space for "expand" layers (more ch_out will be added)
  DRAM_PX_OUT =
      DRAM_LAYER_OUT + (is_expand_layer ? 2 * (int)px_offset : (int)px_offset);

  LOG("MemoryController: writeBackOutputPixel (%2d, %2d)\n", (int)y_out,
      (int)x_out);
  LOG(" - writing %2d channels to DRAM @%luB+\n", (int)ch_out,
      (long)DRAM_PX_OUT - (long)DRAM_DATA);

  LOG_LEVEL++;
L_writeBackOutputPixel:
  for (channel_t co = 0; co < ch_out; co++) {
    DRAM_PX_OUT[co] = outputCache[co];
    LOG(" WB ch%d (@%luB): %6.2f\n", (int)co,
        ((long)(DRAM_PX_OUT + co) - (long)DRAM_DATA), outputCache[co]);
  }
  LOG_LEVEL--;
}

void MemoryController::writeBackResult(data_t *globalPoolCache) {
L_writeBackResult:
  for (int i = 0; i < ch_out; i++) {  // ch_out set from last layer
    DRAM_DATA[i] = globalPoolCache[i];
  }
  LOG("MemoryCtrl: writeBackResult (%d Bytes) to DRAM @0\n",
      (int)(ch_out * sizeof(data_t)));
}

// ===============
// = Image Cache =
// ===============
ImageCache::ImageCache(){};
void ImageCache::reset() {
  LOG("ImageCache: Reset Write Pointers.\n");

  curr_img_cache_line = 0;
  next_img_cache_addr = 0;
}

void ImageCache::setNextChannel(data_t value) {
  if (LOG_DETAILS)
    LOG("ImageCache: setNextChannel         ICACHE[%3d] <- %.2f.\n",
        (int)next_img_cache_addr, value);

  BRAM[next_img_cache_addr] = value;
  next_img_cache_addr++;
  if (next_img_cache_addr == line_width * NUM_IMG_CACHE_LINES)
    next_img_cache_addr = 0;
}

void ImageCache::preloadPixelFromDRAM(MemoryController *DRAM) {
  LOG("ImageCache: preloadPixelFromDRAM (%2d channels)\n", (int)ch_in);

  LOG_LEVEL++;

  for (channel_t ci = 0; ci < ch_in; ci++) {
    if (loads_left == 0) {
      LOG("ImageCache: NO MORE PIXELS LEFT IN DRAM\n");
      break;
    }
    data_t px = DRAM->loadNextChannel();
    setNextChannel(px);
    loads_left--;
  }

  LOG_LEVEL--;
}

void ImageCache::preloadRowFromDRAM(MemoryController *DRAM) {
  LOG("ImageCache: preloadRowFromDRAM (%2d pixels, each %2d channels)\n",
      (int)width_in, (int)ch_in);

  LOG_LEVEL++;

  for (coordinate_t x = 0; x < width_in; x++) {
    preloadPixelFromDRAM(DRAM);
  }

  LOG_LEVEL--;
}

void ImageCache::setLayerConfig(layer_t &layer) {
  width_in = layer.width;
  ch_in = layer.channels_in;
  line_width = ch_in * width_in;
  loads_left = line_width * layer.height;

  LOG("ImageCache: setLayerConfig\n");
  LOG(" - width_in        = %d\n", (int)width_in);
  LOG(" - ch_in           = %d\n", (int)ch_in);
  LOG(" - line_width      = %d\n", (int)line_width);
  LOG(" - height_in       = %d\n", (int)layer.height);
  LOG(" - loads_left      = %d\n", (int)loads_left);

  reset();
}

data_t ImageCache::getPixel(coordinate_t y, coordinate_t x, channel_t ci) {
  assert((x >= 0) & (x < width_in) && "ImageCache: ILLEGAL PIXEL REQUESTED");

  cacheline_t req_line = (y) % NUM_IMG_CACHE_LINES;
  imgcacheaddr_t addr = req_line * width_in * ch_in + x * ch_in + ci;

  if (LOG_DETAILS)
    LOG(
        "ImageCache: getPixel( y: %d, x: %d, ci: %d ) "
        "= ICACHE[%3d] (cache line %d) -> %.2f\n",
        (int)y, (int)x, (int)ci, (int)addr, (int)req_line, BRAM[addr]);

  return BRAM[addr];
}

// =================
// = Weights Cache =
// =================
WeightsCache::WeightsCache(){};

void WeightsCache::print_setup() {
  LOG("MAX_WEIGHTS_PER_LAYER   = %d\n", (int)MAX_WEIGHTS_PER_LAYER);
  LOG("weightaddr_t write_addr = %d\n", (int)write_addr);
  LOG("MAX_WEIGHTS_PER_LAYER   = %d\n", (int)MAX_WEIGHTS_PER_LAYER);
  LOG("kernel_t kernel         = %d\n", (int)kernel);
  LOG("channel_t ch_out        = %d\n", (int)ch_out);
  LOG("channel_t ch_in         = %d\n", (int)ch_in);
  LOG("weightaddr_t ci_offset  = %d\n", (int)ci_offset);
  LOG("weights_per_filter      = %d\n", (int)weights_per_filter);
}

void WeightsCache::loadFromDRAM(MemoryController *DRAM) {
  LOG("WeightsCache: loadFromDRAM (total %d weights, %d biases)\n",
      (int)(ch_in * ch_out * weights_per_filter), (int)ch_out);
  LOG_LEVEL++;
  LOG_LEVEL++;

  weightaddr_t addr;
  // Load Filter Coefficients (1x1 Filters are expanded to 3x3)
  for (addr = 0; addr < ch_in * ch_out * weights_per_filter; addr++) {
    data_t weight;
    weight = DRAM->loadNextWeight();
    addWeight(weight);
  }
  // Load Biases
  for (addr = 0; addr < ch_out; addr++) {
    data_t bias = DRAM->loadNextWeight();
    addWeight(bias);
  }

  LOG_LEVEL--;
  LOG_LEVEL--;
}
void WeightsCache::addWeight(data_t weight) {
  assert(write_addr < MAX_WEIGHTS_PER_LAYER && "Too many weights loaded!");
  if (LOG_DETAILS)
    LOG("WeightsCache: addWeight WCACHE[%3d] = %6.2f\n", (int)write_addr,
        weight);
  BRAM[write_addr] = weight;
  write_addr++;
}
void WeightsCache::setLayerConfig(layer_t &layer) {
  kernel = layer.kernel;
  ch_in = layer.channels_in;
  ch_out = layer.channels_out;
  weights_per_filter = (kernel == 3) ? 9 : 1;
  write_addr = 0;

  LOG("WeightsCache: setLayerConfig\n");
  LOG(" - kernel          = %d\n", (int)kernel);
  LOG(" - ch_in           = %d\n", (int)ch_in);
  LOG(" - ch_out          = %d\n", (int)ch_out);
  LOG(" - w_per_filter    = %d\n", (int)weights_per_filter);
}
void WeightsCache::setInputChannel(channel_t ci) {
  ci_offset = ci * ch_out * weights_per_filter;
  LOG("WeightsCache: setInputChannel(%d), ci_offset = %d Elements, @%luB\n",
      (int)ci, (int)ci_offset, (int)ci_offset * sizeof(data_t));
}
void WeightsCache::getNineWeights(channel_t co, data_t wbuffer[9]) {
  weightaddr_t addr = ci_offset + co * weights_per_filter;
  LOG("WeightsCache: getNineWeights( co=%-2d ) from WCache[%3d]+\n", (int)co,
      (int)addr);
  wbuffer[0] = (kernel == 3) ? BRAM[addr + 0] : 0.0;
  wbuffer[1] = (kernel == 3) ? BRAM[addr + 1] : 0.0;
  wbuffer[2] = (kernel == 3) ? BRAM[addr + 2] : 0.0;
  wbuffer[3] = (kernel == 3) ? BRAM[addr + 3] : 0.0;
  wbuffer[4] = (kernel == 3) ? BRAM[addr + 4] : BRAM[addr + 0];
  wbuffer[5] = (kernel == 3) ? BRAM[addr + 5] : 0.0;
  wbuffer[6] = (kernel == 3) ? BRAM[addr + 6] : 0.0;
  wbuffer[7] = (kernel == 3) ? BRAM[addr + 7] : 0.0;
  wbuffer[8] = (kernel == 3) ? BRAM[addr + 8] : 0.0;
}
data_t WeightsCache::getOneWeight(channel_t co) {
  LOG("WeightsCache: getOneWeight( co=%-2d ) from WCache[%3d] -> %.2f\n",
      (int)co, (int)(ci_offset + co), BRAM[ci_offset + co]);
  return BRAM[ci_offset + co];
}

// ================
// = Output Cache =
// ================
OutputCache::OutputCache(const char *name) : _name(name){};
void OutputCache::accumulateChannel(channel_t c, data_t value_to_add) {
  data_t old_ch = BRAM[c];
  data_t new_ch = old_ch + value_to_add;
  BRAM[c] = new_ch;
  LOG("%s: accumulateChannel( ch%-2d ) add %+.2f -> %.2f\n", _name, (int)c,
      value_to_add, new_ch);
};
data_t OutputCache::getChannel(channel_t c) {
  if (LOG_DETAILS)
    LOG("%s: getChannel( ch%-2d ) -> %6.2f\n", _name, (int)c, BRAM[c]);
  return BRAM[c];
}
void OutputCache::setChannel(channel_t c, data_t data) {
  if (LOG_DETAILS)
    LOG("%s: setChannel( ch%-2d ) <- %6.2f\n", _name, (int)c, data);
  BRAM[c] = data;
}
void OutputCache::reset() {
  LOG("%s: reset( all ) <- 0.0\n", _name);
  LOG_LEVEL++;
  for (int i = 0; i < MAX_NUM_CHOUT; i++) BRAM[i] = 0.0;
  LOG_LEVEL--;
}

// ======================
// = Processing Element =
// ======================
ProcessingElement::ProcessingElement(){};
void ProcessingElement::setup(channel_t co_offset, ImageCache *ICache,
                              WeightsCache *WCache, OutputCache *OCache) {
  LOG("PE[%d]: setup(co_offset: %d, ICache, WCache, OCache)\n", (int)co_offset,
      (int)co_offset);
  this->ICache = ICache;
  this->WCache = WCache;
  this->OCache = OCache;
  this->co_offset = co_offset;
};

void ProcessingElement::setLayerConfig(layer_t &layer) {
  kernel = layer.kernel;
  ch_out = layer.channels_out;
  width_in = layer.width;
  height_in = layer.height;

  if (LOG_DETAILS) {
    LOG("PE[%d]: setLayerConfig\n", (int)co_offset);
    LOG(" - kernel   = %d\n", (int)kernel);
    LOG(" - ch_out   = %d\n", (int)ch_out);
    LOG(" - width_in = %d\n", (int)width_in);
  }
}

void ProcessingElement::preloadPixels(coordinate_t y_center,
                                      coordinate_t x_center, channel_t ci,
                                      data_t buffer[9]) {
  LOG("PE[%d]: preloadPixels (y_center: %2d, x_center: %2d, ci: %2d)\n",
      (int)co_offset, (int)y_center, (int)x_center, (int)ci);

  for (int k = 0; k < 3; k++) {
    for (int l = 0; l < 3; l++) {
      coordinate_t y = y_center + k - 1;
      coordinate_t x = x_center + l - 1;
      data_t px;
      bool pad = (x < 0 | y < 0 | x >= width_in | y >= height_in);
      LOG_LEVEL++;
      px = pad ? 0.0 : ICache->getPixel(y, x, ci);
      buffer[k * 3 + l] = px;
      LOG_LEVEL--;
      if (LOG_DETAILS)
        LOG(" - loaded (y: %2d, x: %2d) = %6.2f %s\n", (int)y, (int)x, px,
            (pad ? "(pad)" : ""));
    }
  }
}

void ProcessingElement::processInputChannel(coordinate_t y, coordinate_t x,
                                            channel_t ci) {
  LOG("PE[%d]: processInputChannel (y: %2d, x: %2d, ci: %2d)\n", (int)co_offset,
      (int)y, (int)x, (int)ci);
  LOG_LEVEL++;
#pragma HLS DATAFLOW
  data_t pixel_buffer[9];
  preloadPixels(y, x, ci, pixel_buffer);
  processAllCHout(pixel_buffer);
  LOG_LEVEL--;
}

void ProcessingElement::processAllCHout(data_t pixels[9]) {
  LOG("PE[%d]: processAllCHout\n", (int)co_offset);
  LOG_LEVEL++;

  data_t weights_local[9];
  data_t result;
  for (channel_t co = co_offset; co < ch_out; co += N_PE) {
    LOG(" - process output channel %d\n", (int)co);
    LOG_LEVEL++;
    {
      // fetch weights
      WCache->getNineWeights(co, weights_local);
      // multiply-accumulate
      result = macc2d(pixels, weights_local);
      // save result to Output Buffer
      OCache->accumulateChannel(co, result);
    }
    LOG_LEVEL--;
  };
  LOG_LEVEL--;
}

data_t ProcessingElement::macc2d(data_t pixels[9], data_t weights[9]) {
  data_t multresult[9];
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    multresult[i] = pixels[i] * weights[i];
  }
  if (LOG_DETAILS) {
    LOG(" - W: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        weights[0], weights[1], weights[2], weights[3], weights[4], weights[5],
        weights[6], weights[7], weights[8]);
    LOG(" - P: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        pixels[0], pixels[1], pixels[2], pixels[3], pixels[4], pixels[5],
        pixels[6], pixels[7], pixels[8]);
    LOG(" - M: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        multresult[0], multresult[1], multresult[2], multresult[3],
        multresult[4], multresult[5], multresult[6], multresult[7],
        multresult[8]);
  }
  data_t accumulator = 0.00;
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    accumulator = accumulator + multresult[i];
  }
  LOG("PE[%d]: macc2D -> %.2f \n", (int)co_offset, accumulator);

  return accumulator;
}

// ==============
// =  FPGA TOP  =
// ==============

void fpga_top(data_t *SHARED_DRAM, unsigned int num_layers,
              unsigned int weights_offset, unsigned int input_offset) {
  LOG("FPGA TOP started.\n");
  LOG_LEVEL++;

  // =============================
  // = Module + Memory Instances =
  // =============================
  MemoryController DRAM(SHARED_DRAM, weights_offset, input_offset);
  layer_t layerConfig[MAX_NUM_LAYERS];
  ImageCache ICache;
  WeightsCache WCache;
  OutputCache OCache("OCache");
  OutputCache GPoolCache("GPoolCache");
  ProcessingElement PE[N_PE];

  coordinate_t y, x;
  channel_t ci, co;
  layerid_t layer_id;

  // Setup Processing Elements
  LOG("Initial Module Setup:\n");
  LOG_LEVEL++;
  {
    for (int i = 0; i < N_PE; i++) {
      PE[i].setup(i, &ICache, &WCache, &OCache);
    }

    // Setup Global Pooling Cache
    GPoolCache.reset();

    // Load Layer Configuration
    DRAM.loadConfig(num_layers, layerConfig);
  }
  LOG_LEVEL--;

// Layer Loop
L_LAYERS:
  for (layer_id = 0; layer_id < num_layers; layer_id++) {
    LOG("Layer %d:\n", (int)layer_id);
    LOG_LEVEL++;

    // Set Layer Configuration
    layer_t layer = layerConfig[layer_id];
    LOG("Configure Modules for Layer:\n");
    LOG_LEVEL++;
    {
      ICache.setLayerConfig(layer);
      WCache.setLayerConfig(layer);
      DRAM.setLayerConfig(layer);
      for (int i = 0; i < N_PE; i++) {
        PE[i].setLayerConfig(layer);
      }
    }
    LOG_LEVEL--;

    // Load Weights from DRAM
    WCache.loadFromDRAM(&DRAM);

    // Preload Row 0
    DRAM.setPixelLoadRow(0);
    ICache.preloadRowFromDRAM(&DRAM);
    DRAM.setPixelLoadRow(1);
    ICache.preloadPixelFromDRAM(&DRAM);

  // Y Loop
  L_Y:
    for (y = 0; y < layer.height; y++) {
      LOG("Y = %d:\n", (int)y);
      LOG_LEVEL++;

    // X Loop
    L_X:
      for (x = 0; x < layer.width; x++) {
        LOG("X = %d:\n", (int)x);
        LOG_LEVEL++;

        // Reset Output Cache
        OCache.reset();

        // Load Next Pixel (automatically checks #pixels left)
        ICache.preloadPixelFromDRAM(&DRAM);

        // Stride-2 Skipping
        if (layer.stride == 2 & (x % 2 | y % 2)) {
          LOG("stride-2, skipping pixel\n");
          LOG_LEVEL--;
          continue;
        }

      // Input Channel Loop
      L_CH_IN:
        for (ci = 0; ci < layer.channels_in; ci++) {
          LOG("CI = %d:\n", (int)ci);
          LOG_LEVEL++;
          {
            // Setup Weights Cache
            WCache.setInputChannel(ci);

            // Kick off PEs
            LOG("Start PEs for Pixel(%d,%d), input ch %d, all ouput ch\n",
                (int)y, (int)x, (int)ci);
            LOG_LEVEL++;
            for (int i = 0; i < N_PE; i++) {
              PE[i].processInputChannel(y, x, ci);
            }
            LOG_LEVEL--;
          }
          LOG_LEVEL--;
        }  // end L_CH_IN. Pixel (Y,X) is finished.
        LOG("All CI, CO done for pixel(%d,%d)\n", (int)y, (int)x);

        // ===============
        // = Postprocess =
        // ===============
        data_t raw, biased, rectified;

        LOG("Postprocess Pixel(%d,%d)\n", (int)y, (int)x);
        LOG_LEVEL++;

        // Select bias coefficients
        WCache.setInputChannel(layer.channels_in);

      L_POSTPROCESS:
        for (co = 0; co < layer.channels_out; co++) {
          LOG("Postprocess CO=%d:\n", (int)co);
          LOG_LEVEL++;

#pragma HLS pipeline
          // Read output channel from Cache
          raw = OCache.getChannel(co);
          // Add Bias
          biased = raw + WCache.getOneWeight(co);
          // Add ReLU
          rectified = (biased < 0) ? 0.0 : biased;

          LOG("raw: %8.2f > biased: %8.2f > rectified: %8.2f\n", raw, biased,
              rectified);
          // Write Back to Output Cache
          OCache.setChannel(co, rectified);
          // Accumulate for Global Pooling (if enabled)
          if (layer.pool == POOL_GLOBAL) {
            GPoolCache.accumulateChannel(co, rectified);
          }
          LOG_LEVEL--;
        }
        LOG_LEVEL--;

        // Write Output Pixel to DRAM
        dimension_t y_out = (layer.stride == 2) ? (int)y / 2 : (int)y;
        dimension_t x_out = (layer.stride == 2) ? (int)x / 2 : (int)x;
        DRAM.writeBackOutputPixel(y_out, x_out, OCache.BRAMPointer());

        LOG_LEVEL--;
      }
      LOG_LEVEL--;
    }
    LOG_LEVEL--;
  }
  LOG_LEVEL--;

  // Write Back final Result
  DRAM.writeBackResult(GPoolCache.BRAMPointer());
  LOG_LEVEL--;
}