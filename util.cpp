#include "util.h"

using namespace std;

// little endian
uint32_t byte_array_to_int (char* bytes) {
    uint32_t res = 0;

    for (int i = 0; i < 4; i ++) {
        res |= (uint32_t) *(unsigned char *)(bytes + i) << 8*i;
    }

    return res;
}

uint16_t byte_array_to_short (char* bytes) {
    uint16_t res = 0;

    for (int i = 0; i < 2; i ++) {
        res |= (uint16_t) *(unsigned char *)(bytes + i) << 8*i;
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
