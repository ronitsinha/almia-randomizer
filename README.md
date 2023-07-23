# Pokemon Ranger: Shadows of Almia Randomizer
This is a tool for creating a randomizer of the game Pokemon Ranger: Shadows of Almia.

A "randomizer" is a popular romhack for various Pokemon games that involves randomly changing the 
Pokemon encounters throughout the game. 

This randomizer changes encounters in such a way that the
game is guaranteed to still be beatable; that is, it will not replace a Pokemon whose field move
is necessary to beat the level with a Pokemon that has a completely different field move. In
fact, Pokemon's replacement are randomly chosen from the set of Pokemon that have the same field move
as them, with the same field move strength or higher.

## Usage
See "Releases" for Linux and Windows versions.

Run the tool in a terminal, supplying the input ROM file and the name of the
output randomized ROM.
```./range_randommizer [input file] [output file]```
For example:
```./range_randommizer rom.nds randomized.nds```
The program will take a few seconds to run before generating a ROM with the
specified output name.

## References
[AlmiaE](https://github.com/SunakazeKun/AlmiaE) -- an excellent tool for editing Shadows of Almia \
[FEAT](https://github.com/SciresM/FEAT/tree/master) -- for extracting DS rom, LZ10 compression

For my experiments in designing an optimal LZ10 compression algorithm (found in `old/`):
- Kärkkäinen, Juha, and Peter Sanders. "Simple linear work suffix array construction." Automata, Languages and Programming: 30th International Colloquium, ICALP 2003 Eindhoven, The Netherlands, June 30–July 4, 2003 Proceedings 30. Springer Berlin Heidelberg, 2003.
- Babenko, M.A., Starikovskaya, T.A. (2008). Computing Longest Common Substrings Via Suffix Arrays . In: Hirsch, E.A., Razborov, A.A., Semenov, A., Slissenko, A. (eds) Computer Science – Theory and Applications. CSR 2008. Lecture Notes in Computer Science, vol 5010. Springer, Berlin, Heidelberg. https://doi.org/10.1007/978-3-540-79709-8_10
- Kasai, T.; Lee, G.; Arimura, H.; Arikawa, S.; Park, K. (2001). Linear-Time Longest-Common-Prefix Computation in Suffix Arrays and Its Applications. Proceedings of the 12th Annual Symposium on Combinatorial Pattern Matching. Lecture Notes in Computer Science. Vol. 2089. pp. 181–192. doi:10.1007/3-540-48194-X_17