#ifndef POKEDATASTRUCTURE_H
#define POKEDATASTRUCTURE_H

#include <iostream>
#include <map>
#include <vector>
#include <cassert>
#include <stdint.h>
#include <algorithm>

#include "util.h"

class PokeDataStructure {
    public:
        PokeDataStructure();
        ~PokeDataStructure();
        
        void add_pokemon(uint16_t poke_id, uint8_t field_id, uint8_t field_level);
        std::pair<uint8_t, uint8_t> get_field_move(uint16_t poke_id);

        std::vector<uint16_t> get_pokemon_with_geq_field_move (uint16_t poke_id);

        void print_level_order();
        void self_test();
    private:
        std::map<uint16_t, uint16_t> pokemon_field_moves;

        struct TreeNode {
            // first is (field_id << 8) | field_level, second is poke_id
            //  for leaves, field signature
            //  for non-leaves, max value of left subtree
            uint16_t data;
            std::vector<uint16_t> poke_ids;
            
            int height;
            TreeNode *left;
            TreeNode *right;
        };

        TreeNode *root;

        TreeNode* find_vsplit (TreeNode *node, 
            uint16_t min, uint16_t max);        
        void collect_subtree (TreeNode *node, std::vector<uint16_t> *vec);
        std::vector<uint16_t> range_query (uint16_t min, uint16_t max);

        TreeNode* add_node (TreeNode *nod, uint16_t field_signature, 
            uint16_t poke_id);

        bool is_leaf(TreeNode *node);
        int height(TreeNode* node);
        int get_balance(TreeNode *node);
        TreeNode* left_rotate(TreeNode *x);
        TreeNode* right_rotate(TreeNode *y);

        void print_current_level (TreeNode* node, int level);

        void post_order_delete(TreeNode *node);

};

#endif