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

#include "pbnsolve.h"


/* Remove all jobs from the queue */

void flush_jobs(Puzzle *puz)
{
    int i;
    Job *j;

    /* Remember the job array is one based */
    for (i= 1; i <= puz->njob; i++)
    {
	j= &puz->job[i];
    	puz->clue[j->dir][j->n].jobindex= -1;
    }

    puz->njob= 0;
}


/* Move job j into position i.
 */

void job_move(Puzzle *puz, int i, int j)
{
    Job *src= &puz->job[j];
    Job *dst= &puz->job[i];

    puz->clue[src->dir][src->n].jobindex= i;
    dst->priority= src->priority;
    dst->dir= src->dir;
    dst->n= src->n;
}


/* Swap the position of two jobs in the heap.  This is a helper for various
 * heap maintenance functions.
 */

void job_exchange(Puzzle *puz, int i, int j)
{
    Job *ji= &puz->job[i];
    Job *jj= &puz->job[j];
    int idir= ji->dir;
    int in= ji->n;
    int jdir= jj->dir;
    int jn= jj->n;
    int tmp;

    /* Swap pointers from clue array */
    puz->clue[idir][in].jobindex= j;
    puz->clue[jdir][jn].jobindex= i;

    /* Swap priorities */
    tmp= ji->priority;
    ji->priority= jj->priority;
    jj->priority= tmp;

    /* Swap line pointers */
    ji->dir= jdir;
    ji->n= jn;
    jj->dir= idir;
    jj->n= in;
}

void heapify_jobs(Puzzle *puz, int i)
{
    int left= 2*i;
    int right= left+1;
    int largest;

    largest= (left <= puz->njob &&
        puz->job[left].priority > puz->job[i].priority) ? left : i;
    if (right <= puz->njob &&
        puz->job[right].priority > puz->job[largest].priority)
	largest= right;
    if (largest != i)
    {
    	job_exchange(puz, i, largest);
	heapify_jobs(puz, largest);
    }
}


/* Add a job onto the job list */

void add_job(Puzzle *puz, int k, int i)
{
    Job *job;
    int priority;
    int j, par;

    /* Check if it is already on the job list */
    if ((j= puz->clue[k][i].jobindex) >= 0)
    {
	if (VJ) printf(" J: JOB ON %s %d ALREADY ON JOBLIST\n",
		CLUENAME(puz->type,k),i);

	/* Increase the priority of the job, bubbling it up the heap if
	 * necessary
	 */

	puz->job[j].priority+= 4;

	while (j > 1 && puz->job[par= j/2].priority < puz->job[j].priority)
	{
	    job_exchange(puz, j, par);
	    j= par;
	}

    	return;
    }

    /* Give higher priority to things on the edge */
    priority= abs(puz->n[k]/2 - i);


    if (VJ) printf(" J: JOB ON %s %d ADDED TO JOBLIST\n",
		CLUENAME(puz->type,k),i);

    /* Bubble things down until we find the spot to insert the new key */
    j= ++puz->njob;
    while (j > 1 && puz->job[par= j/2].priority < priority)
    {
    	job_move(puz, j, par);
	j= par;
    }

    /* Insert the new key */
    puz->clue[k][i].jobindex= j;
    job= &puz->job[j];
    job->priority= priority;
    job->dir= k;
    job->n= i;
}


/* Add jobs for all lines that cross the given cell */
void add_jobs(Puzzle *puz, Cell *cell)
{
    int k;

    if (maylinesolve)
	for (k= 0; k < puz->nset; k++)
            add_job(puz, k, cell->line[k]);
}


/* Get the next job off the job list (removing it from the list).
 * Return 0 if there is no next job.
 */

int next_job(Puzzle *puz, int *k, int *i)
{
    Job *first;

    if (puz->njob < 1)
    	return 0;

    /* Remove the top job from the heap */
    first= &puz->job[1];
    *k= first->dir;
    *i= first->n;
    puz->clue[*k][*i].jobindex= -1;

    if (--puz->njob == 0) return 1;	/* Heap is now empty */

    /* Copy the bottom job into the deleted job's place */
    job_move(puz, 1, puz->njob + 1);

    /* Reheap */
    heapify_jobs(puz, 1);

    return 1;
}



/* Put every row and column on the job list.
 */

void init_jobs(Puzzle *puz, Solution *sol)
{
    int i, j, k, d;

    /* Delete any previously existing heap */
    if (puz->job != NULL) free(puz->job);

    /* Find total number of lines in the puzzle */
    puz->sjob= 0;
    for (k= 0; k < puz->nset; k++)
    	puz->sjob+= puz->n[k];

    /* Allocate a new heap.  We make it one too big, because we are going
     * to use one-based indices into this to make my life easier.
     */
    puz->job= (Job *)malloc((puz->sjob + 1) * sizeof(Job));

    /* Add all lines in arbitrary order to heap array.  These initial
     * jobs have a high priority, as we would like to process each input
     * line once before processing any for the second time.
     */
    j= 1;
    for (k= 0; k < puz->nset; k++)
    {
	for (i= 0; i < puz->n[k]; i++)
	{
	    d= puz->n[k] - d - 1;
	    if (d > i) d= i;
	    if (puz->clue[k][i].n == 0)
		puz->job[j].priority= 2000;	/* Blank line */
	    else
		puz->job[j].priority= 1000 - d + 2*count_paint(puz,sol,k,i);
	    puz->job[j].dir= k;
	    puz->job[j].n= i;
	    puz->clue[k][i].jobindex= j;
	    j++;
	}
    }

    /* Build that data into a heap */
    puz->njob= puz->sjob;
    for (i= puz->njob/2; i > 0; i--)
    	heapify_jobs(puz,i);
}


/* Add a cell to the history.  This should be called while the cell still
 * contains it's old values.  Branch is true if this is a branch point, that
 * is, not a consequence of what has gone before, but a random guess that might
 * be wrong.
 */

void add_hist(Puzzle *puz, Cell *cell, int branch)
{
    add_hist2(puz,cell,cell->n,cell->bit,branch);
}

void add_hist2(Puzzle *puz, Cell *cell, int oldn, bit_type *oldbit, int branch)
{
    Hist *h;
    int z, nc;

    /* We only start keeping a history after the first branch point */
    if (puz->history == NULL && !branch) return;

    /* Allocate memory for the new history element, enlarging it enough
     * to hold bitstrings of the size we need.
     */
    nc= bit_size(puz->ncolor);
    h= (Hist *)malloc(sizeof(Hist) + nc - bit_size(1));

    /* Add it onto the linked list */
    h->prev= puz->history;
    puz->history= h;

    h->branch= branch;
    h->cell= cell;
    h->n= oldn;

    for (z= 0; z < nc; z++)
    	h->bit[z]= oldbit[z];
}


/* Backtrack up the history, undoing changes we made.  Stop at the first
 * branch point. If leave_branch is true, do not remove the branch point from
 * the history, otherwise, undo and remove that too.
 */

int undo(Puzzle *puz, Solution *sol, int leave_branch)
{
    Hist *h;
    int nc= bit_size(puz->ncolor);
    int z, k, oldn;
    int is_branch;

    while ((h= puz->history) != NULL)
    {
	is_branch= h->branch;

	if (!is_branch || !leave_branch)
	{
	    /* If undoing a solved cell, decrement completion count */
	    if (h->cell->n == 1) puz->nsolved--;

	    /* Restore saved value */
	    h->cell->n= h->n;
	    for (z= 0; z < nc; z++)
	    	h->cell->bit[z]= h->bit[z];

	    if (VU)
	    {
	    	printf("U: UNDOING CELL ");
		for (k= 0; k < puz->nset; k++)
		    printf(" %d",h->cell->line[k]);
		printf(" TO ");
		dump_bits(stdout,puz,h->cell->bit);
		printf(" (%d)\n",h->cell->n);
	    }

	    /* Remove cell from linked list */
	    puz->history= h->prev;
	    free(h);
	}

	if (is_branch)
	    return 0;
    }
    return 1;
}

/* Backtrack up the history, undoing changes we made.  Stop at the first
 * branch point, set that cell to the contradiction of the previous change.
 * Returns 0 if we successfully backtracked.  Returns 1 if there was no
 * previous branch point.
 */

int backtrack(Puzzle *puz, Solution *sol)
{
    Hist *h;
    int nc= bit_size(puz->ncolor);
    int z, k, oldn;

    if (VB) printf("B: BACKTRACKING TO LAST GUESS\n");

    /* Undo up to, but not including, the most recent branch point */
    if (undo(puz,sol, 1))
    {
	if (VB) printf("B: CANNOT BACKTRACK\n");
	return 1;
    }

    if (VB) print_solution(stdout,puz,sol);

    /* This will be the branch point */
    h= puz->history;

    /* Reset any bits previously set */
    for (z= 0; z < nc; z++)
	h->cell->bit[z]= ((~h->cell->bit[z]) & h->bit[z]);

    /* Count the number of colors left */
    oldn= h->cell->n;
    if (puz->ncolor <= 2)
	h->cell->n= 1;
    else
	count_cell(puz,h->cell);
    if (oldn == 1 && h->cell->n > 1) puz->nsolved--;

    if (VB)
    {
	printf("B: INVERTING BRANCH CELL ");
	for (k= 0; k < puz->nset; k++)
	    printf(" %d",h->cell->line[k]);
	printf(" TO ");
	dump_bits(stdout,puz,h->cell->bit);
	printf(" (%d)\n",h->cell->n);
    }

    /* Now that we've backtracked to it and inverted it, it is no
     * longer a branch point.  If there is no previous history, delete
     * this node.  Otherwise, convert it into a non-branch point.
     * Next time we backtrack we will just delete it.
     */
    if (h->prev == NULL)
    {
        puz->history= NULL;
	free(h);
    }
    else
	h->branch= 0;

    /* Remove everything from the job list except the lines containing
     * the inverted cell.
     */
    if (maylinesolve)
    {
	flush_jobs(puz);
	add_jobs(puz,h->cell);
    }

    backtracks++;

    return 0;
}
