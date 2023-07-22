#include "util.h"

using namespace std;

// little endian
uint32_t byte_array_to_int (char* bytes, bool big_endian) {
    uint32_t res = 0;

    for (int i = 0; i < 4; i ++) {
        res |= (uint32_t) *(unsigned char *)(bytes + i) << 8*(big_endian ? 3-i : i);
    }

    return res;
}

uint16_t byte_array_to_short (char* bytes, bool big_endian) {
    uint16_t res = 0;

    for (int i = 0; i < 2; i ++) {
        res |= (uint16_t) *(unsigned char *)(bytes + i) << 8*(big_endian ? 1-i : i);
    }

    return res;
}

uint32_t read_int (ifstream *file) {
    char bytes[4];
    file->read(&bytes[0], 4);

    return byte_array_to_int(&bytes[0]);
}

uint16_t read_short (ifstream *file) {
    char bytes[2];
    file->read(&bytes[0], 2);

    return byte_array_to_short(&bytes[0]);
}

void write_int(std::ofstream *file, uint32_t n) {
    uint8_t little_endian[] = { (uint8_t)(n & 0xFF),
        (uint8_t)((n >> 8) & 0xFF), 
        (uint8_t)((n >> 16) & 0xFF), 
        (uint8_t)((n >> 24) & 0xFF)};

    file->write((char*) little_endian, 4);
}

bool str_ends_with (std::string str, std::string suffix) {
    if (str.length() < suffix.length()) return false;

    for (unsigned int i = 1; i <= suffix.length(); i++) {
        if (suffix[suffix.length() - i] != str[str.length() - i]) return false;
    }

    return true;
}

int get_max (int a, int b) {
    return (a > b)? a : b;
}

int get_min (int a, int b) {
    return (a > b)? b : a;
}

uint32_t round_to_multiple (uint32_t n, uint32_t multiple) {
    if (multiple == 0) return n;

    uint32_t remainder = n % multiple;
    if (remainder == 0) return n;

    return n + multiple - remainder;
}

// https://github.com/pleonex/Ninokuni/blob/master/Programs/NinoPatcher/NinoPatcher/Crc16.cs
// https://web.archive.org/web/20110718184246/http://nocash.emubase.de/gbatek.htm#biosmiscfunctions
// https://github.com/Zetten/hachoir/blob/master/hachoir-parser/hachoir_parser/program/nds.py
uint16_t compute_crc16 (uint8_t *bytes, int size) {
    uint16_t val[8] = {0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001};
    uint16_t crc = 0xFFFF;

    for (int i = 0; i < size; i++) {
        crc ^= bytes[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else         crc >>= 1;
            // crc = (crc & 1) ? (crc >> 1) ^ (val[j] << (7-j)) : (crc >> 1);
        }
    }   

    return crc;
}