#ifndef UTIL_H
#define UTIL_H

#include <fstream>
#include <stdint.h>
#include <string>

uint32_t byte_array_to_int (char* bytes);
uint16_t byte_array_to_short (char* bytes);

uint32_t read_int (std::ifstream *file);
uint16_t read_short (std::ifstream *file);

bool str_ends_with (std::string str, std::string suffix);

int get_max (int a, int b);
int get_min (int a, int b);

#endif