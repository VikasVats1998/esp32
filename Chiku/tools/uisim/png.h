// png.h - tiny PNG writer (no compression, no libraries) for the simulator screenshots.
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <vector>

namespace png {
static uint32_t crcTable[256];
static void initCrc() {
  for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcTable[n] = c; }
}
static uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) { for (size_t i = 0; i < n; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >> 8); return c; }
static void be32(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
static void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> v;
  be32(v, data.size());
  v.insert(v.end(), type, type + 4);
  v.insert(v.end(), data.begin(), data.end());
  be32(v, crc(v.data() + 4, v.size() - 4) ^ 0xFFFFFFFFu);
  fwrite(v.data(), 1, v.size(), f);
}
// rgb: w*h*3 bytes
inline bool write(const char* path, const uint8_t* rgb, int w, int h) {
  if (!crcTable[1]) initCrc();
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> hdr;
  be32(hdr, w); be32(hdr, h);
  hdr.insert(hdr.end(), {8, 2, 0, 0, 0});
  chunk(f, "IHDR", hdr);
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb + y * w * 3, rgb + (y + 1) * w * 3); }
  std::vector<uint8_t> z = {0x78, 0x01};
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  for (size_t i = 0; i < raw.size(); i += 65535) {      // stored deflate blocks
    size_t n = std::min<size_t>(65535, raw.size() - i);
    z.push_back(i + n == raw.size());
    z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
    z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
  }
  be32(z, (b << 16) | a);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  fclose(f);
  return true;
}
}  // namespace png
