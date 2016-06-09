#include "output_cache.hpp"

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