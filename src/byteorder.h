#ifndef _headers_1667127626_Fluff_Assembler_byteorder
#define _headers_1667127626_Fluff_Assembler_byteorder

#include <stdint.h>
#include <stddef.h>

// There no reason to allow arithmetic on
// different endian integer
typedef struct {
  uint8_t data[2];
} le16;
typedef struct {
  uint8_t data[4];
} le32;
typedef struct {
  uint8_t data[8];
} le64;
typedef struct {
  uint8_t data[2];
} be16;
typedef struct {
  uint8_t data[4];
} be32;
typedef struct {
  uint8_t data[8];
} be64;

// Endianess agnostic converters
static inline uint16_t le16_to_cpu(le16 val) {
  return val.data[1] << 8 | val.data[0];
}

static inline le16 cpu_to_le16(uint16_t val) {
  return (le16) {
    .data = {val & 0xFF, val >> 8 & 0xFF}
  };
}

static inline uint16_t be16_to_cpu(be16 val) {
  return val.data[0] << 8 | val.data[1];
}

static inline be16 cpu_to_be16(uint16_t val) {
  return (be16) {
    .data = {val >> 8 & 0xFF, val & 0xFF}
  };
}

static inline uint32_t le32_to_cpu(le32 val) {
  return val.data[3] << 24 | val.data[2] << 16 |
         val.data[1] << 8 | val.data[0];
}

static inline le32 cpu_to_le32(uint32_t val) {
  return (le32) {
    .data = {val & 0xFF, val >> 8 & 0xFF}
  };
}

static inline uint32_t be32_to_cpu(be32 val) {
  return val.data[0] << 24 | val.data[1] << 16 |
         val.data[2] << 8 | val.data[3];
}

static inline be32 cpu_to_be32(uint32_t val) {
  return (be32) {
    .data = {val >> 24 & 0xFF, val >> 16 & 0xFF, 
             val >> 8 & 0xFF, val & 0xFF}
  };
}

static inline uint64_t le64_to_cpu(le64 val) {
  return ((uint64_t) val.data[7]) << 56 | ((uint64_t) val.data[6]) << 48 |
         ((uint64_t) val.data[5]) << 40 | ((uint64_t) val.data[4]) << 32|
         ((uint64_t) val.data[3]) << 24 | ((uint64_t) val.data[2]) << 16 |
         ((uint64_t) val.data[1]) << 8 | ((uint64_t) val.data[0]);
}

static inline le64 cpu_to_le64(uint64_t val) {
  return (le64) {
    .data = {val & 0xFF, val >> 8 & 0xFF, 
             val >> 16 & 0xFF, val >> 24 & 0xFF,  
             val >> 32 & 0xFF, val >> 40 & 0xFF, 
             val >> 48 & 0xFF, val >> 56 & 0xFF}
  };
}

static inline uint64_t be64_to_cpu(be64 val) {
  return ((uint64_t) val.data[0]) << 56 | ((uint64_t) val.data[1]) << 48 |
         ((uint64_t) val.data[2]) << 40 | ((uint64_t) val.data[3]) << 32|
         ((uint64_t) val.data[4]) << 24 | ((uint64_t) val.data[5]) << 16 |
         ((uint64_t) val.data[6]) << 8 | ((uint64_t) val.data[7]);
}

static inline be64 cpu_to_be64(uint64_t val) {
  return (be64) {
    .data = {val >> 56 & 0xFF, val >> 48 & 0xFF, 
             val >> 40 & 0xFF, val >> 32 & 0xFF,
             val >> 24 & 0xFF, val >> 16 & 0xFF, 
             val >> 8 & 0xFF, val & 0xFF}
  };
}

static inline void le16_add_cpu(le16* var, uint16_t val) {
  *var = cpu_to_le16(le16_to_cpu(*var) + val);
}

static inline void le32_add_cpu(le32* var, uint32_t val) {
  *var = cpu_to_le32(le32_to_cpu(*var) + val);
}

static inline void le64_add_cpu(le64* var, uint64_t val) {
  *var = cpu_to_le64(le64_to_cpu(*var) + val);
}

static inline void be16_add_cpu(be16* var, uint16_t val) {
  *var = cpu_to_be16(be16_to_cpu(*var) + val);
}

static inline void be32_add_cpu(be32* var, uint32_t val) {
  *var = cpu_to_be32(be32_to_cpu(*var) + val);
}

static inline void be64_add_cpu(be64* var, uint64_t val) {
  *var = cpu_to_be64(be64_to_cpu(*var) + val);
}

static inline void cpu_to_be32_array(be32* dst, const uint32_t* src, size_t len) {
  size_t i;

  for (i = 0; i < len; i++)
    dst[i] = cpu_to_be32(src[i]);
}

static inline void be32_to_cpu_array(uint32_t* dst, const be32*src, size_t len) {
  size_t i;

  for (i = 0; i < len; i++)
    dst[i] = be32_to_cpu(src[i]);
}

static inline void cpu_to_le32_array(le32* dst, const uint32_t* src, size_t len) {
  size_t i;

  for (i = 0; i < len; i++)
    dst[i] = cpu_to_le32(src[i]);
}

static inline void le32_to_cpu_array(uint32_t* dst, const le32* src, size_t len) {
  size_t i;

  for (i = 0; i < len; i++)
    dst[i] = le32_to_cpu(src[i]);
}

#endif

