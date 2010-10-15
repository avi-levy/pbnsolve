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
MergeElem *mergegrid;	/* Grid of merge cells */

extern bit_type *oldval;


/* INIT_MERGE - Allocate merge array */

void init_merge(Puzzle *puz)
{
    mergegrid= (MergeElem *)calloc(puz->ncells, sizeof(MergeElem));
}


void dump_merge(Puzzle *puz)
{
    MergeElem *m;
    int n= 0;

    for (m= merge_list; m != NULL; m= m->next)
    {
	if (n++ > 1000) {printf("Yicks!\n"); exit(1);}
	if (m->cell)
	{
	    printf("  %d: CELL (%d,%d) BITS ",n,
		m->cell->line[0], m->cell->line[1]);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}
	else
	    printf("  %d: NULL\n",n);
    }
}


/* MERGE_CANCEL() - Don't do merging for this pass after all.  We call this
 * when we skip some probes on a cell, since merging isn't valid then.
 */

void merge_cancel()
{
    MergeElem *m;

    if (VM) printf("M: MERGING CANCELED\n");

    for (m= merge_list; m != NULL; m= m->next)
	m->cell= NULL;

    merge_list= NULL;
    merge_no= -1;
    merging= 0;
}


/* MERGE_GUESS() - Called when we make a new guess on a cell, including
 * before the first guess.  Updates our guess count.  Any cells in the list
 * that had a lower guess count are dropped from the list.
 */

void merge_guess()
{
    MergeElem *m, *p= NULL;
    int ndrop= 0, nleft= 0;

    for (m= merge_list; m != NULL; m= m->next)
    {
	if (m->cell == NULL || m->maxc < merge_no)
	{
	    if (p)
	       p->next= m->next;
	    else
	       merge_list= m->next;
	    m->cell= NULL;	/* Mark the cell unused */
	    if (VM) ndrop++;
	}
	else
	{
	    p= m;
	    if (VM) nleft++;
	}
    }
    merge_no++;

    if (VM) printf("M: MERGE PASS %d - %d dropped, %d left\n",
		    merge_no,ndrop,nleft);
}


/* MERGE_SET() - Each time we change a cell's list of possible color as
 * a consequence of a probing guess, we call this.  It should be called before
 * the new values is stored in the cell.  We pass in a bitstring which has
 * a zero for every color that has been eliminated.
 */

void merge_set(Puzzle *puz, Cell *cell, bit_type *bit)
{
    MergeElem *m, *p;
    color_t z;
    int zero;

    /* Get the merge element for this cell */
    m= &mergegrid[cell->id];
    
    if (m->cell == NULL)
    {
	/* If this is not the first guess, and the cell is not already on the
	 * list, do nothing.  */
    	if (merge_no > 0) return;

	/* Otherwise, make a new merge list entry */
	m->cell= cell;
	m->maxc= merge_no;
#ifdef LIMITCOLORS
	m->bit[0]= cell->bit[0] & ~bit[0];
#else
	for (z= 0; z < fbit_size; z++)
	    m->bit[z]= cell->bit[z] & ~bit[z];
#endif
	m->next= merge_list;
	merge_list= m;
	if (VM)
	{
	    printf("M: NEW MERGE CELL (%d,%d) L%d BITS ",
		m->cell->line[0], m->cell->line[1], merge_no);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}
	return;
    }

    if (m->maxc == merge_no)
    {
    	/* If cell has already been updated on this pass, and this is not
	 * pass zero, then we can't record the change correctly, so we don't
	 * record them.  We might miss some merges this way, but this is a
	 * really rare occurance that happens only on really successful probes,
	 * which are likely to be our next guess anyway.
	 */
	if (merge_no == 0)
	    /* If this is pass zero, the we can OR the changes in because
	     * we haven't ANDed it with anything else yet */
	    m->bit[0]|= cell->bit[0] & ~bit[0];

	if (VM)
	{
	    printf("M: REVISITING MERGE CELL (%d,%d) L%d BITS ",
		m->cell->line[0], m->cell->line[1], merge_no);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}

	return;
    }

    /* If the cell is on the list from previous probe, intersect the changes */
#ifdef LIMITCOLORS
    m->bit[0]&= cell->bit[0] & ~bit[0];
    if (m->bit[0] == 0)
#else
    zero= 1;
    for (z= 0; z < fbit_size; z++)
    {
    	m->bit[z]&= cell->bit[z] & ~bit[z];
	if (m->bit[z]) zero= 0;

    }
    if (zero)
#endif
    {
    	/* No intersection - mark the node for deletion. Actual
	 * deletion from linked list happens during merge_guess. */

	if (VM) printf("M: DROPPING MERGE CELL (%d,%d) L%d\n",
	    m->cell->line[0], m->cell->line[1], merge_no);

	m->cell= NULL;
    }
    else
    {
    	/* Had some intersection - keep the node */
	m->maxc= merge_no;

	if (VM)
	{
	    printf("M: UPDATING MERGE CELL (%d,%d) L%d BITS ",
		m->cell->line[0], m->cell->line[1], merge_no);
	    dump_bits(stdout, puz, m->bit);
	    printf("\n");
	}
    }
}


/* MERGE_CHECK - after completing the last probe on a cell, calling this
 * checks to see if there is anything on the merge list left.  If there is,
 * those settings must be necessary.  Returns true if anything was found.
 * This always resets the merge_list to empty and leaves things properly
 * initialized for probing on a new cell.
 */

int merge_check(Puzzle *puz, Solution *sol)
{
    MergeElem *m;
    dir_t z;
    int found= 0;

    for (m= merge_list; m != NULL; m= m->next)
    {
	if (m->maxc == merge_no && m->cell != NULL)
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
#ifdef LIMITCOLORS
	    oldval[0]= m->cell->bit[0];
	    m->cell->bit[0]&= ~m->bit[0];
#else
	    for (z= 0; z < fbit_size; z++)
	    {
		oldval[z]= m->cell->bit[z];
	        m->cell->bit[z]&= ~m->bit[z];
	    }
#endif
	    if (puz->ncolor <= 2)
	        m->cell->n= 1;
	    else
	        count_cell(puz,m->cell);

	    if (m->cell->n == 1) solved_a_cell(puz, m->cell,1);

            /* Add rows/columns containing this cell to the job list */
	    add_jobs(puz, sol, -1, m->cell, 0, oldval);

	    found= 1;
	}
	/* Reset to unused state */
	m->cell= NULL;
    }
    if (VM && !found) printf("M: NO MERGE CONSEQUENCES\n");

    merge_list= NULL;
    merge_no= -1;
    merging= 0;

    return found;
}
