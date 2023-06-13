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

int longest_common_substring () {
    return 0;
}

// Skew algorithm: linear time algorithm for suffix array and LCP calculation
// source: http://www.cs.cmu.edu/~guyb/paralg/papers/KarkkainenSanders03.pdf#cite.AGKR02
inline bool leq(int a1, int a2, int b1, int b2) // lexicographic order
{ return (a1 < b1 || (a1 == b1 && a2 <= b2)); }            // for pairs

inline bool leq(int a1, int a2, int a3, int b1, int b2, int b3)
{ return (a1 < b1 || (a1 == b1 && leq(a2, a3, b2, b3))); } // and triples

// stably sort a[0...n-1] to b[0...n-1] with keys 0...K from string r
void radix_pass(int *a, int *b, int *r, int n, int K) {
    int *c = new int[K + 1];                    // counter array
    for (int i = 0; i <= K; i++) c[i] = 0;      // initialize counter
    for (int i = 0; i < n; i++) c[r[a[i]]]++;   // count occurences of each character

    for (int i = 0, sum = 0; i <= K; i++) {     // turn it into prefix array
        int t = c[i];
        c[i] = sum;
        sum += t;
    }

    // put a[i] in the prefix denoted by c[r[a[i]]], then incremenet the prefix
    //  so next a[i] will be put after it
    for (int i = 0; i < n; i++) b[c[r[a[i]]]++] = a[i]; // sort

    delete [] c;
}

// range minimum query -- if in block
// https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=2151c8d46282010599d125f01015dc2fc3d2fe4d
// https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=7e70b4ca7d8f7598dccdf0b9f671b30f628fea9f
int rmq(int *L, int *LCP12, int i, int j, int block_size, int *left_min, int *right_min, int *M, int log_A_len) {
    if (i > j) { // ensure i < j
        int tmp = i;
        i = j;
        j = tmp;
    }

    int i_block = i / block_size;
    int j_block = j / block_size;

    if (i_block == j_block) { // i and j in same block
        int w = L[j] & (~0 << i);

        if (w == 0) return LCP12[j];

        return 0;
    }

    // i and j in different blocks
    // largest power of two that fits between i and j blocks, not inclusive
    int num_blocks = (j_block-1) - (i_block+1) + 1;

    if (num_blocks != 0) {
        int k = (int)log2(num_blocks);

        int block_min = get_min(LCP12[ M[(i_block+1)*log_A_len + k] ], LCP12[ M[((j_block-1) - (1 << k) + 1)*log_A_len + k] ]);

        // minimum between (1) min values from blocks between i and j,
        //  not including i and j's blocks, (2) from i rightwards to the end of its
        //  block, and (3) from j leftwards to the beginning of its block.
        return get_min(block_min, get_min(right_min[i], left_min[j]));
    }

    return get_min(right_min[i], left_min[j]);
}

void calculate_LCP(int *s, int *SA, int *LCP, int *s12, int *LCP12, int n, int n02) {
    // preprocess LCP12 for O(1) range minimum queries
    int block_size = (int)log2(n02); // number of bits of a number = log2 of that number

    int *G = new int[n02]; // G[i] = index of min value from the start of i's block up to i
    // since block size is a log, we know it cannot be larger than an int
    // so each label l_i can fit in an int
    int *L = new int[n02]; 
    int *left_min = new int[n02]; // left_min[i] = minimum value over range [a, i], where a is the start of a block
    int *right_min = new int[n02]; // right_min[i] = minimum value over range [i,b], where b is the end of a block

    int A_len = n02 / block_size;
    int log_A_len = (int)log2(A_len);
    int *A_prime = new int[A_len]; // A_prime[i] = minimum value in ith block
    
    int *M = new int[A_len * log_A_len]; // lookup table for sparse table on A'

    // populate G and A'
    for (int a = 0; a < n02; a += block_size) {
        int argmin = a;
        // compute g_i for each i
        for (int b = 1; b < block_size; b++) {
            if (LCP12[argmin] < LCP12[a+b]) G[a+b] = argmin;
            else                               argmin = a+b;
        }
        // minimum value in this block        
        A_prime[a / block_size] = LCP[argmin];
    }

    // sparse table for A' using dynamic programming
    for (int i = 0; i < A_len; i++) {
        // for queries of length 1, min is just the only element
        M[i*log_A_len + 0] = i;
    }

    // Compute values from smaller to bigger intervals
    for (int j = 1; (1 << j) <= A_len; j++) {
        // Compute minimum value for all intervals with size 2^j
        for (int i = 0; (i + (1 << j) -1) < A_len; i++) {
            if (LCP12[ M[i*log_A_len + (j-1)] ] < 
                LCP12[ M[(i + (1 << (j-1)))*log_A_len + (j-1)] ])
                M[i*log_A_len + j] = M[i*log_A_len + (j-1)];
            else
                M[i*log_A_len + j] = M[(i + (1 << (j-1)))*log_A_len + (j-1)];
        }
    }

    // compute L_i
    for (int i = 0; i < n02; i++) {
        if (i == 0) {
            L[i] = 0;
        } else {
            L[i] = L[G[i]] | (1 << G[i]);
        }
    }

    for (int i = 0; i < n-1; i ++) {
        int j = SA[i];
        int k = SA[i+1];
        
        cout << j << " " << k << " test" << endl;

        if (j % 3 == 1 or j % 3 == 2) {
            int j2 = (j % 3 == 1) ? (j - 1)/3 : (n+j-2)/3;

            if (k % 3 == 2 or k % 3 == 1) {
                // int k2 = (k % 3 == 2) ? (n+k-2)/3 : (k-1)/3; // k'
                // since j and k adjacent in SA, j2 and k2 adjacent in SA12
                int l = LCP12[s12[j2]-1];

                int lcp = 3*l;

                // if there is shared lcp beyond the blocks of three (i.e. a remainder, at most 2)
                for (int t=0,a=j+3*l, b=k+3*l; t < 2; t++) {
                    if (a+t >= n or b+t >= n) break;
                    if (s[a+t] == s[b+t]) lcp++;
                }

                LCP[i] = lcp;
            
            } else { // k % 3 = 0
                if (s[j] != s[k]) LCP[i] = 0; 
                else // range minimum query to compute l
                    LCP[i] = 1 + rmq(L, LCP12, j+1, k+1, block_size, left_min, right_min, M, log_A_len);
            }
        } else { // j % 3 = 0
            if (s[j] != s[k]) LCP[i] = 0;
            else // range minimum query to compute l
                LCP[i] = 1 + rmq(L, LCP12, j+1, k+1, block_size, left_min, right_min, M, log_A_len);
        }
    }

    delete [] G; delete [] L; delete[] left_min; delete [] right_min;
    delete [] A_prime; delete [] M;
}


// find the suffix array SA of s[0...n-1] in alphabet of size K
// require s[n]=s[n+1]=s[n+2]=0 and n >= 2
void suffix_array(int *s, int *SA, int *LCP, int n, int K) {
    int n0 = (n+2)/3, n1 = (n+1)/3, n2 = n/3, n02 = n0+n2;
    
    int *s12 = new int[n02+3];
    s12[n02] = s12[n02+1] = s12[n02+2] = 0; // pad the end of the string w/ zeros

    int *SA12 = new int[n02 + 3];
    SA12[n02] = SA12[n02+1] = SA12[n02+2] = 0;

    int *LCP12 = new int[n02];

    int *s0 = new int[n0];
    int *SA0 = new int[n0];

    /* First step: sort suffixes S_i with i % 3 != 0 */

    // +(n0-n1) generates a dummy mod 1 suffix if n%3 == 1
    for (int i = 0, j = 0; i < n+(n0-n1); i++) if (i%3 != 0) s12[j++] = i;

    // lsb radix sort the mod 1 and mod 2 triplets
    radix_pass(s12, SA12, s+2, n02, K); // radix pass on every third character
    radix_pass(SA12, s12, s+1, n02, K); // on every second character
    radix_pass(s12, SA12, s,   n02, K); // on every first character

    // find lexicographic names of triples
    int name = 0, c0 = -1, c1 = -1, c2 = -1;

    for (int i = 0; i < n02; i++) {
        if (s[SA12[i]] != c0 or s[SA12[i]+1] != c1 or s[SA12[i]+2] != c2) {
            name ++; // names start at 1

            if (i > 0) {
                int overlap = 0;

                if (c0 == s[SA12[i]] and c1 == s[SA12[i]+1]) overlap = 2;
                else if (c0 == s[SA12[i]]) overlap = 1;

                LCP12[i-1] = overlap;                    
            }

            c0 = s[SA12[i]]; c1 = s[SA12[i]+1]; c2 = s[SA12[i]+2];
        } else
            if (i > 0) LCP12[i-1] = 3;
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
        suffix_array(s12, SA12, LCP12, n02, name);
        // store unique names in s12 using the suffix array
        for (int i = 0; i < n02; i++) s12[SA12[i]] = i + 1;
    } else { // generate suffix array of s12 directly
        for (int i = 0; i < n02; i++) SA12[s12[i] - 1] = i;
    }



    // stably sort the mod 0 suffixes from SA12 by their first character
    for (int i=0, j=0; i < n02; i++) if (SA12[i] < n0) s0[j++] = 3*SA12[i];
    radix_pass(s0, SA0, s, n0, K);

    // merge sorted SA0 suffixes and SA12 suffixes
    for (int p=0, t=n0-n1, k=0; k < n; k++) {
        #define GetI() (SA12[t] < n0 ? SA12[t]*3 + 1 : (SA12[t]-n0)*3 + 2)
        int i = GetI(); // position of current offset 12 suffix
        int j = SA0[p]; // position of current offset 0  suffix
        if (SA12[t] < n0 ? // different compares for mod 1 and mod 2 suffixes
            leq(s[i], s12[SA12[t] + n0], s[j], s12[j/3]) :
            leq(s[i], s[i+1], s12[SA12[t]-n0+1], s[j], s[j+1], s12[j/3+n0])) // compare triple here because i+1 mod 3 is 0
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

    // at this point, s12 contains SA12' (bar over SA12), the unique names of the triples
    // compute LCP from LCP12
    calculate_LCP(s, SA, LCP, s12, LCP12, n, n02);

    // cout << "LCP: ";
    // for (int i = 0; i < n02; i++) cout << LCP12[i] << " ";
    // cout << endl;

    delete [] s12; delete [] SA12; delete [] SA0; delete [] s0; delete [] LCP12;
}