#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>
#include <string.h>
#include <stdint.h>

#include "util.h"
#include "lcs.h"
#include "pokeid.h"
#include "pokedatastructure.h"

using namespace std;

const char* rom_file_path = "rom.nds";

// DFS through subtable, return a map from filepath to FAT offset
void explore_subtable (uint32_t subtable_offset, uint32_t fnt_offset, string path, uint16_t *file_id, map<string,uint16_t> *file_locations) {
    ifstream rom_file;
    rom_file.open(rom_file_path);

    rom_file.seekg(fnt_offset + subtable_offset, ios::beg);

    // get first byte
    char length_type_byte = 0;
    rom_file.read(&length_type_byte, 1);

    uint32_t length_type = (uint32_t) (unsigned char)length_type_byte;

    while (length_type != 0) {
        if (length_type & 0x80) {
            // this is a directory
            // get name, appended to end of path
            char *new_path_bytes = new char[path.length() + length_type - 0x80 + 2];
            strcpy(new_path_bytes, path.c_str());
            rom_file.read(new_path_bytes + path.length(), length_type - 0x80);
            
            new_path_bytes [path.length() + length_type - 0x80] = '/';
            new_path_bytes[path.length() + length_type - 0x80+1] = '\0';

            string new_path(new_path_bytes);

            // after name, offset for subdirectory from start of fnt
            uint32_t subdir_id = (uint32_t) read_short(&rom_file);

            int pos_in_subtable = rom_file.tellg();

            rom_file.seekg(fnt_offset + (subdir_id & 0xFFF)*8, ios::beg);

            uint32_t new_subtable = read_int(&rom_file);

            uint16_t first_file_id = read_short(&rom_file);

            // now, explore this subtable
            explore_subtable (new_subtable, fnt_offset, new_path, &first_file_id, file_locations);

            // move filestream back to continue while loop
            rom_file.seekg(pos_in_subtable, ios::beg);

            delete [] new_path_bytes;
        } else {
            char *filename_bytes = new char[length_type + 1];
            rom_file.read(filename_bytes, length_type);
            filename_bytes[length_type] = '\0';

            string filename_str(filename_bytes);
            string full_path_str(path);

            full_path_str += filename_str;

            // cout << full_path_str << endl;

            file_locations->insert(pair<string,uint16_t>(full_path_str, *file_id));
            *file_id += 1;
        }
        
        rom_file.read(&length_type_byte, 1);
        length_type = (uint32_t) (unsigned char)length_type_byte;
    }

    rom_file.close();
}

map<string,uint16_t> get_file_locations () {
    // https://web.archive.org/web/20110718184246/http://nocash.emubase.de/gbatek.htm#dsmemorymaps
    // mapping of filenames (strings) to FAT offsets
    map<string,uint16_t> file_locations;

    ifstream rom_file;
    rom_file.open(rom_file_path);

    rom_file.seekg(0x40, ios::beg);
    uint32_t fnt_offset = read_int(&rom_file);

    rom_file.seekg(0x48, ios::beg);
    uint32_t fat_offset = read_int(&rom_file);

    cout << "FNT offset: " << hex << fnt_offset << endl;
    cout << "FAT offset: " << hex << fat_offset << endl;

    // Go to start of fnt
    rom_file.seekg(fnt_offset, ios::beg);

    uint32_t subtable_offset = read_int(&rom_file);
    uint16_t first_file_id = read_short(&rom_file);

    cout << "First subtable offset: " << hex << subtable_offset << endl;
    cout << "First file id: " << hex << first_file_id << endl;

    explore_subtable(subtable_offset, fnt_offset, "", &first_file_id, &file_locations);

    rom_file.close();
    return file_locations;
}

// https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski
// https://magikos.livejournal.com/7375.html?
// https://github.com/SciresM/FEAT/blob/master/FEAT/DSDecmp/Formats/Nitro/LZ10.cs#L83
char* decompress_LZ10 (char *compressed_bytes, uint32_t compressed_size) {
    assert(compressed_size >= 4);

    // First byte 0x10 means file is LZ10 compressed
    assert(*compressed_bytes == 0x10);

    // next three bytes are size of uncompressed file
    uint32_t uncompressed_size = (uint8_t) *(compressed_bytes + 3) << 16 |
        (uint8_t) *(compressed_bytes + 2) << 8 |
        (uint8_t) *(compressed_bytes + 1);

    char* uncompressed_file = new char[uncompressed_size];

    uint32_t input_pos = 4;
    uint32_t output_pos = 0;

    while (input_pos < compressed_size) {
        uint8_t flag_bytes = (uint8_t) *(compressed_bytes + (input_pos++));

        for (int i = 0; i < 8; i ++) {
            bool flag_bit = (flag_bytes >> i) & 1;
            if (flag_bit) {
                // Dictionary entry
                uint8_t token1 = (uint8_t) *(compressed_bytes + (input_pos++));
                uint8_t token2 = (uint8_t) *(compressed_bytes + (input_pos++));
                
                uint16_t disp = ((token1 & 0xF) << 8) | token2;
                uint8_t length = (token1 >> 4) + 3; // plus 3 for some reason?

                uint16_t read_start = output_pos - disp - 1;

                for (int j = 0; j < length; j++) {
                    *(uncompressed_file + (output_pos++)) = *(uncompressed_file + read_start + i);
                }
            } else {
                // Raw byte
                *(uncompressed_file + (output_pos++)) = *(compressed_bytes + (input_pos++));
            }
        }
    }

    return uncompressed_file;
}

uint32_t compress_LZ10 (char *uncompressed_data, uint32_t uncompressed_size, unsigned char **compressed_data) {
    vector<uint8_t> buffer;

    buffer.push_back(0x10);
    buffer.push_back(0);
    buffer.push_back(0);
    buffer.push_back(0);

    // int max_disp = 0xFFFF;
    // int max_length = 0xFFF + 3;

    uint32_t cur_position = 4;

    while (cur_position < uncompressed_size) {
        // https://en.wikipedia.org/wiki/Longest_common_substring
        // get longest common substring (up to max length of dicitionary entry)
        // if passes threshold (length >= 3), make it a dictionary entry
        // otherwise just copy raw bytes

        buffer.push_back(0); // flag byte
        int flag_index = buffer.size() - 1;

        uint8_t flag_byte = 0;

        // int current_compress_idx = buffer.size() - 1;

        for (int i = 0; i < 8; i++) {

            // get longest common substring
            
            // int start_idx = get_max(0, cur_position - max_disp - 1);
            // int end_idx = get_min(cur_position + max_length, uncompressed_size);

            // vector<uint8_t> lcs = longest_common_substring(start_idx, cur_position, cur_position, end_idx, uncompressed_data);

            // if (lcs.size() < 3) {
            //     // store as raw byte

            //     cur_position ++;
            // } else {
            //     // store as dictionary entry

            //     flag_byte |= (1 << 8-i);
            // }

        }

        buffer[flag_index] = flag_byte;
    }

    uint32_t compressed_size = buffer.size();

    // load uncompressed size into buffer
    buffer[1] = (uint8_t) (uncompressed_size & 0xFF);
    buffer[2] = (uint8_t) ((uncompressed_size >> 8) & 0xFF);
    buffer[3] = (uint8_t) ((uncompressed_size >> 16) & 0xFF);


    *compressed_data = buffer.data();

    return compressed_size;
}

void process_map_files (PokeDataStructure* pds, map<string, uint16_t> file_locations, uint32_t fat_offset) {
    map<string, uint16_t> map_dat_files;

    ifstream rom_file(rom_file_path);

    for (auto it = file_locations.begin(); it != file_locations.end(); ++it) {
        if (str_ends_with(it->first, ".map.dat.lz")) {
            map_dat_files.insert(*it);
            // cout << it->first << " " << hex << it->second << endl;
        }

        uint32_t file_in_fat = fat_offset + it->second*8;
        rom_file.seekg(file_in_fat, ios::beg);
        uint32_t file_start = read_int(&rom_file);
        uint32_t file_end = read_int(&rom_file);

        assert(file_start % 512 == 0);

        if (file_end % 512 == 0) {
            cout << "check if no padding after " << it->first << ": " << hex << file_start << "," << file_end << endl;
        }
    }


    // uint32_t map_file_in_fat = fat_offset + file_locations["data/field/map/m038_022.map.dat.lz"]*8;
    // rom_file.seekg(map_file_in_fat, ios::beg);
    // uint32_t map_file_start = read_int(&rom_file);
    // uint32_t map_file_end = read_int(&rom_file);

    // uint32_t file_size = map_file_end - map_file_start;

    // char* map_file_bytes = new char[file_size];
    // rom_file.seekg(map_file_start, ios::beg);
    // rom_file.read(map_file_bytes, file_size);

    // char* uncompressed_file = decompress_LZ10(map_file_bytes, file_size);

    // delete[] map_file_bytes;
    // delete[] uncompressed_file;


    // TODO decompress .map.dat.lz files (LZ10 compression)
    // https://github.com/SunakazeKun/AlmiaE/blob/master/src/com/aurum/almia/game/Compression.java
    // https://ndspy.readthedocs.io/en/latest/_modules/ndspy/lz10.html#decompress

    rom_file.close();
}

int main () {
    // map<string,uint16_t> file_locations = get_file_locations ();
    
    // ifstream rom_file(rom_file_path);

    // rom_file.seekg(0x48, ios::beg);
    // uint32_t fat_offset = read_int(&rom_file);

    // uint32_t pkmn_name_in_fat =  fat_offset + file_locations["data/message/etc/pokemon_name_us.mes"]*8;
    // uint32_t field_move_in_fat = fat_offset + file_locations["data/message/etc/fieldwaza_name_us.mes"]*8;
    // uint32_t pokeid_bin_in_fat = fat_offset + file_locations["data/param/PokeID.bin"]*8;

    // rom_file.seekg(pkmn_name_in_fat, ios::beg);
    // uint32_t pkmn_name_offset = read_int(&rom_file);

    // rom_file.seekg(field_move_in_fat, ios::beg);
    // uint32_t field_move_offset = read_int(&rom_file);

    // rom_file.seekg(pokeid_bin_in_fat, ios::beg);
    // uint32_t pokeid_bin_offset = read_int(&rom_file);

    // cout << "Pokemon name offset: " << pkmn_name_offset << endl;
    // cout << "Field name offset: " << field_move_offset << endl;
    // cout << "PokeID bin offset: " << pokeid_bin_offset << endl;

    // vector<string> pkmn_names = get_pokemon_names_rom (rom_file_path, 
    //     pkmn_name_offset);
    // vector<string> field_moves = get_field_moves_rom (rom_file_path, 
    //     field_move_offset);

    // // cout << pkmn_names.size() << endl;

    // PokeDataStructure pds;

    // // TODO: make this a PDS member function
    // read_pokeID_rom (&pds, rom_file_path, pokeid_bin_offset);

    // // uint16_t poke_id = 411;

    // // vector<uint16_t> res = pds.get_pokemon_with_geq_field_move(poke_id);

    // // pair<uint8_t, uint8_t> start_field_info = pds.get_field_move(poke_id);
    // // cout << "Pokemon that can replace " << pkmn_names[poke_id] << " (" << 
    // //     field_moves[start_field_info.first] << " " << dec 
    // //     << (uint32_t) start_field_info.second << "):" << endl;

    // // for (auto it = res.begin(); it != res.end(); ++it) {
    // //     pair<uint8_t, uint8_t> field_info = pds.get_field_move(*it);
    // //     cout << pkmn_names[*it] << " (" << field_moves[field_info.first] << " "
    // //         << dec << (uint32_t) field_info.second << ")" << endl;
    // // }

    // // cout << endl;
    // // pds.print_level_order();

    // // pds.self_test();

    // process_map_files(&pds, file_locations, fat_offset);

    // rom_file.close();

    unsigned char thing[] = "papaya";

    int *s = new int[6];

    for (int i = 0; i < 6; i++) {
        s[i] = (int) (unsigned char) thing[i]; 
    }

    int *sa = new int[6];
    int *LCP = new int[6]; 

    suffix_array(s, sa, LCP, 6, 255);

    for (int i = 0; i < 6; i++) 
        cout << sa[i] << " ";

    cout << endl << "LCP: ";

    for (int i = 0; i < 6-1; i++) 
        cout << LCP[i] << " ";

    cout << endl;

    delete [] s; delete [] sa; delete [] LCP;

    return 0;
}