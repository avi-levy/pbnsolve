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

#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "bitstring.h"

#ifdef LIMITCOLORS
void fbit_init(int n)
{
    if (n > fbit_n)
    {
	printf("Number of colors cannot exceed %d\n"
	    "Recompile with LIMITCOLORS off to remove this limitation\n",
	    fbit_n);
	exit(1);
    }
}
#else

int fbit_n;
int fbit_size;

void fbit_init(int n)
{
    int i;

    /* Number of bits in our standard bitstring size */
    fbit_n= n;

    /* Number of ints required to store that many bits */
    fbit_size= bit_size(n);
}
#endif
