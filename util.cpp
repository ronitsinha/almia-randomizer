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
    // uint16_t val[8] = {0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001};
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
            bool flag_bit = (flag_byte >> (8-i)) & 1;
            if (flag_bit) {
                // Dictionary entry
                uint8_t token1 = (uint8_t) compressed_bytes[input_pos++];
                uint8_t token2 = (uint8_t) compressed_bytes[input_pos++];
                
                int disp = ((token1 & 0x0F) << 8) | token2; disp += 1;
                uint8_t length = (token1 >> 4) + 3; // plus 3 for some reason?

                int read_start = output_pos - disp;

                assert ((uint32_t) disp <= output_pos);

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
            int best_run_length = 0;
            int best_run_start = start_idx;

            for (uint32_t j = start_idx; j < cur_position and best_run_length < max_length; j++) {
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

// http://llref.emutalk.net/docs/?file=xml/narc.xml#xml-doc
// http://www.pipian.com/ierukana/hacking/ds_narc.html
// https://www.romhacking.net/documents/%5B469%5Dnds_formats.htm#NARC
// returns vector of start/end offsets for files
vector<pair<uint32_t,uint32_t>> decode_NARC (uint8_t *data) {
    uint32_t magic = byte_array_to_int((char*) data, true); // big endian due to byte order of magic (bom)
    assert(magic == NARC_MAGIC); //  magic is "NARC"

    uint32_t fatb_offset = 0x10;
    uint32_t fatb_stamp = byte_array_to_int((char*) data + fatb_offset); 
    assert(fatb_stamp == NARC_FATB);

    uint32_t num_files_offset = fatb_offset + 0x8;
    uint32_t num_files = byte_array_to_int((char*) data + num_files_offset);

    vector<pair<uint32_t,uint32_t>> file_positions;

    for (uint32_t i = 0; i < num_files; i++) {
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