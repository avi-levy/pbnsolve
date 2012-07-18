/* Copyright 2007 Jan Wolter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>
#include "config.h"

/* Functions for computing gamma functions and factorials.  Could use C99
 * lgamma() but that would be less portable.
 */

double gammacoef[]= {
     76.18009173,
     -86.50532033,
      24.01409822,
      -1.231739516,
       0.120858003e-2,
      -0.536382e-5
};

double gammln(double xx)
{
    int j;
    double x= xx-1;
    double tmp= x + 5.5;
    double ser= 1.0;
    tmp-= (x + 0.5) * log(tmp);

    for (j= 0; j <= 5; j++)
    {
	x+= 1.0;
	ser+= gammacoef[j] / x;
    }
    return -tmp + log(2.50662827465 * ser);
}

#define MAX_CACHE 100
double factln_cache[MAX_CACHE];
char factln_known[MAX_CACHE];

void init_factln()
{
    int i;
    for (i= 0; i < MAX_CACHE; i++)
    	factln_known[i]= 0;
}

double factln(int n)
{
    double fact;

    if (n <= 1) return 0;

    if (n <= MAX_CACHE && factln_known[n])
	return factln_cache[n];

    fact= gammln((double)(n+1));

    if (n <= 100)
    {
	factln_cache[n]= fact;
	factln_known[n]= 1;
    }

    return fact;
}

/* bico(N,K) - return N choose K. */

int bico(int n, int k)
{
    return floor(0.5 + exp(factln(n) - factln(k) - factln(n-k) ) );
}

/* bicoln(N,K) - return natural log of the binomial coefficient N choose K. */
double bicoln(int n, int k)
{
    return factln(n) - factln(k) - factln(n-k);
}
