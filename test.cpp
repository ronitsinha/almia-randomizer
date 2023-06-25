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

            delete[] filename_bytes;
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
uint32_t decompress_LZ10 (char *compressed_bytes, uint32_t compressed_size, uint8_t **uncompressed_data) {
    assert(compressed_size >= 4);

    // First byte 0x10 means file is LZ10 compressed
    assert(*compressed_bytes == 0x10);

    // next three bytes are size of uncompressed file
    uint32_t uncompressed_size = (uint8_t) *(compressed_bytes + 3) << 16 |
        (uint8_t) *(compressed_bytes + 2) << 8 |
        (uint8_t) *(compressed_bytes + 1);

    cout << "uncompressed_size: " << uncompressed_size << endl;

    uint8_t* uncompressed_file = new uint8_t[uncompressed_size];

    uint32_t input_pos = 4;
    uint32_t output_pos = 0;

    while (output_pos < uncompressed_size) {
        assert(input_pos < compressed_size);
        uint8_t flag_byte = (uint8_t) compressed_bytes[input_pos++];

        for (int i = 1; i <= 8 and output_pos < uncompressed_size and input_pos < compressed_size; i ++) {
            bool flag_bit = (flag_byte >> 8-i) & 1;
            if (flag_bit) {
                // Dictionary entry
                uint8_t token1 = (uint8_t) compressed_bytes[input_pos++];
                uint8_t token2 = (uint8_t) compressed_bytes[input_pos++];
                
                int disp = ((token1 & 0x0F) << 8) | token2; disp += 1;
                uint8_t length = (token1 >> 4) + 3; // plus 3 for some reason?

                int read_start = output_pos - disp;

                assert (disp <= output_pos);

                for (int j = 0; j < length; j++) {
                    uncompressed_file[output_pos++] = uncompressed_file[read_start + (j % disp)];
                }
            } else {
                // Raw byte
                uncompressed_file[output_pos++] = compressed_bytes[input_pos++];
            }
        }
    }

    *uncompressed_data = uncompressed_file;
    return uncompressed_size;
}

uint32_t compress_LZ10 (uint8_t *uncompressed_data, uint32_t uncompressed_size, vector<uint8_t> *compressed_data, LCS* lcs_helper) {
    vector<uint8_t> buffer;

    buffer.push_back(0x10);
    buffer.push_back(0); /* TODO: load uncompressed size into these three bytes */
    buffer.push_back(0);
    buffer.push_back(0);

    int max_disp = 0xFFF;
    int max_length = 0xF + 3;

    uint32_t cur_position = 0;

    // LCS *lcs_helper = new LCS; // used for computing dictionary entries
    // lcs_helper->initialize(uncompressed_data, uncompressed_size);

    while (cur_position < uncompressed_size) {
        // https://en.wikipedia.org/wiki/Longest_common_substring
        // get longest common substring (up to max length of dictionary entry)
        // if passes threshold (length >= 3), make it a dictionary entry
        // otherwise just copy raw bytes

        buffer.push_back(0); // flag byte
        int flag_index = buffer.size() - 1;

        uint8_t flag_byte = 0;

        for (int i = 1; i <= 8 and cur_position < uncompressed_size; i++) {

            // get longest common substring
            int start_idx = get_max(0, cur_position - max_disp - 1);
            int length_from_start = cur_position - start_idx;
            int length_from_current = uncompressed_size - cur_position;

            int best_run_length = 0;
            int best_run_start = start_idx;
            int cur_run_start = start_idx;

            for (int j = start_idx; j < cur_position and best_run_length < max_length; j++) {
                int length = get_min(cur_position - j, lcs_helper->get_lcp(j, cur_position));
                if (j + length >= cur_position) { // repeat string
                    int k = cur_position + length;
                    int l = 0;
                    while (k < uncompressed_size && uncompressed_data[k] == uncompressed_data[j + l] && length < max_length) {
                        length ++;
                        k ++; l = (l + 1) % (cur_position - j);
                    }
                }

                if (best_run_length < length) {
                    best_run_length = get_min(max_length, length);
                    best_run_start = j;
                }
            }

            // TODO: precompute SA and LCP for entire file, then do constant time RMQs for each value in window
            // for (int j = start_idx; j < cur_position and best_run_length < max_length; j++) {
            //     int length = 0, k = 0;
            //     while (cur_position + k < uncompressed_size && uncompressed_data[j+k] == uncompressed_data[cur_position+k] && length < max_length) {
            //         length ++; k++;
            //     }

            //     if (j + length >= cur_position) {
            //         int l = cur_position + length;
            //         int m = 0;
            //         while (l < uncompressed_size && uncompressed_data[l] == uncompressed_data[j+m] && length < max_length) {
            //             length++; l++;
            //             m = (m+1) % (cur_position-j);
            //         }
            //     }

            //     if (length > best_run_length) {
            //         best_run_length = get_min(length, max_length);
            //         best_run_start = j;
            //     }
            // }

            // if (start_idx < cur_position) {
            //     while (cur_run_start < cur_position) {
            //         int this_run = 0;
            //         int k = 0;
            //         while (cur_position + k < uncompressed_size && uncompressed_data[cur_run_start + k] == uncompressed_data[cur_position + k]) {
            //             this_run ++;
            //             k ++;
            //         }
            //         if (cur_run_start + this_run >= cur_position) {
            //             int r = cur_position + this_run;
            //             int s = 0;
            //             while (r < uncompressed_size and uncompressed_data[r] == uncompressed_data[cur_run_start + s]) {
            //                 this_run ++;
            //                 r ++;
            //                 s = (s + 1) % (cur_position - cur_run_start);
            //             }
            //         }

            //         if (this_run > best_run_length) {
            //             best_run_length = get_min(max_length, this_run);
            //             best_run_start = cur_run_start;
            //         }

            //         cur_run_start ++;
            //     }
            // }

            if (best_run_length < 3) // store as raw byte 
                buffer.push_back(uncompressed_data[cur_position++]);
            else { // store as dictionary entry
                uint16_t disp = cur_position - best_run_start -1;
                
                uint8_t length = best_run_length -3;
                uint8_t token1 = ((disp >> 8) & 0x0F) | ((length << 4) & 0xF0);
                uint8_t token2 = disp & 0xFF; 

                buffer.push_back(token1); buffer.push_back(token2);

                flag_byte |= (1 << 8-i); // set corresponding flag bit
                cur_position += best_run_length;
            }

        }

        buffer[flag_index] = flag_byte;
    }

    uint32_t compressed_size = buffer.size();

    // load uncompressed size into buffer
    buffer[1] = (uint8_t) (uncompressed_size & 0xFF);
    buffer[2] = (uint8_t) ((uncompressed_size >> 8) & 0xFF);
    buffer[3] = (uint8_t) ((uncompressed_size >> 16) & 0xFF);

    *compressed_data = buffer;

    // delete lcs_helper;

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

        // assert(file_start % 512 == 0);

        // if (file_end % 512 == 0) {
        //     cout << "check if no padding after " << it->first << ": " << hex << file_start << "," << file_end << endl;
        // }
    }


    uint32_t map_file_in_fat = fat_offset + file_locations["data/field/map/m038_022.map.dat.lz"]*8;
    rom_file.seekg(map_file_in_fat, ios::beg);
    uint32_t map_file_start = read_int(&rom_file);
    uint32_t map_file_end = read_int(&rom_file);

    uint32_t file_size = map_file_end - map_file_start;

    char* map_file_bytes = new char[file_size];
    rom_file.seekg(map_file_start, ios::beg);
    rom_file.read(map_file_bytes, file_size);

    uint8_t* decompressed_file;
    uint32_t decompressed_size = decompress_LZ10(map_file_bytes, file_size, &decompressed_file);

    LCS* lcs_helper = new LCS;
    lcs_helper->initialize(decompressed_file, decompressed_size);

    for (int i = 0; i < 100; i++) {
        vector<uint8_t> compressed_file;
        uint32_t compressed_size = compress_LZ10(decompressed_file, decompressed_size, &compressed_file, lcs_helper);
    }

    /* just to test */
    // ofstream outfile("recompressed.map.dat", ofstream::binary);
    // outfile.write((const char *) compressed_file.data(), compressed_size);

    // outfile.close();

    delete[] map_file_bytes;
    delete[] decompressed_file;
    delete[] lcs_helper;

    // TODO decompress .map.dat.lz files (LZ10 compression)
    // https://github.com/SunakazeKun/AlmiaE/blob/master/src/com/aurum/almia/game/Compression.java
    // https://ndspy.readthedocs.io/en/latest/_modules/ndspy/lz10.html#decompress

    rom_file.close();
}

int main () {
    map<string,uint16_t> file_locations = get_file_locations ();
    
    ifstream rom_file(rom_file_path);

    rom_file.seekg(0x48, ios::beg);
    uint32_t fat_offset = read_int(&rom_file);

    uint32_t pkmn_name_in_fat =  fat_offset + file_locations["data/message/etc/pokemon_name_us.mes"]*8;
    uint32_t field_move_in_fat = fat_offset + file_locations["data/message/etc/fieldwaza_name_us.mes"]*8;
    uint32_t pokeid_bin_in_fat = fat_offset + file_locations["data/param/PokeID.bin"]*8;

    rom_file.seekg(pkmn_name_in_fat, ios::beg);
    uint32_t pkmn_name_offset = read_int(&rom_file);

    rom_file.seekg(field_move_in_fat, ios::beg);
    uint32_t field_move_offset = read_int(&rom_file);

    rom_file.seekg(pokeid_bin_in_fat, ios::beg);
    uint32_t pokeid_bin_offset = read_int(&rom_file);

    cout << "Pokemon name offset: " << pkmn_name_offset << endl;
    cout << "Field name offset: " << field_move_offset << endl;
    cout << "PokeID bin offset: " << pokeid_bin_offset << endl;

    vector<string> pkmn_names = get_pokemon_names_rom (rom_file_path, 
        pkmn_name_offset);
    vector<string> field_moves = get_field_moves_rom (rom_file_path, 
        field_move_offset);

    // cout << pkmn_names.size() << endl;

    PokeDataStructure pds;

    // TODO: make this a PDS member function
    read_pokeID_rom (&pds, rom_file_path, pokeid_bin_offset);

    // uint16_t poke_id = 411;

    // vector<uint16_t> res = pds.get_pokemon_with_geq_field_move(poke_id);

    // pair<uint8_t, uint8_t> start_field_info = pds.get_field_move(poke_id);
    // cout << "Pokemon that can replace " << pkmn_names[poke_id] << " (" << 
    //     field_moves[start_field_info.first] << " " << dec 
    //     << (uint32_t) start_field_info.second << "):" << endl;

    // for (auto it = res.begin(); it != res.end(); ++it) {
    //     pair<uint8_t, uint8_t> field_info = pds.get_field_move(*it);
    //     cout << pkmn_names[*it] << " (" << field_moves[field_info.first] << " "
    //         << dec << (uint32_t) field_info.second << ")" << endl;
    // }

    // cout << endl;
    // pds.print_level_order();

    // pds.self_test();

    process_map_files(&pds, file_locations, fat_offset);

    rom_file.close();

    // uint8_t thing1[] = "bananayaban";
    // uint8_t thing2[] = "papaya";

    // LCS *lcs_helper = new LCS;

    // lcs_helper->longest_common_substring(&thing1[0], &thing2[0], 11, 6);

    // lcs_helper->self_test_rmq();

    // delete lcs_helper;

    return 0;
}