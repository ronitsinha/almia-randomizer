#include "romprocessor.h"

using namespace std;

RomProcessor::RomProcessor(const char *in_path, const char *out_path) {
    rom_file.open(in_path);
    out_file.open(out_path, fstream::in | fstream::out | fstream::trunc);

    out_file << rom_file.rdbuf(); out_file.flush(); // copy over file

    rom_file.seekg(0x48, ios::beg);
    fat_offset = read_int(&rom_file);

    get_file_locations();

    uint32_t pokeid_bin_in_fat = fat_offset + file_locations["data/param/PokeID.bin"]*8;
    rom_file.seekg(pokeid_bin_in_fat, ios::beg);
    uint32_t pokeid_bin_offset = read_int(&rom_file);

    read_pokeID_rom(pokeid_bin_offset);
}

RomProcessor::~RomProcessor() {
    rom_file.close(); out_file.close();
}

void RomProcessor::randomize_rom() {
    process_map_files();
}

void RomProcessor::process_map_files () {
    map<uint32_t, string> inv_start_offsets; // sorted by key in increasing order by default

    rom_file.seekg(0x20, ios::beg);

    for (auto it = file_locations.begin(); it != file_locations.end(); ++it) {
        uint32_t file_in_fat = fat_offset + it->second*8;
        rom_file.seekg(file_in_fat, ios::beg);
        uint32_t file_start = read_int(&rom_file);
        inv_start_offsets[file_start] = it->first;
    }

    uint32_t last_end_offset = 0;

    rom_file.seekg(0x80, ios::beg);
    uint32_t total_size = read_int(&rom_file);
    cout << "total size: " << dec << total_size << endl;

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
                out_file.put((uint8_t) 0xFF);

        }

        if (str_ends_with(it->second, ".map.dat.lz") and !str_ends_with(it->second, "m001_013.map.dat.lz")) {
            char *file_bytes = new char[file_size];
            rom_file.seekg(file_start, ios::beg);
            rom_file.read(file_bytes, file_size);

            // decompress map file
            uint8_t* decompressed_file;
            uint32_t decompressed_size = decompress_LZ10(file_bytes, file_size, &decompressed_file);

            delete[] file_bytes;

            // unpack NARC
            vector<pair<uint32_t,uint32_t>> narc_offsets = decode_NARC(decompressed_file);
            uint32_t lyr_start;
            for (auto jt = narc_offsets.begin(); jt != narc_offsets.end(); ++jt) {
                uint32_t stamp = byte_array_to_int((char*) decompressed_file + jt->first, true);
                if (stamp == LYR_MAGIC) {
                    lyr_start = jt->first;
                }
            }

            // decode NARC of LYR file
            vector<pair<uint32_t,uint32_t>> lyr_narc_offsets = decode_NARC(decompressed_file + lyr_start + 0x4);
            
            for (auto jt = lyr_narc_offsets.begin(); jt != lyr_narc_offsets.end(); ++jt) {
                uint32_t narc_container_start = jt->first;
                vector<pair<uint32_t,uint32_t>> layers = decode_NARC(decompressed_file + lyr_start + 0x4 + narc_container_start);

                // find LYR layer that contains pokemon data and randomize it 
                for (auto kt = layers.begin(); kt != layers.end(); ++kt) {
                    uint32_t start = lyr_start + 0x4 + narc_container_start + kt->first;
                    uint32_t identifier = byte_array_to_int((char*) decompressed_file + start);
                    if (identifier == 0x9) { // Pokemon data
                        uint32_t num_entries = byte_array_to_int((char*) decompressed_file + start + 0x4);
                        for (uint32_t i = 0; i < num_entries; i++) {
                            
                            uint32_t pokeid_offset = 0x6 + (i*0xA);
                            uint16_t poke_id = byte_array_to_short((char*) decompressed_file + start + 0x8 + pokeid_offset);
                            
                            vector<uint16_t> replacement_mons = pds.get_pokemon_with_geq_field_move(poke_id);
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

            delete[] decompressed_file;

            new_file_end = new_file_start + compressed_size;

            out_file.seekp(file_fat_offset, ios::beg);
            write_int((ofstream*) &out_file, new_file_start); // write new start in FAT

            out_file.seekp(file_fat_offset + 4, ios::beg);
            write_int((ofstream*) &out_file, new_file_end); // write new end in FAT

            out_file.seekp(new_file_start, ios::beg);
            out_file.write((char*) compressed_file.data(), compressed_size);

            for (uint32_t i = compressed_size; i < file_size; i++)
                out_file.put((uint8_t) 0xFF);

        } else if (new_file_start != file_start) {
            // need to copy over file starting at new offset
            char *file_bytes = new char[file_size];
            rom_file.seekg(file_start, ios::beg);
            rom_file.read(file_bytes, file_size);

            out_file.seekp(new_file_start, ios::beg);
            out_file.write(file_bytes, file_size);

            delete[] file_bytes;

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
}

void RomProcessor::get_file_locations () {
    // https://web.archive.org/web/20110718184246/http://nocash.emubase.de/gbatek.htm#dsmemorymaps
    // mapping of filenames (strings) to FAT offsets
    rom_file.seekg(0x40, ios::beg);
    uint32_t fnt_offset = read_int(&rom_file);

    cout << "FNT offset: " << hex << fnt_offset << endl;
    cout << "FAT offset: " << hex << fat_offset << endl;

    // Go to start of fnt
    rom_file.seekg(fnt_offset, ios::beg);

    uint32_t subtable_offset = read_int(&rom_file);
    uint16_t first_file_id = read_short(&rom_file);

    cout << "First subtable offset: " << hex << subtable_offset << endl;
    cout << "First file id: " << hex << first_file_id << endl;

    explore_subtable(subtable_offset, fnt_offset, "", &first_file_id);
}

void RomProcessor::explore_subtable (uint32_t subtable_offset,
    uint32_t fnt_offset, string path, uint16_t *file_id) {

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
            explore_subtable (new_subtable, fnt_offset, new_path,
                &first_file_id);

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

            file_locations.insert(pair<string,uint16_t>(full_path_str, *file_id));
            *file_id += 1;

            delete[] filename_bytes;
        }
        
        rom_file.read(&length_type_byte, 1);
        length_type = (uint32_t) (unsigned char)length_type_byte;
    }
}

void RomProcessor::read_pokeID_rom (uint32_t offset) {
    rom_file.seekg(offset, ios::beg);

    // size of file
    uint32_t total_size = read_int(&rom_file);

    cout << "File size: " << dec << total_size << " bytes" << endl;

    // other header stuff
    uint32_t unique_size = read_int(&rom_file);
    uint32_t data_size = read_int(&rom_file);

    char unknown_bytes[4];
    rom_file.read(&unknown_bytes[0], 4);

    
    cout << "Unique size: " << hex << unique_size << endl;
    cout << "Data size: " << hex << data_size << endl;

    assert(unique_size == UNIQUE_SIZE);

    char unique[UNIQUE_SIZE];

    rom_file.read(&unique[0], UNIQUE_SIZE);

    for (int i = 0; i < UNIQUE_SIZE; i ++) {
        assert(unique[i] == UNIQUE[i]);
    }

    uint32_t num_entries = data_size / ENTRY_SIZE;

    cout << "Number of entries: " << dec << num_entries << endl;

    for (uint32_t i = 0; i < num_entries; i ++) {
        char entry_data[ENTRY_SIZE];
        rom_file.read(&entry_data[0], ENTRY_SIZE);

        uint16_t name_id = byte_array_to_short(&entry_data[0]);
        uint8_t field_id = (uint8_t) entry_data[5];
        uint8_t field_level = (uint8_t) entry_data[6];

        pds.add_pokemon(i, name_id, field_id, field_level);
    }
}