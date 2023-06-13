#ifndef LCS_H
#define LCS_H

#include <iostream>
#include <stdint.h>
#include <string.h>
#include <math.h>       /* log2 */

#include "util.h"

int longest_common_substring();
void suffix_array(int *s, int *SA, int *LCP, int n, int K);

#endif