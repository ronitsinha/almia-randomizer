#include "pokedatastructure.h"

#define RED 1
#define BLACK 0

using namespace std;

PokeDataStructure::PokeDataStructure()
{
    root = nullptr;
}


PokeDataStructure::~PokeDataStructure()
{
        // walk tree in post-order traversal and delete
        post_order_delete(root);
        root = nullptr;  // not really necessary, since the tree is going
                         // away, but might want to guard against someone
                         // using a pointer after deleting
}

void PokeDataStructure::post_order_delete(TreeNode* node)
{
        if (node == nullptr) return; // Empty tree

        post_order_delete(node->left);
        post_order_delete(node->right);

        delete node;   
}

void PokeDataStructure::print_current_level (TreeNode* node, int level)
{
    if (node == nullptr) return;
    
    if (level == 1) {
        if (is_leaf(node))
            cout << "[" << node->data << ":" << node->poke_ids.size() << "] ";
        else {
            int num_children = (node->left == nullptr or node->right == nullptr) ? 1 : 2;
            cout << "(" << node->data << " : " << num_children << ") ";
        }
    } else if (level > 1) {
        print_current_level(node->left, level-1);
        print_current_level(node->right, level-1);
    }
}

void PokeDataStructure::print_level_order()
{
    int h = height(root);
    for (int i = 1; i <= h; i++) {
        cout << "LEVEL: " << i << endl;
        print_current_level(root, i);
        cout << endl;
    }
}

bool PokeDataStructure::is_leaf (TreeNode *node) {
    if (node == nullptr) return false;

    return (node->left == nullptr and node->right == nullptr);
}

int PokeDataStructure::height(TreeNode* node)
{
    if (node == nullptr) return 0;

    return node->height;
}

int PokeDataStructure::get_balance(TreeNode* node) {
    if (node == nullptr) return 0;

    return height(node->left) - height(node->right);
}

// https://www.geeksforgeeks.org/insertion-in-an-avl-tree/#
PokeDataStructure::TreeNode* PokeDataStructure::left_rotate(TreeNode *x) {
    TreeNode *y = x->right;
    TreeNode *T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = get_max(height(x->left), height(x->right)) + 1;
    y->height = get_max(height(y->left), height(y->right)) + 1;

    return y;
}

PokeDataStructure::TreeNode* PokeDataStructure::right_rotate(TreeNode *y) {
    TreeNode *x = y->left;
    TreeNode *T2 = x->right;

    x->right = y;
    y->left = T2;

    x->height = get_max(height(x->left), height(x->right)) + 1;
    y->height = get_max(height(y->left), height(y->right)) + 1;

    return x;
}

PokeDataStructure::TreeNode* PokeDataStructure::add_node(TreeNode *node, 
    uint16_t field_signature, uint16_t poke_id) 
{
    if (node == nullptr) {
        // data always stored in the leaves
        TreeNode *new_node = new TreeNode;
        new_node->left = new_node->right = nullptr;
        new_node->height = 1;
        new_node->data = field_signature;
        new_node->poke_ids.push_back(poke_id);

        return new_node;
    }

    if (is_leaf(node)) {
        // make a new node with two leaves
        if (node->data == field_signature) {
            node->poke_ids.push_back(poke_id);
            return node;
        }

        TreeNode *new_leaf = new TreeNode;
        new_leaf->left = new_leaf->right = nullptr;
        new_leaf->height = 1;
        new_leaf->data = field_signature;
        new_leaf->poke_ids.push_back(poke_id);

        TreeNode *new_root = new TreeNode;
        new_root->left = new_root->right = nullptr;
        new_root->height = 2;

        if (field_signature < node->data) {
            new_root->left = new_leaf;
            new_root->right = node;
            new_root->data = field_signature;
        } else {
            new_root->left = node;
            new_root->right = new_leaf;
            new_root->data = node->data;
        }

        return new_root;
    }

    if (field_signature > node->data)
        node->right = add_node(node->right, field_signature, poke_id);
    else
        node->left = add_node(node->left, field_signature, poke_id);

    node->height = 1 + get_max(height(node->left), height(node->right));

    int balance = get_balance(node); 

    // 4 cases for rebalancing AVL tree
    while (balance > 1 or balance < -1) {

        // Left-Left
        if (balance > 1 and field_signature < node->left->data)
            node = right_rotate(node);

        // Right-Right
        else if (balance < -1 and field_signature > node->right->data)
            node = left_rotate(node);

        // Left-Right
        else if (balance > 1 and field_signature > node->left->data) {
            node->left = left_rotate(node->left);
            node = right_rotate(node);
        }

        // Right-Left
        else if (balance < -1 and field_signature < node->right->data) {
            node->right = right_rotate(node->right);
            node = left_rotate(node);
        }


        balance = get_balance(node);
        field_signature = node->data; 
    }

    // no unbalance; return unchanged node
    return node;

}

void PokeDataStructure::add_pokemon(uint16_t poke_id, uint16_t name_id, uint8_t field_id, 
    uint8_t field_level)
{
    pokemon_reals_to_names[poke_id] = name_id;
 
    if (pokemon_field_moves.find(name_id) != pokemon_field_moves.end()) 
        return; // already seen this pokemon

    uint16_t field_signature = ((uint16_t) field_id << 8) | field_level;

    // assert(field_id == (field_signature >> 8));
    // assert(field_level == (uint8_t) field_signature);

    pokemon_field_moves[name_id] = field_signature;
    pokemon_names_to_reals[name_id] = poke_id;
    root = add_node(root, field_signature, name_id);
}

pair<uint8_t, uint8_t> PokeDataStructure::get_field_move(uint16_t poke_id) {
    uint16_t field_signature = pokemon_field_moves[pokemon_reals_to_names[poke_id]];

    uint8_t field_move = (uint8_t)(field_signature >> 8);
    // https://stackoverflow.com/questions/27889213/c-integer-downcast
    // downcast truncates most significant bytes
    uint8_t field_level = (uint8_t) field_signature;

    return pair<uint8_t, uint8_t>(field_move, field_level);
}

vector<uint16_t> PokeDataStructure::get_pokemon_with_geq_field_move(
    uint16_t poke_id) {
    assert (pokemon_reals_to_names.find(poke_id) != pokemon_reals_to_names.end());
    uint16_t field_signature = pokemon_field_moves[pokemon_reals_to_names[poke_id]];
    uint16_t field_sig_max = field_signature | 0xFF;
    
    // the reason we store name ids as opposed to the actual pokemon ids
    //  is because name ids are unique and we want to avoid duplicates
    //  unfortunately this means we need maps to go from name ids to real ids
    //  and back.
    vector<uint16_t> query = range_query(field_signature, field_sig_max);
    vector<uint16_t> answer;

    // get real ids from name ids
    for (auto it = query.begin(); it != query.end(); ++it)
        answer.push_back(pokemon_names_to_reals[*it]);

    return answer;
}

void PokeDataStructure::collect_subtree(TreeNode *node, vector<uint16_t> *vec)
{
    if (node == nullptr) return;

    if (is_leaf(node)) {
        vec->insert(vec->end(), node->poke_ids.begin(), node->poke_ids.end());
        return;
    }

    collect_subtree(node->left, vec);
    collect_subtree(node->right, vec);
}


PokeDataStructure::TreeNode* PokeDataStructure::find_vsplit (TreeNode *node, 
    uint16_t min, uint16_t max)
{
    if (node == nullptr) return nullptr;

    if (node->data > min and node->data > max) 
        return find_vsplit(node->left, min, max);

    if (node->data < min and node->data < max) 
        return find_vsplit(node->right, min, max);

    return node;
}

// TODO: report in order (not really necessary...)
vector<uint16_t> PokeDataStructure::range_query(uint16_t min, uint16_t max) {
    vector<uint16_t> results;

    TreeNode* v_split = find_vsplit(root, min, max);

    if (v_split == nullptr) return results;

    if (is_leaf(v_split)) {
        results.insert(results.end(), v_split->poke_ids.begin(), 
            v_split->poke_ids.end());
        return results;
    }

    // get all right subtrees on path to min
    TreeNode *min_path = v_split->left;
    while (min_path != nullptr) {
        if (is_leaf(min_path)) {// leaf
            if (min_path->data >= min and min_path->data <= max) {
                results.insert(results.end(), min_path->poke_ids.begin(), 
                    min_path->poke_ids.end());
                
                break;
            }
        }

        if (min_path->data >= min) {
            collect_subtree(min_path->right, &results);
            min_path = min_path->left;
        } else
            min_path = min_path->right;
    }

    // get all left subtrees on path to max
    TreeNode *max_path = v_split->right;
    while (max_path != nullptr) {
        if (is_leaf(max_path)) {// leaf
            if (max_path->data >= min and max_path->data <= max) {
                results.insert(results.end(), max_path->poke_ids.begin(), 
                    max_path->poke_ids.end());

                break;
            }
        }

        if (max_path->data < max) {
            collect_subtree(max_path->left, &results);
            max_path = max_path->right;
        } else
            max_path = max_path->left;
    }

    return results;
}

void PokeDataStructure::self_test () {
    for (auto it = pokemon_field_moves.begin(); it != pokemon_field_moves.end(); ++it) {
        vector<uint16_t> manually_checked;
        
        uint8_t field_move  = (uint8_t) (it->second >> 8);
        uint8_t field_level = (uint8_t) it->second;

        for (auto it2 = pokemon_field_moves.begin(); it2 != pokemon_field_moves.end(); ++it2) {
            uint8_t field_move2  = (uint8_t) (it2->second >> 8);
            uint8_t field_level2 = (uint8_t) it2->second;

            if (field_move2 == field_move and field_level2 >= field_level)
                manually_checked.push_back(it2->first);
        }

        vector<uint16_t> range_query = get_pokemon_with_geq_field_move(it->first);  
        
        sort(manually_checked.begin(), manually_checked.end());
        sort(range_query.begin(), range_query.end());

        assert(range_query.size() == manually_checked.size());

        for (unsigned int i = 0; i < range_query.size(); i ++) {
            assert(range_query[i] == manually_checked[i]);
        }

    }

    cout << "self-test passed." << endl;
}
