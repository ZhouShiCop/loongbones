// 最小 SHA-256（FIPS 180-4）：只服务 `sourceRef.contentHash`，无第三方依赖。
//
// 说明：这是**校验和工具**而非安全原语 —— 不用于口令、签名或抗碰撞场景。
// 仓库不得引入加密库或新构建依赖（Task4_Implementation_Plan.md §6），故自实现。

#include "loongbones/importers/api/sha256.hpp"

#include <algorithm>
#include <cstring>

namespace loongbones::importers::api {
namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept {
  return (x >> n) | (x << (32u - n));
}

void compressBlock(std::uint32_t h[8], const std::uint8_t p[64]) noexcept {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    const std::size_t o = static_cast<std::size_t>(i) * 4u;
    w[i] = (static_cast<std::uint32_t>(p[o]) << 24) | (static_cast<std::uint32_t>(p[o + 1]) << 16) |
           (static_cast<std::uint32_t>(p[o + 2]) << 8) | static_cast<std::uint32_t>(p[o + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

  for (int i = 0; i < 64; ++i) {
    const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ (~e & g);
    const std::uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
    const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = S0 + maj;
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
  h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

}  // namespace

std::string sha256Hex(const std::uint8_t* data, std::size_t size) {
  std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

  const std::uint8_t* p = data;
  std::size_t remaining = (data != nullptr) ? size : 0u;

  while (remaining >= 64) {
    compressBlock(h, p);
    p += 64;
    remaining -= 64;
  }

  // 尾部块：剩余字节 + 0x80 + 补 0 + 64 位大端长度。
  std::uint8_t block[64];
  std::memcpy(block, p, remaining);
  block[remaining] = 0x80;
  std::memset(block + remaining + 1, 0, 64 - remaining - 1);

  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8u;
  auto writeLength = [](std::uint8_t* dst, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
      dst[56 + i] = static_cast<std::uint8_t>((value >> (56 - 8 * i)) & 0xffu);
    }
  };

  // 剩余 >= 56 时，长度字段塞不进当前块，需多出一个块。
  if (remaining >= 56) {
    compressBlock(h, block);
    std::memset(block, 0, sizeof(block));
    writeLength(block, bits);
    compressBlock(h, block);
  } else {
    writeLength(block, bits);
    compressBlock(h, block);
  }

  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (int i = 0; i < 8; ++i) {
    for (int b = 3; b >= 0; --b) {
      const std::uint8_t byte = static_cast<std::uint8_t>((h[i] >> (b * 8)) & 0xffu);
      out.push_back(kHex[byte >> 4]);
      out.push_back(kHex[byte & 0x0fu]);
    }
  }
  return out;
}

std::string sha256Hex(const std::vector<std::uint8_t>& bytes) {
  return sha256Hex(bytes.data(), bytes.size());
}

}  // namespace loongbones::importers::api
