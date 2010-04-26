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

#ifdef LINEWATCH
#define WL(clue) (clue).watch
#define WC(cell) (puz->clue[0][(cell)->line[0]].watch || puz->clue[1][(cell)->line[1]].watch)
#else
#define WL(clue) 0
#define WC(cell) 0
#endif

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

    /* Swap depths */
    tmp= ji->depth;
    ji->depth= jj->depth;
    jj->depth= tmp;

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

void add_job(Puzzle *puz, dir_t k, line_t i, int depth, int bonus)
{
    Job *job;
    int priority;
    int j, par;

    /* Check if it is already on the job list */
    if ((j= puz->clue[k][i].jobindex) >= 0)
    {
	if (VJ || WL(puz->clue[k][i]))
		printf(" J: JOB ON %s %d ALREADY ON JOBLIST\n",
		    CLUENAME(puz->type,k),i);

	/* Increase the priority of the job, bubbling it up the heap if
	 * necessary
	 */

	puz->job[j].priority+= 4 + 25*bonus;
	if (puz->job[j].depth > depth) puz->job[j].depth= depth;

	while (j > 1 && puz->job[par= j/2].priority < puz->job[j].priority)
	{
	    job_exchange(puz, j, par);
	    j= par;
	}

    	return;
    }

    /* Give higher priority to things on the edge */
    priority= abs(puz->n[k]/2 - i) + 25*bonus - 10*depth;


    if (VJ || WL(puz->clue[k][i]))
    	printf(" J: JOB ON %s %d ADDED TO JOBLIST DEPTH %d PRIORITY %d\n",
	    CLUENAME(puz->type,k),i,depth,priority);

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
    job->depth= depth;
    job->dir= k;
    job->n= i;
}


/* Add jobs for all lines that cross the given cell.  Don't add direction
 * 'except'.  The cell should already have been updated with it's new value,
 * and 'old' should give it's old value.  This is also responsible for
 * checking if the newly set cell invalidates any cached solutions in for
 * the lines being put back on the job queue.
 */

void add_jobs(Puzzle *puz, Solution *sol, int except, Cell *cell,
	int depth, bit_type *old)
{
    dir_t k;
    line_t i, j;
    int lwork, rwork;

    /* While probing, we OR all bits set into our scratchpad.  These values
     * should not be probed on later during this sequence.
     */
    if (probing)
    	fbit_or(propad(cell),cell->bit);

    if (!maylinesolve) return;

    for (k= 0; k < puz->nset; k++)
	if (k != except)
	{
	    i= cell->line[k];
	    j= cell->index[k];

	    /* We only add the job only if either the saved left or right
	     * solution for the line has been invalidated.
	     */
	    if (VL || WL(puz->clue[k][i]))
		printf ("L: CHECK OLD SOLN FOR %s %d CELL %d\n",
	    	CLUENAME(puz->type,k),i,j);
	    lwork= left_check(&puz->clue[k][i], j, cell->bit);
	    rwork= right_check(&puz->clue[k][i], j, cell->bit);
	    if (lwork || rwork)
	    {
		add_job(puz, k, i, depth,
		    newedge(puz, sol->line[k][i], j, old, cell->bit) );

		if (!VJ && WL(puz->clue[k][i]))
		    dump_jobs(stdout,puz);
	    }
	}
}


/* Get the next job off the job list (removing it from the list).
 * Return 0 if there is no next job.
 */

int next_job(Puzzle *puz, dir_t *k, line_t *i, int *d)
{
    Job *first;

    if (puz->njob < 1)
    	return 0;

    /* Remove the top job from the heap */
    first= &puz->job[1];
    *k= first->dir;
    *i= first->n;
    *d= first->depth;
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
    dir_t k;
    line_t i, j, d;

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
	    d= puz->n[k] - i - 1;
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


/* ENLARGE_HIST - Expand the history array.  Puzzle history can't be longer
 * then ncells*(ncolors-1) but we don't want to allocate that much memory if
 * we can avoid it.  So we don't allocate anything until the first guess.
 * Then we allocate a history element for each remaining cell, adding 50% if
 * we are multicolor and never allocating less than 40.  If we run out, we
 * call this again to further enlarge the array.
 */
void enlarge_hist(Puzzle *puz)
{
   int inc= puz->ncells - puz->nsolved + 1;
   if (puz->ncolor > 2) inc*= 1.5;
   if (inc < 40) inc= 40;
   puz->shist+= inc;
   puz->history= (Hist *)realloc(puz->history, HISTSIZE(puz)*puz->shist);
}


/* Add a cell to the history.  This should be called while the cell still
 * contains it's old values.  Branch is true if this is a branch point, that
 * is, not a consequence of what has gone before, but a random guess that might
 * be wrong.
 */

Hist *add_hist(Puzzle *puz, Cell *cell, int branch)
{
    return add_hist2(puz,cell,cell->n,cell->bit,branch);
}


Hist *add_hist2(Puzzle *puz, Cell *cell, color_t oldn, bit_type *oldbit, int branch)
{
    Hist *h;
    color_t z;

    /* We only start keeping a history after the first branch point */
    if (puz->nhist == 0 && !branch) return NULL;

    /* Make sure we have memory for the new history element */
    if (puz->nhist >= puz->shist) enlarge_hist(puz);

    /* Get the new history element */
    h= HIST(puz, puz->nhist++);

    h->branch= branch;
    h->cell= cell;
    h->n= oldn;

    fbit_cpy(h->bit, oldbit);

    return h;
}


/* Backtrack up the history, undoing changes we made.  Stop at the first
 * branch point. If leave_branch is true, do not remove the branch point from
 * the history, otherwise, undo and remove that too.
 */

int undo(Puzzle *puz, Solution *sol, int leave_branch)
{
    Hist *h;
    Clue *clue;
    Cell **line;
    dir_t k;
    int is_branch;
    line_t i;

    while (puz->nhist > 0)
    {
	h= HIST(puz, puz->nhist-1);

	/* Invalidate any saved positions for lines crossing undone cell.
	 * We can't just have the fact that nhist < stamp mean the line is
	 * invalid, because we might backtrack and then advance past stamp
	 * again before we recheck the line.
	 */
	for (k= 0; k < puz->nset; k++)
	{

	    i= h->cell->line[k];
	    clue= &(puz->clue[k][i]);
	    line= sol->line[k][i];

	    if ((VL && VU) || WL(*clue))
		printf("U: CHECK %s %d", CLUENAME(puz->type,k),i);

	    left_undo(puz, clue, line, h->cell->index[k], h->bit);
	    right_undo(puz, clue, line,  h->cell->index[k], h->bit);

	    if ((VL && VU) || WL(*clue)) printf("\n");
	}

	is_branch= h->branch;

	if (!is_branch || !leave_branch)
	{
	    /* If undoing a solved cell, decrement completion count */
	    if (h->cell->n == 1) solved_a_cell(puz, h->cell, -1);

	    /* Restore saved value */
	    h->cell->n= h->n;
	    fbit_cpy(h->cell->bit, h->bit);

	    if (VU || WC(h->cell))
	    {
	    	printf("U: UNDOING CELL ");
		for (k= 0; k < puz->nset; k++)
		    printf(" %d",h->cell->line[k]);
		printf(" TO ");
		dump_bits(stdout,puz,h->cell->bit);
		printf(" (%d)\n",h->cell->n);
	    }

	    puz->nhist--;
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
    color_t z, oldn, newn;
    dir_t k;

    if (VB) printf("B: BACKTRACKING TO LAST GUESS\n");

    /* Undo up to, but not including, the most recent branch point */
    if (undo(puz,sol, 1))
    {
	if (VB) printf("B: CANNOT BACKTRACK\n");
	return 1;
    }

    if (VB) print_solution(stdout,puz,sol);

    /* This will be the branch point since undo() backed us up to it */
    h= HIST(puz, puz->nhist-1);

    if (VB || WC(h->cell))
    {
	printf("B: LAST GUESS WAS ");
	print_coord(stdout,puz,h->cell);
	printf(" |");
	dump_bits(stdout,puz,h->bit);
	printf("| -> |");
	dump_bits(stdout,puz,h->cell->bit);
	printf("|\n");
    }

    /* If undoing a solved cell, uncount it */
    if (h->cell->n == 1) solved_a_cell(puz, h->cell, -1);

    /* Reset any bits previously set */
#ifdef LIMITCOLORS
    h->cell->bit[0]= ((~h->cell->bit[0]) & h->bit[0]);
#else
    for (z= 0; z < fbit_size; z++)
	h->cell->bit[z]= ((~h->cell->bit[z]) & h->bit[z]);
#endif
    h->cell->n= h->n - h->cell->n;  /* Since the bits set in h are always
				       a superset of those in h->cell,
				       this should always work */

    /* If inverted cell is solved, count it */
    if (h->cell->n == 1) solved_a_cell(puz, h->cell, 1);

    if (VB || WC(h->cell))
    {
	printf("B: INVERTING GUESS TO |");
	dump_bits(stdout,puz,h->cell->bit);
	printf("| (%d)\n",h->cell->n);
    }

    /* Now that we've backtracked to it and inverted it, it is no
     * longer a branch point.  If there is no previous history, delete
     * this node.  Otherwise, convert it into a non-branch point.
     * Next time we backtrack we will just delete it.
     */
    if (puz->nhist == 1)
	puz->nhist= 0;
    else
	h->branch= 0;

    /* Remove everything from the job list except the lines containing
     * the inverted cell.
     */
    if (maylinesolve)
    {
	flush_jobs(puz);
	add_jobs(puz, sol, -1, h->cell, 0, h->bit);
    }

    backtracks++;

    return 0;
}


/* If the ith cell in the given line has transitioned from old to new, has
 * it made an edge in that direction, in other words, if it used to have some
 * colors in common with one of it's two neighbor cells, but doesn't any more.
 */

#define noedge(a,b) (~((a) ^ (b)) & ((a) | (b)))
int newedge(Puzzle *puz, Cell **line, line_t i, bit_type *old, bit_type *new)
{
    color_t z;
    bit_type *n1, *n2;
    static bit_type one=1, zero= 0;

    for (z= 0; z < fbit_size; z++)
    {
    	n1= (i > 0) ? &(line[i-1]->bit[z]) :
	    ((z == 0) ? &one : &zero);
    	n2= (line[i+1] != NULL) ? &(line[i+1]->bit[z]) : 
	    ((z == 0) ? &one : &zero);
	if ((!noedge(*n1,new[z]) && noedge(*n1,old[z])) ||
	    (!noedge(*n2,new[z]) && noedge(*n2,old[z])) )
		return 1;
    }
    return 0;
}
