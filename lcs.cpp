#include "lcs.h"

using namespace std;
/*                           */
/* Longest Common Substring  */
/*                           */
// Sources:

// https://web.archive.org/web/20120616234651/http://www.cs.sysu.edu.cn/nong/index.files/Two%20Efficient%20Algorithms%20for%20Linear%20Suffix%20Array%20Construction.pdf
// https://link.springer.com/chapter/10.1007/978-3-642-22300-6_32
// https://zork.net/~st/jottings/sais.html
// https://www.youtube.com/watch?v=Ic80xQFWevc
// https://www.youtube.com/watch?v=OIuG_Dqyl_s
// https://link.springer.com/chapter/10.1007/978-3-540-79709-8_10
// http://www.cs.cmu.edu/~guyb/paralg/papers/KarkkainenSanders03.pdf
// https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=2151c8d46282010599d125f01015dc2fc3d2fe4d
// https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=7e70b4ca7d8f7598dccdf0b9f671b30f628fea9f

LCS::LCS() {
    T = sa = lcp = rank = nullptr;
    G = L = A_prime = left_min = right_min = lsb_map = nullptr;
    M = nullptr;
    n = block_size = A_len = log_A_len = 0;
    length = start_index = 0;
}

LCS::~LCS() {
    do_free();
}

void LCS::do_free() {
    if (T != nullptr) {delete [] T; T = nullptr; }
    if (sa != nullptr) {delete [] sa; sa = nullptr; }
    if (lcp != nullptr) {delete [] lcp; lcp = nullptr; }
    if (rank != nullptr) {delete [] rank; rank = nullptr; }
    if (G != nullptr) {delete [] G; G = nullptr; }
    if (M != nullptr) {
        for (int i = 0; i < A_len; i++) delete [] M[i];
        delete [] M; 
        M = nullptr; 
    }
    if (L != nullptr) {delete [] L; L = nullptr; }
    if (A_prime != nullptr) {delete [] A_prime; A_prime = nullptr; }
    if (left_min != nullptr) {delete [] left_min; left_min = nullptr; }
    if (right_min != nullptr) {delete [] right_min; right_min = nullptr; }
    if (lsb_map != nullptr) {delete [] lsb_map; lsb_map = nullptr; }
}

// range minimum query on LCP
int LCS::rmq(int i, int j) {
    if (i == j) return lcp[i];

    if (i > j) {// ensure i < j
        int tmp = i; i = j; j = tmp;
    }

    int i_block = i / block_size;
    int j_block = j / block_size;

    if (i_block == j_block) { // i and j in same block
        int w = L[j] & (~0 << i % block_size); // clear all bits with index smaller than i
        if (w == 0) return lcp[j];
        // use precomputed lsb map for O(1) instead of logarithmic time
        return lcp[i_block*block_size + lsb_map[w]];
    }

    // i and j in different blocks
    // largest power of two that fits between i and j blocks, not inclusive
    int num_blocks = (j_block-1) - (i_block+1) + 1;
    
    if (num_blocks > 0) {
        int k = (int)log2(num_blocks);
        int block_min = get_min(A_prime[ M[i_block+1][k] ], 
            A_prime[ M[(j_block-1) - (1 << k) + 1][k] ]);
        
        // minimum between (1) min values from blocks between i and j,
        //  not including i and j's blocks, (2) from i rightwards to the end of its
        //  block, and (3) from j leftwards to the beginning of its block.
        return get_min(block_min, get_min(right_min[i], left_min[j]));
    }

    return get_min(right_min[i], left_min[j]);
}

void LCS::st_preprocess() {
    // preprocess LCP array for O(1) range minimum queries
    // https://www.geeksforgeeks.org/range-minimum-query-for-static-array/
    block_size = (int)log2(n); // array will be divided up into blocks
    G = new int[n]; // G[i] = index of min value from the start of i's block up to i
    // since block size is a log, we know it cannot be larger than the size of "int" data type
    // so each label l_i can fit in an int
    L = new int[n];
    left_min = new int[n]; // left_min[i] = minimum value over range [a,i], where a is the start of a block
    right_min = new int[n]; // right_min[i] = minimum value over range [i,b], where b is the start of a block

    A_len = (int) ceil((double) n / (double) block_size); // each block is reduced to its min in the array A'
    log_A_len = (int)ceil(log2(A_len));
    A_prime = new int[A_len]; // A_prime[i] = minimum value in the ith block
    M = new int*[A_len]; // lookup table for sparse table on A', 2d array

    for (int i = 0; i < n; i++) G[i] = -1; // initialize G

    for (int a = 0; a < n; a += block_size) { // populate G and A'
        int argmin = a; 
        left_min[a] = lcp[argmin];
        G[a] = -1;

        int last_index = a; // index of last element in block, in case block is not completely full

        for (int b = 1; b < block_size and a+b < n; b++) { // compute g_i for each i
            if (lcp[argmin] > lcp[a+b]) argmin = a+b;
            left_min[a+b] = lcp[argmin]; // compute left_min
            last_index = a+b; // update last index
        }

        A_prime[a / block_size] = lcp[argmin]; // minimum value in this block
    
        argmin = last_index;

        for (int t = last_index; t >= a; t--) {
            if (lcp[t] < lcp[argmin]) argmin = t;
            right_min[t] = lcp[argmin]; // compute right_min
        }

        // compute G[i]
        vector<int> stack;

        // I think this should be linear time?
        for (int t = last_index-1; t >= a; t--) {
            if (lcp[t] < lcp[t+1]) G[t+1] = t-a;
            else                   stack.push_back(t+1);

            while (stack.size() > 0 and lcp[t] < lcp[stack.back()]) {
                G[stack.back()] = t-a;
                stack.pop_back();
            }
        }
    }

    for (int i = 0; i < n; i++) { // compute L_i
        int block_start = (i / block_size) * block_size;
        if (i % block_size == 0) L[i] = 0;
        else if (G[i] == -1)     L[i] = 0;
        else                     L[i] = L[block_start+G[i]] | (1 << G[i]);
    }

    for (int i = 0; i < A_len; i ++) { // initialize M w/ max vals
        M[i] = new int[log_A_len];
        for (int j = 0; j < log_A_len; j++) M[i][j] = n;
    }

    for (int i = 0; i < A_len; i++) { // sparse table for A' using dynamic programming
        M[i][0] = i; // for queries of length 2^0 = 1, min is just the only element
    }

    for (int j = 1; j < log_A_len; j++) { // compute values from smaller to bigger intervals
        for (int i = 0; (i + (1<<j) -1) < A_len; i++) {
            if (A_prime[ M[i][j-1] ] <
                A_prime[ M[i + (1 << (j-1))][j-1] ])
                M[i][j] = M[i][j-1];
            else
                M[i][j] = M[i + (1 << (j-1))][j-1];
        }
    }

    // preprocess lsb map to be used in RMQs
    // https://www.academia.edu/download/86383235/d059dda279b1aed4a0301e4e46f9daf65174.pdf
    lsb_map = new int[n+1]; //longest possible word w in RMQ is 
                          //2^(log n) - 1 <= n, this happens when at end of
                          //log n block and all previous block elements are set
    lsb_map[0] = lsb_map[1] = 0; // lsb_map[i] = least significant bit in binary representation of i, zero-indexed
    for (int i = 2; i <= n; i++) { // O(n), dynamic programming :^)
        lsb_map[i] = (i & 1) ? 0 : 1 + lsb_map[i / 2];
    }
}

void LCS::initialize (uint8_t *s, int l) {
    do_free();
    
    n = l+1;
    T = new int[n]; 
    sa = new int[n]; lcp = new int[n];

    for (int i = 0; i < n; i++) T[i] = (unsigned int) s[i]+1;
    T[l] = 0;

    suffix_array(T, sa, n, 257);
    lcp_array(T, n);
    st_preprocess();
}

int LCS::get_lcp(int i, int j) {
    if (rank[i] < rank[j]) {
        return rmq(rank[i]+1, rank[j]);
    }

    return rmq(rank[j]+1, rank[i]);
}


// find the longest common sequence of numbers between two sequences
int LCS::longest_common_substring (uint8_t *s1, uint8_t *s2, int l1, int l2) {
    // concatenate strings with sentinels
    n = l1 + l2 + 2; // one sentinel between the strings and one at the end, hence +2
    T = new int[n]; 
    sa = new int[n]; // suffix array
    lcp = new int[n]; // LCP array
    // "color" of each suffix; whether it starts w/ a character from l1 (1), l2 (2), or a sentinel (0)
    int *color = new int[n];

    for (int i = 0; i < l1; i++) T[i] = (unsigned int) s1[i] + 2; // increase each element by 2 so that 0,1 can be sentinels
    for (int i = 0; i < l2; i++) T[l1 + 1 + i] = (unsigned int) s2[i] + 2;
    T[l1] = 0;
    T[l1 + l2 + 1] = 1; // add sentinels

    // construct suffix array
    suffix_array(T, sa, n, 258); // 256 distinct byte values + 2 sentinels

    // only care about lcs that start at the beginning of s2, so get its own color
    for (int i = 0; i < n; i++) { // construct color array
        if (sa[i] == l1+1)   color[i] = 2; // start of s2
        else if (sa[i] < l1) color[i] = 1;
        else                 color[i] = 0;
    }

    lcp_array(T, n);    // construct LCP array
    st_preprocess (); // do preprocessing for RMQs
    
    // sliding window
    int window_start = 2, window_end = 2;
    int color_in_window[3]; // color_in_window[i] = # of elements in window w/ color i
    color_in_window[0] = color_in_window[1] = color_in_window[2] = 0;

    int lcs_length = 0; // length of LCS
    int lcs_start_idx = 0; // index in l1 where LCS starts
    int idx_of_s1 = 0; // start index of most recent 1-color suffix in window
    int idx_of_s2 = 0; // minimum index of most recent 2-color suffix in window
                       //   we only report lcs's if this idx is 0
    color_in_window[color[window_end]] ++; // get color of new element
    if (color[window_end] == 1) idx_of_s1 = sa[window_end];

    // TODO: only get LCS that starts at 0 in s2
    // while (window_end < n and window_start < n-1) {
    //     // both non-sentinel colors represented, expand down
    //     if (color_in_window[1] == 0 or color_in_window[2] == 0) {
    //         window_end ++;
    //         if (window_end < n) {
    //             color_in_window[color[window_end]] ++; // get color of new element
    //             if (color[window_end] == 1) idx_of_s1 = sa[window_end];
    //             else if (color[window_end] == 2)
    //                 idx_of_s2 = get_min(idx_of_s2, sa[window_end]);
    //         }
    //     } else {
    //         int this_lcs_len = rmq(window_start+1, window_end);

    //         if (this_lcs_len > lcs_length) { // new best LCS found
    //             lcs_length = this_lcs_len; lcs_start_idx = idx_of_s1;
    //         }

    //         // decrease window size by bringing start down
    //         color_in_window[color[window_start]]--; window_start ++; 
    //     }

    // }


    length = lcs_length; start_index = lcs_start_idx;

    do_free(); delete [] color;

    return 0;
}

// Skew algorithm: linear time algorithm for suffix array and LCP calculation
// source: http://www.cs.cmu.edu/~guyb/paralg/papers/KarkkainenSanders03.pdf#cite.AGKR02
inline bool leq(int a1, int a2, int b1, int b2) // lexicographic order
{ return (a1 < b1 || (a1 == b1 && a2 <= b2)); }            // for pairs

inline bool leq(int a1, int a2, int a3, int b1, int b2, int b3)
{ return (a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3))); } // and triples

inline int get_char(int *s, int offset, int s_len) 
{ return (offset < s_len) ? s[offset] : 0; }

// stably sort a[0...n-1] to b[0...n-1] with keys 0...K from string r
void radix_pass(int *a, int *b, int *r, int n, int K, int offset, int r_len) {
    int *c = new int[K + 1];                    // counter array
    for (int i = 0; i <= K; i++) c[i] = 0;      // initialize counter
    for (int i = 0; i < n; i++)
        c[get_char(r, a[i]+offset, r_len)]++;   // count occurences of each character

    for (int i = 0, sum = 0; i <= K; i++) {     // turn it into prefix array
        int t = c[i];
        c[i] = sum;
        sum += t;
    }

    // put a[i] in the prefix denoted by c[r[a[i]]], then increment the prefix
    //  so next a[i] will be put after it
    for (int i = 0; i < n; i++) 
        b[c[get_char(r, a[i]+offset, r_len)]++] = a[i]; // sort

    delete [] c;
}

// find the suffix array SA of s[0...n-1] in alphabet of size K
// require s[n]=s[n+1]=s[n+2]=0 and n >= 2
void LCS::suffix_array(int *s, int *SA, int n, int K) {
    int n0 = (n+2)/3, n1 = (n+1)/3, n2 = n/3, n02 = n0+n2;
    
    int *s12 = new int[n02+3];
    s12[n02] = s12[n02+1] = s12[n02+2] = 0; // pad the end of the string w/ zeros

    int *SA12 = new int[n02 + 3];
    SA12[n02] = SA12[n02+1] = SA12[n02+2] = 0;

    int *s0 = new int[n0];
    int *SA0 = new int[n0];

    /* First step: sort suffixes S_i with i % 3 != 0 */

    // +(n0-n1) generates a dummy mod 1 suffix if n%3 == 1
    for (int i = 0, j = 0; i < n+(n0-n1); i++) if (i%3 != 0) s12[j++] = i;

    // lsb radix sort the mod 1 and mod 2 triplets
    radix_pass(s12, SA12, s, n02, K, 2, n); // radix pass on every third character
    radix_pass(SA12, s12, s, n02, K, 1, n); // on every second character
    radix_pass(s12, SA12, s, n02, K, 0, n); // on every first character

    // find lexicographic names of triples
    int name = 0, c0 = -1, c1 = -1, c2 = -1;

    for (int i = 0; i < n02; i++) {
        if (get_char(s,SA12[i],n) != c0 or get_char(s,SA12[i]+1,n) != c1 or get_char(s,SA12[i]+2,n) != c2) {
            name ++; // names start at 1
            c0 = get_char(s,SA12[i],n); c1 = get_char(s,SA12[i]+1,n); c2 = get_char(s,SA12[i]+2,n);
        }
        // s12 = [s_i : i % 3 = 1] + [s_i : i % 3 = 2]
        //  and s12 is of length 2n/3, so first half (0 to n/3) is for mod 1
        //  and second half (n/3 to 2n/3) is for mod 2
        if (SA12[i] % 3 == 1)
            s12[SA12[i]/3] = name;    // left half
        else {
            s12[SA12[i]/3 + n0] = name; // right half
        }
    }

    // recurse if names are not unique
    if (name < n02) {
        suffix_array(s12, SA12, n02, name);
        // store unique names in s12 using the suffix array
        for (int i = 0; i < n02; i++) s12[SA12[i]] = i + 1;
    } else { // generate suffix array of s12 directly
        for (int i = 0; i < n02; i++) SA12[s12[i] - 1] = i;
    }



    // stably sort the mod 0 suffixes from SA12 by their first character
    for (int i=0, j=0; i < n02; i++) if (SA12[i] < n0) s0[j++] = 3*SA12[i];
    radix_pass(s0, SA0, s, n0, K, 0, n);

    // merge sorted SA0 suffixes and SA12 suffixes
    for (int p=0, t=n0-n1, k=0; k < n; k++) {
        #define GetI() (SA12[t] < n0 ? SA12[t]*3 + 1 : (SA12[t]-n0)*3 + 2)
        int i = GetI(); // position of current offset 12 suffix
        int j = SA0[p]; // position of current offset 0  suffix
        if (SA12[t] < n0 ? // different compares for mod 1 and mod 2 suffixes
            leq(s[i], get_char(s12,SA12[t]+n0,n02+3), s[j], get_char(s12,j/3,n02+3)) :
            leq(s[i], s[i+1], get_char(s12,SA12[t]-n0+1,n02+3), s[j], get_char(s,j+1,n), get_char(s12,j/3+n0,n02+3))) // compare triple here because i+1 mod 3 is 0
        { // suffix from SA12 is smaller
            SA[k] = i; t++;
            if (t == n02) // done -- only SA0 suffixes left
                for (k ++; p < n0; p++, k++) SA[k] = SA0[p];
        } else { // suffix from SA0 is smaller
            SA[k] = j; p++;
            if (p == n0) // done -- only SA12 suffixes left
                for (k++; t < n02; t++, k++) SA[k] = GetI();
        }
    }

    delete [] s12; delete [] SA12; delete [] SA0; delete [] s0;
}


// O(n) LCP array construction using Kasai's algorithm
// http://alumni.cs.ucr.edu/~rakthant/cs234/01_KLAAP_Linear%20time%20LCP.PDF
// https://www.youtube.com/watch?v=VpEumOuakOU
void LCS::lcp_array(int *s, int n) {
    for(int i = 0; i < n; i++) lcp[i] = 0;

    rank = new int[n];

    // rank is inverse of suffix array, i.e. rank[i] = k <=> SA[k] = i
    for (int i = 0; i < n; i++) rank[sa[i]] = i;

    int l = 0;
    for (int i = 0; i < n; i++) {
        if (rank[i] > 0) {
            int j = sa[rank[i]-1];
            
            while (i+l < n && j+l < n && s[i+l] == s[j+l]) l++;

            lcp[rank[i]] = l;
            l = get_max(l-1, 0);
        }
    }

}

void LCS::self_test_rmq (bool debug_print) {
    do_free();
    srand (time(NULL)); // initialize random seed
    
    n = rand() % 100 + 1; // length from 1 to 100
    lcp = new int[n];

    for (int i = 0; i < n; i ++) {
        lcp[i] = rand() % 255;
    }

    if (debug_print) {
        cout << "LCP of length " << n << ":" << endl << "{";
        for (int i = 0; i < n; i ++) {
            cout << lcp[i] << ", ";
        }
        cout << "}" << endl;
    }

    st_preprocess (); // do preprocessing for RMQs
    
    bool passed_test = true;

    // check lsb_map
    for (int i = 0; i <= n; i++) {
        int i2 = i, lsb = 0;
        
        if (i2 > 1) {
            while ((i2 & 1) == 0) {
                lsb++; i2 >>= 1;
            }
        }

        if (lsb != lsb_map[i]) {
            passed_test = false;
            cout << "lsb wrong for " << i << ", got " << lsb_map[i] << " instead of " << lsb << endl;
        }
    }


    for (int i = 0; i < n; i++) {
        for (int j = i; j < n; j++) {
            int res = rmq(i, j);
            int min = 1000;
            for (int k = i; k <= j; k++) min = get_min(min, lcp[k]);

            if (res != min) {
                passed_test = false;
                cout << "wrong for [" << i << "," << j << "]: " << res << " instead of " << min;
                if (i / block_size == j /block_size) cout << " and in same block"; 
                cout << endl;
            }
        }
    }

    if (passed_test) cout << "RMQ self-test passed." << endl;

}