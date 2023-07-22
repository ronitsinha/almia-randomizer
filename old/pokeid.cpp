#include "pokeid.h"

using namespace std;

// TODO: cleanup

// each pokemon entry is 28 bytes long
#define ENTRY_SIZE 0x1C
#define UNIQUE_SIZE 24

char UNIQUE[] = {
    0x07, 0x03, 0x01, 0x07, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01, 0x02,
    0x02, 0x01, 0x01, 0x02, 0x02, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x05
};

vector<string> read_mes_from_rom (const char* path, uint32_t offset) {
    vector<string> messages;

    ifstream mes_file(path);
    mes_file.seekg(offset, ios::beg);

    char total_size_bytes[4];
    char misc_header_bytes[16];

    mes_file.read(&total_size_bytes[0], 4);

    uint32_t num_mesgs = read_int(&mes_file);

    mes_file.read(&misc_header_bytes[0], 16);

    for (uint32_t i = 0; i < num_mesgs; i++) {
        uint32_t msg_length = read_int(&mes_file);

        char *msg = new char[msg_length];
        mes_file.read(&msg[0], msg_length);

        string msg_str(&msg[0], msg_length);
        messages.push_back(msg_str);
        
        delete [] msg;
    }

    mes_file.close();

    return messages;
}

vector<string> read_mes_file (const char* path) {
    vector<string> messages;

    ifstream mes_file;
    mes_file.open(path);

    char total_size_bytes[4];
    char num_mesgs_bytes[4];
    char misc_header_bytes[16];

    mes_file.read(&total_size_bytes[0], 4);
    mes_file.read(&num_mesgs_bytes[0], 4);
    mes_file.read(&misc_header_bytes[0], 16);

    uint32_t num_mesgs = byte_array_to_int(&num_mesgs_bytes[0]);

    for (uint32_t i = 0; i < num_mesgs; i++) {
        char msg_length_bytes[4];
        mes_file.read(&msg_length_bytes[0], 4);

        uint32_t msg_length = byte_array_to_int(&msg_length_bytes[0]);

        char *msg = new char[msg_length];
        mes_file.read(&msg[0], msg_length);

        string msg_str(&msg[0], msg_length);
        messages.push_back(msg_str);
    
        delete [] msg;
    }

    mes_file.close();

    return messages;
}

void read_pokeID_rom (PokeDataStructure *pds, const char* rom_path, uint32_t offset) {
    ifstream bin_file(rom_path);
    bin_file.seekg(offset, ios::beg);

    // size of file
    uint32_t total_size = read_int(&bin_file);

    cout << "File size: " << dec << total_size << " bytes" << endl;

    // other header stuff
    uint32_t unique_size = read_int(&bin_file);
    uint32_t data_size = read_int(&bin_file);

    char unknown_bytes[4];
    bin_file.read(&unknown_bytes[0], 4);

    
    cout << "Unique size: " << hex << unique_size << endl;
    cout << "Data size: " << hex << data_size << endl;

    assert(unique_size == UNIQUE_SIZE);

    char unique[UNIQUE_SIZE];

    bin_file.read(&unique[0], UNIQUE_SIZE);

    for (int i = 0; i < UNIQUE_SIZE; i ++) {
        assert(unique[i] == UNIQUE[i]);
    }

    uint32_t num_entries = data_size / ENTRY_SIZE;

    cout << "Number of entries: " << dec << num_entries << endl;

    for (uint32_t i = 0; i < num_entries; i ++) {
        char entry_data[ENTRY_SIZE];
        bin_file.read(&entry_data[0], ENTRY_SIZE);

        uint16_t name_id = byte_array_to_short(&entry_data[0]);
        uint8_t field_id = (uint8_t) entry_data[5];
        uint8_t field_level = (uint8_t) entry_data[6];

        pds->add_pokemon(i, name_id, field_id, field_level);

        // cout << "Name: " << pkmn_names[name_id] << " (ID: " << name_id << ", " << field_moves[field_id] << " " << dec << field_level << ")" << endl;
        // cout << "Name: " << pkmn_names[name_id] << " (ID: " << i << " " << dec << field_level << ")" << endl;
    }

    bin_file.close();
}

void read_pokeID_bin (vector<string> pkmn_names, vector<string> field_moves) {
    ifstream bin_file;
    bin_file.open("PokeID.bin");

    // size of file
    char total_size_bytes[4];
    bin_file.read(&total_size_bytes[0], 4);

    uint32_t total_size = byte_array_to_int(&total_size_bytes[0]);

    cout << "File size: " << dec << total_size << " bytes" << endl;

    // other header stuff
    char unique_size_bytes[4];
    char data_size_bytes[4];
    char unknown_bytes[4];

    bin_file.read(&unique_size_bytes[0], 4);
    bin_file.read(&data_size_bytes[0], 4);
    bin_file.read(&unknown_bytes[0], 4);

    uint32_t unique_size = byte_array_to_int(&unique_size_bytes[0]);
    uint32_t data_size = byte_array_to_int(&data_size_bytes[0]);

    cout << "Unique size: " << hex << unique_size << endl;
    cout << "Data size: " << hex << data_size << endl;

    assert(unique_size == UNIQUE_SIZE);

    char unique[UNIQUE_SIZE];

    bin_file.read(&unique[0], UNIQUE_SIZE);


    for (int i = 0; i < UNIQUE_SIZE; i ++) {
        assert(unique[i] == UNIQUE[i]);
    }

    uint32_t num_entries = data_size / ENTRY_SIZE;

    cout << "Number of entries: " << dec << num_entries << endl;

    for (uint32_t i = 0; i < num_entries; i ++) {
        char entry_data[ENTRY_SIZE];
        bin_file.read(&entry_data[0], ENTRY_SIZE);

        uint16_t name_id = byte_array_to_short(&entry_data[0]);
        uint32_t field_id = (uint32_t) (unsigned char) entry_data[5];
        uint32_t field_level = (uint32_t) (unsigned char) entry_data[6];


        cout << "Name: " << pkmn_names[name_id] << " (ID: " << name_id << ", " << field_moves[field_id] << " " << dec << field_level << ")" << endl;
    }

    bin_file.close();
}

vector<string> get_pokemon_names_rom (const char *path, uint32_t offset) {
    return read_mes_from_rom(path, offset);
}

vector<string> get_field_moves_rom (const char *path, uint32_t offset) {
    return read_mes_from_rom(path, offset);
}