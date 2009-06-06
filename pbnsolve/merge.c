/* Copyright 2008 Jan Wolter
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

#include "pbnsolve.h"

int merging= 0;		/* Are we currently merging? */
int merge_no= -1;	/* Guess count.  If 0 we are on first guess for cell */
MergeElem *merge_list= NULL; /* List of consequences of all guesses so far */


/* MERGE_GUESS() - Called when we make a new guess on a cell, including
 * before the first guess.  Updates our guess count.  Any cells in the list
 * that had a lower guess count are dropped from the list.
 */

void merge_guess()
{
    MergeElem *m, *n, *p= NULL;
    int ndrop= 0, nleft= 0;

    if (!mergeprobe) return;

    for (m= merge_list; m != NULL; m= n)
    {
	n= m->next;
	if (m->maxc < merge_no)
	{
	    if (p)
	       p->next= n;
	    else
	       merge_list= n;
	    free(m);
	    if (VM) ndrop++;
	}
	else
	{
	    p= m;
	    if (VM) nleft++;
	}
    }
    merge_no++;
    merging= 1;

    if (VM) printf("M: MERGE PASS %d - %d dropped, %d left\n",
		    merge_no,ndrop,nleft);
}

/* MERGE_SET() - Each time we change a cell's list of possible color due as
 * a consequence of a probing guess, we call this.  It should be called before
 * the new values is stored in the cell.  We pass in a bitstring which has
 * a zero for every color that has been eliminated.
 */

void merge_set(Puzzle *puz, Cell *cell, bit_type *bit)
{
    MergeElem *m, *p;
    int z, zero;

    if (!merging) return;

    /* Scan existing merge list, to see if we have an entry for this cell */
    for (m= merge_list, p= NULL;
    	 m != NULL && m->cell != cell;
	 p=m, m= m->next)
	 { }

    if (m == NULL)
    {
	/* If this is not the first guess, and the cell is not already on the
	 * list, do nothing.  */
    	if (merge_no > 0) return;

	/* Otherwise, make a new merge list entry */
	m= (MergeElem *)malloc(sizeof(MergeElem) + puz->colsize - bit_size(1));
	m->cell= cell;
	m->maxc= merge_no;
	for (z= 0; z < puz->colsize; z++)
	    m->bit[z]= cell->bit[z] & ~bit[z];
	m->next= merge_list;
	merge_list= m;
	if (VM)
	{
	    printf("M: NEW MERGE CELL (%d,%d) BITS ",
		m->cell->line[0], m->cell->line[1]);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}
	return;
    }

    /* If the cell is on the list, intersect the changes */
    zero= 1;
    for (z= 0; z < puz->colsize; z++)
    {
    	m->bit[z]&= cell->bit[z] & ~bit[z];
	if (m->bit[z]) zero= 0;

    }
    if (zero)
    {
    	/* No intersection - delete the node */
	if (VM) printf("M: DROPPING MERGE CELL (%d,%d)\n",
	    m->cell->line[0], m->cell->line[1]);

	if (p)
	    p->next= m->next;
	else
	    merge_list= m->next;
	free(m);
    }
    else
    {
    	/* Had some intersection - keep the node */
	m->maxc= merge_no;

	if (VM)
	{
	    printf("M: UPDATING MERGE CELL (%d,%d) BITS ",
		m->cell->line[0], m->cell->line[1]);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}
    }
}

/* MERGE_CHECK - after completing the last probe on a cell, calling this
 * checks to see if there is anything on the merge list left.  If there is,
 * those settings must be necessary.  Returns true if anything was found.
 * This always consumes the merge_list and leaves things properly initialized
 * for probing on a new cell.
 */

int merge_check(Puzzle *puz)
{
    MergeElem *m, *n;
    int z, d;
    int found= 0;

    if (!merging) return 0;

    for (m= merge_list; m != NULL; m= n)
    {
	n= m->next;
	if (m->maxc == merge_no)
	{
	    if (VM)
	    {
	    	printf("M: FOUND MERGED CONSEQUENCE ON CELL (%d,%d) BITS ",
		    m->cell->line[0], m->cell->line[1]);
		dump_bits(stdout, puz, m->bit);
		printf("\n");
	    }

	    /* Add to history as a necessary consequence */
	    add_hist(puz, m->cell, 0);

	    /* Set the new value in the cell */
	    for (z= 0; z < puz->colsize; z++)
	        m->cell->bit[z]&= ~m->bit[z];
	    if (puz->ncolor <= 2)
	        m->cell->n= 1;
	    else
	        count_cell(puz,m->cell);

	    if (m->cell->n == 1) puz->nsolved++;

            /* Add rows/columns containing this cell to the job list */
	    add_jobs(puz, m->cell,0);

	    found= 1;
	}
	free(m);
    }
    if (VM && !found) printf("M: NO MERGE CONSEQUENCES\n");

    merge_list= NULL;
    merge_no= -1;
    merging= 0;

    return found;
}
