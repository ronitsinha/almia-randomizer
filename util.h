#ifndef UTIL_H
#define UTIL_H

#include <fstream>
#include <vector>
#include <stdint.h>
#include <string>
#include <cassert>

#define NARC_MAGIC 0x4E415243
#define NARC_FATB  0x46415442
#define NARC_FNTB  0x464E5442
#define NARC_FIMG  0x46494D47
#define LYR_MAGIC  0x4C595200

uint32_t byte_array_to_int (char* bytes, bool big_endian=false);
uint16_t byte_array_to_short (char* bytes, bool big_endian=false);

uint32_t read_int (std::ifstream *file);
uint16_t read_short (std::ifstream *file);

void write_int(std::ofstream *file, uint32_t n);

bool str_ends_with (std::string str, std::string suffix);

int get_max (int a, int b);
int get_min (int a, int b);

uint32_t round_to_multiple (uint32_t n, uint32_t multiple);

uint16_t compute_crc16 (uint8_t *bytes, int size);

uint32_t decompress_LZ10 (char *compressed_bytes, uint32_t compressed_size,
    uint8_t **uncompressed_data);

uint32_t compress_LZ10 (uint8_t *uncompressed_data, uint32_t uncompressed_size,
    std::vector<uint8_t> *compressed_data);

std::vector<std::pair<uint32_t,uint32_t>> decode_NARC (uint8_t *data);


#endif