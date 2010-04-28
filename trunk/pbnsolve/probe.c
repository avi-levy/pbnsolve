/* Copyright 2009 Jan Wolter
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
#define WL(k,i) (puz->clue[k][i].watch)
#define WC(i,j) (puz->clue[0][i].watch || puz->clue[1][j].watch)
#else
#define WL(k,i) 0
#define WC(i,j) 0
#endif

long nprobe= 0;

/* Some data structures used purely for collecting and reporting probing
 * statistics */

/* Sources of probe cells */
#define PRBSRC_ADJACENT 0
#define PRBSRC_TWONEIGH 1
#define PRBSRC_HEURISTIC 2
#define N_PRBSRC 3
static int probeon[N_PRBSRC]= {1,1,0};
static char *Probesource[N_PRBSRC]= {"ADJACENT", "TWO-NEIGHBOR", "HEURISTIC"};
static char *probesource[N_PRBSRC]= {"adj", "2-neigh", "heur"};
static long probesrc[N_PRBSRC]= {0,0,0};
static int currsrc;

/* Outcomes of probe sequences */
#define PRBRES_BEST 0
#define PRBRES_CONTRADICT 1
#define PRBRES_SOLVE 2
#define N_PRBRES 3
static long probeseq_res[N_PRBRES][N_PRBSRC]= {{0,0,0},{0,0,0},{0,0,0}};

/* SCRATCHPAD - An array of bitstrings for every cell.  Every color that is
 * set for a cell in the course of the current probe sequence is ORed into it.
 * Any setting which has been part of a previous probe will not be probed on,
 * because the consequences of that can only be a subset of the consequences
 * of the previous probe.
 */

bit_type *probepad= NULL;
int probing= 0;

/* Create or clear the probe pad */
void init_probepad(Puzzle *puz)
{
    if (!probepad)
	probepad= (bit_type *)
	    calloc(puz->ncells, fbit_size * sizeof(bit_type));
    else
    	memset(probepad, 0, puz->ncells * fbit_size * sizeof(bit_type));
}


/* PROBE_INIT - Warn that we are going to be probing for a while */

void probe_init(Puzzle *puz, Solution *sol)
{
    if (probeon[PRBSRC_HEURISTIC])
	bookkeeping_on(puz,sol);
    else
	bookkeeping_off();
}


/* PROBE_CELL - Do a sequence of probes on a cell.  We normally do one probe
 * on each possible color for the cell.  <cell> points and the cell, and <i>
 * and <j> are its coordinates.  <bestnleft> points to the nleft value of the
 * best guess found previously, and <bestc> points to the color of that guess.
 * The return code tells the result:
 *
 *   -2   A guess solved the puzzle.
 *   -1   A necessary consequence was found, either by contradiction or
 *        merging.  Either way, the cell has been set and we are ready to
 *        resume logic solving.
 *    0   No guess was found that was better than <bestnleft>
 *   >0   A guess was found that was better than <bestnleft>.  Both
 *        <bestnleft> and <bestc> have been updated with the new value.
 */

int probe_cell(Puzzle *puz, Solution *sol, Cell *cell, line_t i, line_t j,
	int *bestnleft, color_t *bestc)
{
    color_t c;
    int rc;
    int nleft;
    int foundbetter= 0;

    merging= mergeprobe;

    /* For each possible color of the cell */
    for (c= 0; c < puz->ncolor; c++)
    {
	if (may_be(cell, c))
	{
	    if (bit_test(propad(cell),c))
	    {
		/* We can skip this probe because it was a consequence
		 * of a previous probe.  However, if we do that, then
		 * we can't do merging on this cell.
		 */
		if (merging) merge_cancel();
	    }
	    else
	    {
		/* Found a candidate color - go probe on it */
		if (VP || VB || WC(i,j))
		    printf("P: PROBING (%d,%d) COLOR %d\n", i,j,c);
		probes++;
		probesrc[currsrc]++;

		if (merging) merge_guess();

		guess_cell(puz,sol,cell,c);
		rc= logic_solve(puz, sol, 0);

		if (rc == 0)
		{
		    /* Probe complete - save it's rating and undo it */
		    nleft= puz->ncells - puz->nsolved;
		    if (VQ || VP || WC(i,j))
			printf("P: PROBE #%d ON (%d,%d)%d COMPLETE "
			    "WITH %d CELLS LEFT (%s)\n",nprobe,
			    i,j,c,nleft, Probesource[currsrc]);
		    if (nleft < *bestnleft)
		    {
			*bestnleft= nleft;
			*bestc= c;
			foundbetter++;
		    }
		    if (VP)
			printf("P: UNDOING PROBE\n");

		    undo(puz, sol, 0);
		}
		else if (rc < 0)
		{
		    /* Found a contradiction - what luck! */
		    if (VP)
			printf("P: PROBE ON (%d,%d)%d "
				"HIT CONTRADICTION (%s)\n", i,j,c,
				Probesource[currsrc]);
		    
		    if (merging) merge_cancel();
		    guesses++;

		    /* Backtrack to the guess point, invert that */
		    if (backtrack(puz, sol))
		    {
			/* Nothing to backtrack to.  This should never
			 * happen, because we made a guess a few lines
			 * ago.
			 */
			printf("ERROR: "
			    "Could not backtrack after probe\n");
			exit(1);
		    }
		    if (VP)
		    {
			print_solution(stdout,puz,sol);
			dump_history(stdout, puz, VV);
		    }
		    probing= 0;
		    probeseq_res[PRBRES_CONTRADICT][currsrc]++;
		    return -1;
		}
		else
		{
		    /* by wild luck, we solved it */
		    if (merging) merge_cancel();
		    probing= 0;
		    probeseq_res[PRBRES_SOLVE][currsrc]++;
		    return -2;
		}
	    }
	}
    }

    /* Finished all probes on a cell.  Check if there is anything that
     * was a consequence of all alternatives.  If so, set that as a
     * fact, cancel probing and proceed.
     */

    if (merging && merge_check(puz, sol))
    {
	merges++;
	probing= 0;
	return -1;
    }
    return foundbetter;
}


/* List of additional candidate cells for probing.  Among the cells that
 * have less than two solved neighbors, we will collect the top rated ones
 * (based on the heuristic function) to also probe on.  N_GOOD is the maximum
 * number to collect.  ngood is the number currently collected.  Best is in
 * goodcell[0]
 */

#define N_GOOD 15
struct {
    line_t i, j;
    float score;
} goodcell[N_GOOD];
int ngood;


/* ADD_GOODCELL - Check if the given cell should be added to the goodcell
 * array. If so, insert it.
 */

void add_goodcell(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    float score;
    int a,b;
    
    /* If two score functions are defined, first is usually neighborlyness,
     * which we don't want, so use second in that case */
    if (cell_score_2 == NULL)
	score= (*cell_score_1)(puz,sol,i,j);
    else
	score= (*cell_score_2)(puz,sol,i,j);

    /* If array is full, and this no better than the worst in our collection
     * ignore it. */
    if (ngood == N_GOOD && score >= goodcell[N_GOOD-1].score) return;

    /* Find insertion point */
    for (a= 0; a < ngood; a++)
	if (score < goodcell[a].score) break;

    if (ngood < N_GOOD) ngood++;

    /* Shift down everything below the insertion point */
    for (b= ngood-1; b > a; b--)
	goodcell[b]= goodcell[b-1];

    /* Insert new value */
    goodcell[a].i= i;
    goodcell[a].j= j;
    goodcell[a].score= score;
}


/* Search energetically for the guess that lets us make the most progress
 * toward solving the puzzle, by trying lots of guesses and search on each
 * until it stalls.
 *
 * Normally it returns 0, with besti,bestj,bestc containing our favorite guess.
 *
 * If we accidentally solve the puzzle when we were just trying to probe,
 * return 1.
 *
 * If we discover a logically necessary cell, then we set it, add jobs to the
 * job list, and return -1.
 */

int probe(Puzzle *puz, Solution *sol,
    line_t *besti, line_t *bestj, color_t *bestc)
{
    line_t i, j, k;
    int bestsrc;
    color_t c;
    Cell *cell;
    int rc, neigh;
    int bestnleft= INT_MAX;
    line_t ci,cj;
    Hist *h;

    /* Starting a new probe sequence - initialize stuff */
    if (VP) printf("P: STARTING PROBE SEQUENCE\n");
    init_probepad(puz);
    probing= 1;
    nprobe++;

    if (probeon[PRBSRC_ADJACENT])
    {
	/* Scan through history, probing on cells adjacent to cells changed
	 * since the last guess.
	 */
	currsrc= PRBSRC_ADJACENT;

	for (k= puz->nhist - 1; k > 0; k--)
	{
	    h= HIST(puz,k);
	    ci= h->cell->line[D_ROW];
	    cj= h->cell->line[D_COL];

	    /* Check the neighbors */
	    for (neigh= 0; neigh < 4; neigh++)
	    {
		/* Find a neighbor, making sure it isn't off edge of grid */
		switch (neigh)
		{
		case 0: if (ci == 0) continue;
			i= ci - 1; j= cj; break;
		case 1: if (ci == sol->n[D_ROW]-1) continue;
			i= ci+1; j= cj; break;
		case 2: if (cj == 0) continue;
			i= ci; j= cj - 1; break;
		case 3: if (sol->line[D_ROW][ci][cj+1] == NULL) continue;
			i= ci; j= cj + 1; break;
		}
		cell= sol->line[D_ROW][i][j];

		/* Skip solved cells */
		if (cell->n < 2) continue;

		/* Test solve with each possible color */
		rc= probe_cell(puz, sol, cell, i, j, &bestnleft, bestc);
		if (rc < 0)
		    return (rc == -2) ? 1 : -1;
		if (rc > 0)
		{
		    *besti= i;
		    *bestj= j;
		    bestsrc= currsrc;
		}
	    }

	    /* Stop if we reach the cell that was our last guess point */
	    if (h->branch) break;
	}
    }

    /* Scan through all cells, probing on cells with 2 or more solved neighbors
     */
    if (probeon[PRBSRC_TWONEIGH] || probeon[PRBSRC_HEURISTIC])
    {
	ngood= 0;
	currsrc= PRBSRC_TWONEIGH;
	for (i= 0; i < sol->n[D_ROW]; i++)
	{
	    for (j= 0; (cell= sol->line[D_ROW][i][j]) != NULL; j++)
	    {
		/* Skip solved cells */
		if (cell->n < 2) continue;

		/* Skip cells with less than two solved neighbors */
		if (!probeon[PRBSRC_TWONEIGH] || count_neighbors(sol, i, j) < 2)
		{
		    if (probeon[PRBSRC_HEURISTIC])
			add_goodcell(puz,sol,i,j);
		    continue;
		}

		/* Test solve with each possible color */
		rc= probe_cell(puz, sol, cell, i, j, &bestnleft, bestc);
		if (rc < 0)
		    return (rc == -2) ? 1 : -1;
		if (rc > 0)
		{
		    *besti= i;
		    *bestj= j;
		    bestsrc= currsrc;
		}
	    }
	}
    }

    /* Probe on cells on the goodcell list */
    if (probeon[PRBSRC_HEURISTIC])
    {
	int a;
	currsrc= PRBSRC_HEURISTIC;
	for (a= 0; a < ngood; a++)
	{
	    /* Test solve with each possible color */
	    cell= sol->line[D_ROW][goodcell[a].i][goodcell[a].j];
	    rc= probe_cell(puz, sol, cell, goodcell[a].i, goodcell[a].j,
		    &bestnleft, bestc);
	    if (rc < 0)
		return (rc == -2) ? 1 : -1;
	    if (rc > 0)
	    {
		*besti= goodcell[a].i;
		*bestj= goodcell[a].j;
		bestsrc= currsrc;
	    }
	}
    }

    probeseq_res[PRBRES_BEST][currsrc]++;

    /* completed probing all cells - select best as our guess */
    if (bestnleft == INT_MAX)
    {
    	print_solution(stdout,puz,sol);
	printf("ERROR: found no cells to prob on.  Puzzle done?\n");
	printf("solved=%d cells=%d\n",puz->nsolved, puz->ncells);
	exit(1);
    }

    if (VP && VV) print_solution(stdout,puz,sol);
    if (VP || WC(*besti,*bestj))
	printf("P: PROBE SEQUENCE COMPLETE - CHOSING (%d,%d)%d (%s)\n",
	    *besti, *bestj, *bestc, Probesource[bestsrc]);

    probing= 0;
    return 0;
}


/* PROBE_STAT() - Print some statistics on probing */

void probe_stat_line(char *txt, int res)
{
    int i;
    long n= 0;
    int comma= 0;
    for (i= 0; i < N_PRBSRC; i++)
	n+= probeseq_res[res][i];
    printf("  %s %ld (",txt,n);
    for (i= 0; i < N_PRBSRC; i++)
    {
	if (!probeon[i]) continue;
	if (comma) fputs(", ", stdout);
	printf("%ld %s",probeseq_res[res][i],probesource[i]);
	comma= 1;
    }
    printf(")\n");
}

void probe_stats(void)
{
    int i, comma= 0;
    printf("Probe Sequences: %ld\n",nprobe);
    if (nprobe == 0) return;
    probe_stat_line("Found Contradiction:",PRBRES_CONTRADICT);
    probe_stat_line("Found Solution:     ",PRBRES_SOLVE);
    probe_stat_line("Choose Optimum:     ",PRBRES_BEST);
    printf("Total probes: %ld (",probes);
    for (i= 0; i < N_PRBSRC; i++)
    {
	if (!probeon[i]) continue;
	if (comma) fputs(", ", stdout);
	printf("%ld %s",probesrc[i],probesource[i]);
	comma= 1;
    }
    printf(")\n");
}


/* Return the fraction of probe sequences that have have ended in making
 * a guess (ie, have not found a contradiction or a solution).
 */
float probe_rate(void)
{
    int i,n;
    float rate;
    static long lastn= 0;
    static long lastnprobe= 0;

    for (i= 0; i < N_PRBSRC; i++)
	n+= probeseq_res[PRBRES_BEST][i];

    rate= (float)(n-lastn)/(float)(nprobe-lastnprobe);
    lastn= n;
    lastnprobe= nprobe;
    return rate;
}


/* SET_PROBING - set the probing algorithms to use */

int set_probing(int n)
{
    switch (n)
    {
    case 1:  /* Old style */
	probeon[PRBSRC_ADJACENT]= 0;
	probeon[PRBSRC_TWONEIGH]= 1;
	probeon[PRBSRC_HEURISTIC]= 0;
	return 1;

    case 2: /* Current Default */
	probeon[PRBSRC_ADJACENT]= 1;
	probeon[PRBSRC_TWONEIGH]= 1;
	probeon[PRBSRC_HEURISTIC]= 0;
	return 1;

    case 3:
	probeon[PRBSRC_ADJACENT]= 1;
	probeon[PRBSRC_TWONEIGH]= 0;
	probeon[PRBSRC_HEURISTIC]= 1;
	set_scoring_rule(4,0);	/* Use Simpson's heurstic */
	return 1;

    case 4:
	probeon[PRBSRC_ADJACENT]= 1;
	probeon[PRBSRC_TWONEIGH]= 1;
	probeon[PRBSRC_HEURISTIC]= 1;
	set_scoring_rule(4,0);	/* Use Simpson's heurstic */
	return 1;
    }
    return 0;
}
