#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <ctime>
#include <cstdlib>

#include "romprocessor.h"

using namespace std;

// const char* rom_file_path = "rom.nds";
// const char* out_file_path = "randomized.nds";

int main (int argc, char *argv[]) {
    if (argc < 3) {
        cout << 
            "Usage: ./ranger_randomizer [input file name] [output file name]" 
            << endl;
    
        return 0;
    }

    srand(time(NULL));

    RomProcessor rp(argv[1], argv[2]);

    rp.randomize_rom();

    return 0;
}