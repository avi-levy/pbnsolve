/* Copyright 2010 Jan Wolter
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

/* Line solution cache
 *
 * The idea here is that we put all results from the line solver into a hash
 * table, keyed by the clues and the current line state, storing the line
 * state after the line solver has run.  We check this before running the
 * line solver.  If there is a hit, we can avoid the run.
 *
 * The hash table is of fixed size.  If it gets full (or rather 90% full),
 * we simply empty it and start over.
 *
 * If there puzzle is rectangular, we have separate tables for the rows and
 * columns, but if it is square, they are all in the same hash table.
 *
 * The hash key consists of (1) the clues and (2) the current state of the line.
 * The clues are encoded with clue numbers, which are different for each row
 * and column, except when two happen to have the same clues, in which case they
 * get the same clue id.  The line state is pretty much as usual, except the
 * bits are packed together more closely to reduce memory consumption.
 *
 * The hash algorithm is a standard double probing algorithm.
 */

#include "pbnsolve.h"

/* Number of slots in the hash table.  THIS MUST ALL BE PRIME NUMBERS, each
 * about double the size of the previous one.  Each time the cache is filled,
 * we empty it and replace it with a bigger one.
 */

int nslot[]= {
     25013,
     50021,
    100003,
    200003,
    400009,
    800011,
    	0};

/* Hash Element Structure.  clid is zero if this slot is empty.  "data" is
 * actually two variable length bit strings, oldstate and newstate, oldstate
 * being part of the key, and new state being the value.
 */

typedef struct {
    line_t clid;	/* Clue ID.  Identifies the clue.  Part of key */
    bit_type data[2];	/* The old and new line states.  Actual length is
                           2*hash.len */
} HashElem;

#define clueid(e) (e)->clid
#define oldstate(e) (e)->data
#define newstate(h,e) ((e)->data+(h)->len)
#define HashElemSize(h) (sizeof(HashElem) + (2*(h)->len - 2) * sizeof(bit_type))

typedef struct {
    int len;		/* Length (in number of longs) of keys and values */
    int esize;		/* Element size in bytes - just HashElemSize(h) */
    long nslots;	/* Number of slots in the hash table */
    int nsloti;		/* Index into nslot[] array for current slot size */
    long flushat;	/* Flush the hash table if it gets this full */
    long n;		/* Number of data elements currently in hash */
    long lastslot;	/* Slot found by last search - -1 if no last search */
    char *hash;		/* Pointer to the memory containing the hash */
} LineHash;

#define HashSlot(h,i) (HashElem *)&((h)->hash[i * (h)->esize])


/* These are the roots of the caches.  We have two, one for rows and one for
 * columns, though they are merged for square puzzles */
static LineHash *cache[2];

/* This attaches a line ID to each row and each column.  Normally each one
 * would have a different ID number, but if two lines are the same length
 * and have the same clues, then they have the same clue id.  If one clue
 * is the same as another clue reversed, then the first one found gets a
 * positive clue id, and the second gets the negative of that value.
 */
static line_t *clid[2];

/* A temporary storage place for a compressed bit array */
static bit_type *tmp;

/* A storage place for an uncompressed solution */
static bit_type *col;
#define colbit(i) (col+(fbit_size*(i)))

/* Forward declarations of some functions */
void compress_line(Puzzle *puz, Solution *sol,
		   dir_t k, line_t i, line_t ncell, bit_type *out);
void rev_compress_line(Puzzle *puz, Solution *sol,
		   dir_t k, line_t i, line_t ncell, bit_type *out);
void uncompress_line(bit_type *in, int ncell, int ncolor, bit_type *out);
void rev_uncompress_line(bit_type *in, int ncell, int ncolor, bit_type *out);
void dump_comp(bit_type *c, int ncell, int ncolor);

/* Hash statistics */
long cache_req= 0;
long cache_hit= 0;
long cache_add= 0;
long cache_flush= 0;

/* INIT_HASH: Some initialization of a hash in an empty state.  Does not
 * allocate the hash->hash array.
 */

void init_hash(LineHash **hash, line_t ncell, color_t ncolor)
{
    *hash= (LineHash *)malloc(sizeof(LineHash));
    (*hash)->len= bit_size( ncell * ncolor );
    (*hash)->esize= HashElemSize(*hash);
    (*hash)->nsloti= 0;
    (*hash)->n= 0;
    (*hash)->lastslot= -1;
}


/* ALLOC_HASH:  The rest of init_hash. */

void alloc_hash(LineHash *hash)
{
    hash->nslots= nslot[hash->nsloti];
    hash->flushat= hash->nslots * 9 / 10;
    hash->hash= (char *)calloc(hash->nslots, hash->esize);
}

/* EMPTY_HASH: Flush out the hash table, deleting all entries.
 */

void empty_hash(LineHash *hash)
{
    hash->n= 0;
    hash->lastslot= -1;
    if (nslot[hash->nsloti+1] > 0)
    {
	hash->nslots= nslot[++hash->nsloti];
	hash->flushat= hash->nslots * 9 / 10;
	free(hash->hash);
	hash->hash= (char *)calloc(hash->nslots, hash->esize);
    }
    else
	memset(hash->hash, 0, hash->nslots * hash->esize);
    if (VH) printf("H: New hash size=%d\n",hash->nslots);
    cache_flush++;
}


/* MATCH_CLUE:  Does any clue in direction k, with an index less than n
 * match the given clue?  If not return 0.  If so return the clue id of
 * that clue.  If it matches a reversed clue, then return the negative of
 * the clue id.
 */

int match_clue(Clue *clue, Puzzle *puz, int k, int n)
{
    Clue *c;
    int i, j;
    int len;

    for (i= 0; i < n; i++)
    {
	c= &(puz->clue[k][i]);
	len= c->n;
	if (len != clue->n) goto next;
	/* Try matching two clues */
	for (j= 0; j < len; j++)
	{
	    if (clue->length[j] != c->length[j] ||
		clue->color[j] != c->color[j]) goto rev;
	}
	return clid[k][i];
rev:	/* Try matching to reverse of clue */
	if (len < 2) goto next;
	for (j= 0; j < len; j++)
	{
	    if (clue->length[len-j-1] != c->length[j] ||
		clue->color[len-j-1] != c->color[j]) goto next;
	}
	return -clid[k][i];
next:;
    }
    return 0;
}


/* INIT_CACHE: Constructs the caches for a puzzle
 */

void init_cache(Puzzle *puz)
{
    int k, i, square;
    int nextclid= 1;
    int maxdimension= 0;

    if (VH) printf("H: Initializing Hash.\n");

    /* For now, this only works for grids */
    if (puz->type != PT_GRID)
    {
	cachelines= 0;
	return;
    }

    /* Construct the hash for rows */
    init_hash(&(cache[D_ROW]), puz->n[D_COL], puz->ncolor);

    /* Construct the hash for columns */
    square= (puz->n[D_ROW] == puz->n[D_COL]);
    if (!square)
    {
	/* For rectangular puzzles,
	 * we use separate caches for rows and columns */
	init_hash(&(cache[D_COL]), puz->n[D_ROW], puz->ncolor);
	alloc_hash(cache[D_ROW]);
	if (VH) printf("H:   Using two hash tables.\n");
    }
    else
    {
	/* For square puzzles, we use just one cache but double size */
	cache[D_ROW]->nsloti++;
	cache[D_COL]= cache[D_ROW];
	if (VH) printf("H:   Using one hash table.\n");
    }
    alloc_hash(cache[D_COL]);

    /* Build clue id arrays */
    clid[D_ROW]= (line_t *)malloc(sizeof(line_t) * puz->n[D_ROW]);
    clid[D_COL]= (line_t *)malloc(sizeof(line_t) * puz->n[D_COL]);

    if (VH) printf("H:   Assigning Clue IDs:\n");

    /* Assign a clue id to each row and column - not a terribly fast algorithm
     * but who cares?  Puzzles aren't that big. */
    for (k= 0; k < 2; k++)
    {
	if (puz->n[k] > maxdimension) maxdimension= puz->n[k];

	for (i= 0; i < puz->n[k]; i++)
	{
	    if ((clid[k][i]= match_clue(&(puz->clue[k][i]), puz, k, i)) == 0)
	    {
		if (k == 0 || !square ||
		    (clid[k][i]= match_clue(&(puz->clue[k][i]),
					    puz, 0, puz->n[0])) == 0)
		{
		    clid[k][i]= nextclid++;
		}
	    }
	    if (VH) printf("H:     %s %d => %d\n",
			cluename(puz->type,k), i, clid[k][i]);
	}
    }

    /* Allocate storage for a compressed row or column */
    tmp= (bit_type *)malloc(
	    bit_size(maxdimension * puz->ncolor) * sizeof(bit_type));

    /* Allocate storage for an uncompressed row or column */
    col= (bit_type *)malloc( maxdimension * fbit_size * sizeof(bit_type));
}


/* HASH_INDEX: Given a clue id and a compressed line, generate an integer
 * index by hashing it all together.
 */

bit_type hash_index(line_t clid, bit_type *line, int len)
{
    int i;
    bit_type result= clid;

    for (i= 0; i < len; i++)
	/* This is  result= 9*result + line[i] */
	result+= (result<<3) + line[i];
    return result;
}



/* HASH_FIND: Given a key, find the entry in the hash table.  If found, return
 * the index.  If not found, return the index of the empty cell we found
 * instead.  If the table is full return a -1, but that should never happen.
 */

int hash_find(LineHash *hash, line_t clid, bit_type *line)
{
    int i,j;
    HashElem *e;
    bit_type v= hash_index(clid, line, hash->len);
    int index= v % hash->nslots;
    int offset= (v % (hash->nslots - 2)) + 1;

    if (VH) printf("H: hash search - index=%d offset=%d\n", index, offset);
    cache_req++;

    for (i=0; i < hash->nslots; i++)
    {
	e= HashSlot(hash,index);

	/* check for empty slot */
	if (clueid(e) == 0)
	{
	    if (VH) printf("H:   slot %d - empty\n", index);
	    return index;
	}

	/* check for hit */
	if (clueid(e) == clid && oldstate(e)[0] == line[0])
	{
	    for (j= 1; j < hash->len; j++)
		if (oldstate(e)[j] != line[j])
		    goto nope;
	    if (VH) printf("H:   slot %d - matches\n", index);
	    cache_hit++;
	    return index;
	nope:;
	}
	if (VH) printf("H:   slot %d - no match\n", index);

	/* find index of next slot */
	index= (index + offset) % hash->nslots;
    }
    return -1;
}


/* LINE_CACHE - Check if there is a cached solution for the given line.  If
 * so return a solution for it as an array of bitstrings.  If not, return
 * NULL.
 */

bit_type *line_cache(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    int index;
    HashElem *e;
    line_t this_clid= clid[k][i];
    line_t ncell= puz->clue[k][i].linelen;

    if (VH) printf("H: checking cache for %s %i\n", cluename(puz->type,k),i);

    cache[k]->lastslot= -1;

    /* Get the line length if it wasn't passed to us */

    /* Compress the current state of the line */
    if (this_clid > 0)
	compress_line(puz, sol, k, i, ncell, tmp);
    else
	rev_compress_line(puz, sol, k, i, ncell, tmp);
    if (VH)
    {
	printf("H: clueid=%d compressed line: ",this_clid);
	dump_comp(tmp, ncell, puz->ncolor);
    }

    /* Look for a match in the hash table */
    index= hash_find(cache[k], abs(this_clid), tmp);

    if (index < 0)
      	/* Table is full - should never happen */
	return NULL;

    e= HashSlot(cache[k], index);
    if (clueid(e) == 0)
    {
       	/* No matching table entry found */
	cache[k]->lastslot= index;
	return NULL;
    }

    /* Uncompress the solution */
    if (this_clid > 0)
	uncompress_line(newstate(cache[k],e), ncell, puz->ncolor, col);
    else
	rev_uncompress_line(newstate(cache[k],e), ncell, puz->ncolor, col);
    if (VH)
    {
	printf("H: uncompressed solution:\n");
	dump_lro_solve(puz, k, i, col);
    }

    /* Return the solution */
    return col;
}


/* ADD_CACHE:  If a call to line_cache() fails, and we compute a solution
 * the hard way, then call this to add it to the cache.  It will be stored
 * in the empty cell where the most recent call to line_cache() stopped.
 */

void add_cache(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    HashElem *e;
    line_t ncell= puz->clue[k][i].linelen;

    if (VH) printf("H: adding %s %i solution to cache %d\n",
		cluename(puz->type,k),i,k);

    /* Empty the cache if it is too full */
    if (cache[k]->n > cache[k]->flushat)
    {
	if (VH) printf("H: Flushing cache %d\n",k);
	empty_hash(cache[k]);
	/* Redo the find so lastslot will point to the right place */
	hash_find(cache[k], clid[k][i], tmp);
    }
    
    /* If no previous search, silently do nothing */
    if (cache[k]->lastslot < 0) return;
    cache_add++;


    e= HashSlot(cache[k], cache[k]->lastslot);
    clueid(e)= abs(clid[k][i]);
    memmove(oldstate(e), tmp, cache[k]->len * sizeof(bit_type));
    if (clid[k][i] > 0)
	compress_line(puz, sol, k, i, ncell, newstate(cache[k],e));
    else
	rev_compress_line(puz, sol, k, i, ncell, newstate(cache[k],e));
    if (VH)
    {
	printf("H: added in slot %d:\n", cache[k]->lastslot);
	printf("   clue id:  %d\n", clueid(e));
	printf("   old state:  ");
	dump_comp(oldstate(e), ncell, puz->ncolor);
	printf("   new state:  ");
	dump_comp(newstate(cache[k],e), ncell, puz->ncolor);
    }
    cache[k]->n++;
}


/* COMPRESS_LINE: Take a line of the current solution and compress it into
 * a single bit string.  The string will contain <ncell>*<ncolors> bits,
 * each bit being one if that cell can be that color.  "out" must point to a
 * preallocated buffer big enough to store the compressed line.
 */

void compress_line(Puzzle *puz, Solution *sol,
		   dir_t k, line_t i, line_t ncell, bit_type *out)
{
    int j, z, m;
    Cell **cell= sol->line[k][i];
    int bi= 0;            /* Currently storing into out[i] */
    int bn= _bit_intsiz;  /* First free bit in out[i] */

    out[bi]= 0;

#ifdef LIMITCOLORS
    z= 0;
#endif

    for (j= 0; j < ncell; j++)
    {
#ifdef LIMITCOLORS
	m= puz->ncolor;
#else
	for (z= 0; z < fbit_size; i++)
	{
	    /* number of bits we want from cell[j]->bit[z] */
	    m= (z == fbit_size-1) ? puz->ncolor % _bit_intsize : _bit_intsize;

#endif
	    if (m > bn)
	    {
		out[bi]|= cell[j]->bit[z] >> (m - bn);
		m-= bn;
		out[++bi]= 0;
		bn= _bit_intsiz;
	    }
	    if (m > 0 && m <= bn)
	    {
		out[bi]|= cell[j]->bit[z] << (bn - m);
		bn-= m;
		if (bn == 0)
		{
		    out[++bi]= 0;
		    bn= _bit_intsiz;
		}
	    }
#ifndef LIMITCOLORS
	}
#endif
    }
}


/* REV_COMPRESS_LINE: This is the same as compress_line, except we reverse
 * the order of the cells (but not the bit strings that make up the cells).
 * This differs from rev_compress_line by only one line of code, but having
 * two versions gets us more speed.
 */

void rev_compress_line(Puzzle *puz, Solution *sol,
		   dir_t k, line_t i, line_t ncell, bit_type *out)
{
    int j, z, m;
    Cell **cell= sol->line[k][i];
    int bi= 0;            /* Currently storing into out[i] */
    int bn= _bit_intsiz;  /* First free bit in out[i] */

    out[bi]= 0;

#ifdef LIMITCOLORS
    z= 0;
#endif

    for (j= ncell-1; j >= 0; j--)
    {
#ifdef LIMITCOLORS
	m= puz->ncolor;
#else
	for (z= 0; z < fbit_size; i++)
	{
	    /* number of bits we want from cell[j]->bit[z] */
	    m= (z == fbit_size-1) ? puz->ncolor % _bit_intsize : _bit_intsize;

#endif
	    if (m > bn)
	    {
		out[bi]|= cell[j]->bit[z] >> (m - bn);
		m-= bn;
		out[++bi]= 0;
		bn= _bit_intsiz;
	    }
	    if (m > 0 && m <= bn)
	    {
		out[bi]|= cell[j]->bit[z] << (bn - m);
		bn-= m;
		if (bn == 0)
		{
		    out[++bi]= 0;
		    bn= _bit_intsiz;
		}
	    }
#ifndef LIMITCOLORS
	}
#endif
    }
}


void uncompress_line(bit_type *in, int ncell, int ncolor, bit_type *out)
{
    int i,z;
    bit_type *b;
    int bi= 0;
    int bn= _bit_intsiz;

    for (i= 0; i < ncell; i++)
    {
	b= colbit(i);
	for (z= 0; z < fbit_size - 1; z++)
	{
	    if (bn == _bit_intsiz)
	    {
		b[z]= in[bi];
		bi++;
	    }
	    else
	    {
		b[z]= in[bi] << (_bit_intsiz - bn);
		bi++;
		b[z]|= in[bi] >> bn;
	    }
	}
	if (ncolor <= bn)
	{
	    b[z]= (in[bi] >> (bn - ncolor)) & bit_zeroone(ncolor);
	    bn-= ncolor;
	}
	else
	{
	    b[z]= ((in[bi] << (ncolor - bn)) |
		   (in[++bi] >> (_bit_intsiz - ncolor + bn))
		   & bit_zeroone(ncolor));
	    bn+= _bit_intsiz - ncolor;
	}
    }
}


void rev_uncompress_line(bit_type *in, int ncell, int ncolor, bit_type *out)
{
    int i,z;
    bit_type *b;
    int bi= 0;
    int bn= _bit_intsiz;

    for (i= ncell-1; i >= 0; i--)
    {
	b= colbit(i);
	for (z= 0; z < fbit_size - 1; z++)
	{
	    if (bn == _bit_intsiz)
	    {
		b[z]= in[bi];
		bi++;
	    }
	    else
	    {
		b[z]= in[bi] << (_bit_intsiz - bn);
		bi++;
		b[z]|= in[bi] >> bn;
	    }
	}
	if (ncolor <= bn)
	{
	    b[z]= (in[bi] >> (bn - ncolor)) & bit_zeroone(ncolor);
	    bn-= ncolor;
	}
	else
	{
	    b[z]= ((in[bi] << (ncolor - bn)) |
		   (in[++bi] >> (_bit_intsiz - ncolor + bn))
		   & bit_zeroone(ncolor));
	    bn+= _bit_intsiz - ncolor;
	}
    }
}


void dump_comp(bit_type *c, int ncell, int ncolor)
{
    int i, j;
    int bi= 0, bn= _bit_intsiz;

    for (i= 0; i < ncell; i++)
    {
	putchar('(');
	for (j= 0; j < ncolor; j++)
	{
	    if (bn-- == 0) {bi++; bn= _bit_intsiz;}
	    putchar(bit_test(c+bi,bn) ? '1':'0');
	}
	putchar(')');
    }
    putchar('\n');
}
