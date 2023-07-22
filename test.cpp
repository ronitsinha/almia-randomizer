#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <ctime>
#include <cstdlib>

#include "util.h"
#include "lcs.h"
#include "pokeid.h"
#include "pokedatastructure.h"

using namespace std;

const char* rom_file_path = "rom.nds";
const char* out_file_path = "randomized.nds";

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

uint32_t compress_LZ10 (uint8_t *uncompressed_data, uint32_t uncompressed_size, vector<uint8_t> *compressed_data) {
    assert(uncompressed_size <= 0xFFFFFF);

    vector<uint8_t> buffer;

    buffer.push_back(0x10);
    // load uncompressed size into buffer
    buffer.push_back((uint8_t) (uncompressed_size & 0xFF));
    buffer.push_back((uint8_t) ((uncompressed_size >> 8) & 0xFF));
    buffer.push_back((uint8_t) ((uncompressed_size >> 16) & 0xFF));

    int max_disp = 0xFFF;
    int max_length = 0xF + 3;

    uint32_t cur_position = 0;

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
                int length = 0, k = 0;
                while (cur_position + k < uncompressed_size && uncompressed_data[j+k] == uncompressed_data[cur_position+k] && length < max_length) {
                    length ++; k++;
                }

                if (length > best_run_length) {
                    best_run_length = get_min(length, max_length);
                    best_run_start = j;
                }
            }

            if (best_run_length < 3) // store as raw byte 
                buffer.push_back(uncompressed_data[cur_position++]);
            else { // store as dictionary entry
                uint16_t disp = cur_position - best_run_start -1;
                
                uint8_t length = best_run_length -3;
                uint8_t token1 = ((disp >> 8) & 0x0F) | ((length << 4) & 0xF0);
                uint8_t token2 = disp & 0xFF; 

                buffer.push_back(token1); buffer.push_back(token2);

                flag_byte |= 1 << (8-i); // set corresponding flag bit
                cur_position += best_run_length;
            }

        }

        buffer[flag_index] = flag_byte;
    }

    uint32_t compressed_size = buffer.size();

    *compressed_data = buffer;

    return compressed_size;
}

#define NARC_MAGIC 0x4E415243
#define NARC_FATB  0x46415442
#define NARC_FNTB  0x464E5442
#define NARC_FIMG  0x46494D47
#define LYR_MAGIC  0x4C595200

// http://llref.emutalk.net/docs/?file=xml/narc.xml#xml-doc
// http://www.pipian.com/ierukana/hacking/ds_narc.html
// https://www.romhacking.net/documents/%5B469%5Dnds_formats.htm#NARC
// returns vector of start/end offsets for files
vector<pair<uint32_t,uint32_t>> decode_NARC (uint8_t *data) {
    uint32_t magic = byte_array_to_int((char*) data, true); // big endian due to byte order of magic (bom)
    assert(magic == NARC_MAGIC); //  magic is "NARC"

    uint32_t file_size =  byte_array_to_int((char*) data +0x8);

    uint32_t fatb_offset = 0x10;
    uint32_t fatb_stamp = byte_array_to_int((char*) data + fatb_offset); 
    assert(fatb_stamp == NARC_FATB);

    uint32_t num_files_offset = fatb_offset + 0x8;
    uint32_t num_files = byte_array_to_int((char*) data + num_files_offset);

    vector<pair<uint32_t,uint32_t>> file_positions;

    for (int i = 0; i < num_files; i++) {
        uint32_t start_offset = byte_array_to_int((char *) data + num_files_offset + 0x4 + i*8);
        uint32_t end_offset = byte_array_to_int((char *) data + num_files_offset + 0x4 + (i*8) + 0x4);
        file_positions.push_back(pair<uint32_t, uint32_t>(start_offset, end_offset));
    }

    uint32_t fntb_offset = num_files_offset + 0x4 + num_files * 8;
    uint32_t fntb_stamp = byte_array_to_int((char*) data + fntb_offset);
    assert(fntb_stamp == NARC_FNTB);

    uint32_t fntb_size = byte_array_to_int((char *) data + fntb_offset + 0x4);

    uint32_t fimg_offset = fntb_offset + fntb_size;

    uint32_t fimg_stamp = byte_array_to_int((char *) data + fimg_offset);

    assert(fimg_stamp == NARC_FIMG);

    // update vector so it contains absolute offsets (not just relative to start of FIMG)
    for (auto it = file_positions.begin(); it != file_positions.end(); ++it) {
        it->first = fimg_offset + 0x8 + it->first; // +8 b/c relative offsets start after FIMG header
        it->second = fimg_offset + 0x8 + it->second;
    }

    return file_positions;
}

void process_map_files (PokeDataStructure* pds, map<string, uint16_t> file_locations, uint32_t fat_offset) {
    map<uint32_t, string> inv_start_offsets; // sorted by key in increasing order by default

    ifstream rom_file(rom_file_path);
    fstream out_file(out_file_path);

    rom_file.seekg(0x20, ios::beg);
    uint32_t arm9_offset = read_int(&rom_file);
    uint32_t arm9_size = read_int(&rom_file);

    for (auto it = file_locations.begin(); it != file_locations.end(); ++it) {
        uint32_t file_in_fat = fat_offset + it->second*8;
        rom_file.seekg(file_in_fat, ios::beg);
        uint32_t file_start = read_int(&rom_file);
        inv_start_offsets[file_start] = it->first;
    }

    uint32_t last_end_offset = 0;

    for (auto it = inv_start_offsets.begin(); it != inv_start_offsets.end(); ++it) {
        uint32_t file_fat_offset = fat_offset + file_locations[it->second]*8;
        rom_file.seekg(file_fat_offset, ios::beg);
        uint32_t file_start = read_int(&rom_file);
        uint32_t file_end   = read_int(&rom_file);
        uint32_t file_size = file_end - file_start;

        uint32_t new_file_start = file_start;
        uint32_t new_file_end = file_end;

        if (file_start < last_end_offset) {
            new_file_start = round_to_multiple(last_end_offset, 512);
            
            // pad with FF between last_end offset and new start 
            out_file.seekp(last_end_offset, ios::beg);
            for (uint32_t i = last_end_offset; i < new_file_start; i++) 
                out_file.put(0xFF);

        }

        if (str_ends_with(it->second, ".map.dat.lz") and !str_ends_with(it->second, "m001_013.map.dat.lz")) {
            char file_bytes[file_size];
            rom_file.seekg(file_start, ios::beg);
            rom_file.read(file_bytes, file_size);

            // decompress map file
            uint8_t* decompressed_file;
            uint32_t decompressed_size = decompress_LZ10(file_bytes, file_size, &decompressed_file);

            // unpack NARC
            vector<pair<uint32_t,uint32_t>> narc_offsets = decode_NARC(decompressed_file);
            uint32_t lyr_start, lyr_end;
            for (auto it = narc_offsets.begin(); it != narc_offsets.end(); ++it) {
                uint32_t stamp = byte_array_to_int((char*) decompressed_file + it->first, true);
                if (stamp == LYR_MAGIC) {
                    lyr_start = it->first; lyr_end = it->second;
                }
            }

            // decode NARC of LYR file
            vector<pair<uint32_t,uint32_t>> lyr_narc_offsets = decode_NARC(decompressed_file + lyr_start + 0x4);
            
            for (auto jt = lyr_narc_offsets.begin(); jt != lyr_narc_offsets.end(); ++jt) {
                uint32_t narc_container_start = jt->first;
                vector<pair<uint32_t,uint32_t>> layers = decode_NARC(decompressed_file + lyr_start + 0x4 + narc_container_start);

                // find LYR layer that contains pokemon data and randomize it 
                for (auto it = layers.begin(); it != layers.end(); ++it) {
                    uint32_t start = lyr_start + 0x4 + narc_container_start + it->first;
                    uint32_t identifier = byte_array_to_int((char*) decompressed_file + start);
                    if (identifier == 0x9) { // Pokemon data
                        uint32_t num_entries = byte_array_to_int((char*) decompressed_file + start + 0x4);
                        for (int i = 0; i < num_entries; i++) {
                            
                            uint32_t pokeid_offset = 0x6 + (i*0xA);
                            uint16_t poke_id = byte_array_to_short((char*) decompressed_file + start + 0x8 + pokeid_offset);
                            
                            vector<uint16_t> replacement_mons = pds->get_pokemon_with_geq_field_move(poke_id);
                            uint16_t new_pokemon = replacement_mons[rand() % replacement_mons.size()];

                            decompressed_file[start+0x8+pokeid_offset] = new_pokemon & 0xFF;
                            decompressed_file[start+0x8+pokeid_offset+1] = (new_pokemon >> 8) & 0xFF;
                        }
                    }
                }
            }

            // compress file
            vector<uint8_t> compressed_file; 
            uint32_t compressed_size = compress_LZ10(decompressed_file, decompressed_size, &compressed_file);

            new_file_end = new_file_start + compressed_size;

            if (round_to_multiple(new_file_end, 512) > round_to_multiple(file_end,512)) continue;

            out_file.seekp(file_fat_offset, ios::beg);
            write_int((ofstream*) &out_file, new_file_start); // write new start in FAT

            out_file.seekp(file_fat_offset + 4, ios::beg);
            write_int((ofstream*) &out_file, new_file_end); // write new end in FAT

            out_file.seekp(new_file_start, ios::beg);
            out_file.write((char*) compressed_file.data(), compressed_size);

            for (uint32_t i = compressed_size; i < file_size; i++)
                out_file.put(0xFF);

            delete[] decompressed_file;

        } else if (new_file_start != file_start) {
            // need to copy over file starting at new offset
            char file_bytes[file_size];
            rom_file.seekg(file_start, ios::beg);
            rom_file.read(file_bytes, file_size);

            out_file.seekp(new_file_start, ios::beg);
            out_file.write(file_bytes, file_size);

            new_file_end = new_file_start + file_size;

            out_file.seekp(file_fat_offset, ios::beg);
            write_int((ofstream*) &out_file, new_file_start); // write new start in FAT

            out_file.seekp(file_fat_offset + 4, ios::beg);
            write_int((ofstream*) &out_file, new_file_end); // write new end in FAT
        }

        last_end_offset = new_file_end;
    }

    cout << "new rom size: " << last_end_offset << endl;

    // update total size in header
    out_file.seekp(0x80, ios::beg);
    write_int((ofstream*) &out_file, last_end_offset);

    // recompute header checksum
    uint8_t header_data[0x15E];
    out_file.seekg(0, ios::beg);
    out_file.read((char *) header_data, 0x15E);

    // https://github.com/Luca1991/NDSFactory/blob/master/ndsfactory/ndsfactory.cpp#L156
    uint16_t header_checksum = compute_crc16(header_data, 0x15E);
    uint8_t checksum_LE[] = {(uint8_t) (header_checksum & 0xFF), 
        (uint8_t) ((header_checksum >> 8) & 0xFF)};

    out_file.seekp(0x15E, ios::beg);
    out_file.write((char *) checksum_LE, 2);

    cout << "new header checksum: " << hex << header_checksum << endl;

    // TODO decompress .map.dat.lz files (LZ10 compression)
    // https://github.com/SunakazeKun/AlmiaE/blob/master/src/com/aurum/almia/game/Compression.java
    // https://ndspy.readthedocs.io/en/latest/_modules/ndspy/lz10.html#decompress

    // m001_004
    // uint32_t file_fat_offset = fat_offset + file_locations["data/field/map/m001_004.map.dat.lz"]*8;
    // rom_file.seekg(file_fat_offset, ios::beg);
    // uint32_t file_start = read_int(&rom_file);
    // uint32_t file_end   = read_int(&rom_file);
    // uint32_t file_size = file_end - file_start;

    // char file_bytes[file_size];
    // rom_file.seekg(file_start, ios::beg);
    // rom_file.read(file_bytes, file_size);

    // uint8_t* decompressed_file;
    // uint32_t decompressed_size = decompress_LZ10(file_bytes, file_size, &decompressed_file);    

    // vector<uint8_t> compressed_file; 
    // uint32_t compressed_size = compress_LZ10(decompressed_file, decompressed_size, &compressed_file);

    // ofstream decomp_file("recompressed_001_004.map.dat");
    // decomp_file.write((char *) compressed_file.data(), compressed_size);

    // delete[] decompressed_file;

    rom_file.close(); out_file.close();
}

int main () {
    srand(time(NULL));

    map<string,uint16_t> file_locations = get_file_locations ();
    
    ifstream rom_file(rom_file_path);

    ofstream out_file(out_file_path); // copy over file
    out_file << rom_file.rdbuf(); out_file.close();

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

    rom_file.seekg(0x80, ios::beg);
    uint32_t total_size = read_int(&rom_file);
    cout << "total size: " << dec << total_size << endl;

    process_map_files(&pds, file_locations, fat_offset);

    rom_file.close();

    return 0;
}