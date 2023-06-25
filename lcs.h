#ifndef LCS_H
#define LCS_H

#include <iostream>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <math.h>       /* log2, ceil */
/* randomness for self-test */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

#include "util.h"

class LCS {
    public:
        LCS();
        ~LCS();

        int length, start_index; /* store values after computing LCS */

        void initialize(uint8_t *s, int l);
        int get_lcp(int i, int j);

        int longest_common_substring (uint8_t *s1, uint8_t *s2, int l1, int l2);
        void self_test_rmq(bool debug_print = false);
    
    private:
        int *T, *sa, *lcp, *rank;
        int *G, **M, *L, *A_prime, *left_min, *right_min, *lsb_map;
        int n, block_size, A_len, log_A_len;


        void suffix_array(int *s, int *SA, int n, int K);
        void lcp_array(int *s, int n);
        void st_preprocess();
        int rmq(int i, int j);

        void do_free();

};

#endif