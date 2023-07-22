#ifndef UTIL_H
#define UTIL_H

#include <fstream>
#include <stdint.h>
#include <string>

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

#endif