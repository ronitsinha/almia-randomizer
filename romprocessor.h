#ifndef ROMPROCESSOR_H
#define ROMPROCESSOR_H

#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <ctime>

#include "pokedatastructure.h"
#include "util.h"

// For PokeID.bin: each pokemon entry is 28 bytes long
// from: https://github.com/SunakazeKun/AlmiaE/blob/6af31527f579037b71fe3461309820f481c42be9/src/com/aurum/almia/game/param/PokeID.java
#define ENTRY_SIZE 0x1C
#define UNIQUE_SIZE 24

class RomProcessor {
public:
    RomProcessor(const char *in_path, const char *out_path);
    ~RomProcessor();

    void randomize_rom();    

private:
    std::ifstream rom_file; std::fstream out_file;
    std::map<std::string, uint16_t> file_locations;
    PokeDataStructure pds;

    uint32_t fat_offset;
    char UNIQUE[24] = {
        0x07, 0x03, 0x01, 0x07, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01, 0x02,
        0x02, 0x01, 0x01, 0x02, 0x02, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x05
    };

    void get_file_locations();
    void read_pokeID_rom (uint32_t offset);
    void process_map_files ();

    void explore_subtable (uint32_t subtable_offset, uint32_t fnt_offset, 
        std::string path, uint16_t *file_id);

};

#endif