#ifndef POKEID_H
#define POKEID_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <map>
#include <string>
#include <stdint.h>

#include "util.h"
#include "pokedatastructure.h"

std::vector<std::string> read_mes_from_rom (const char* path, uint32_t offset);

std::vector<std::string> get_pokemon_names_rom (const char *path, uint32_t offset);
std::vector<std::string> get_field_moves_rom (const char *path, uint32_t offset);

void read_pokeID_rom (PokeDataStructure *pds, const char* rom_path, 
    uint32_t offset);


#endif