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

/*
 * This file contains functions to evauluite and select guesses when
 * doing heuristic search.  It's meant to be highly reconfigurable to
 * aide with experimentation with different heuristics.
 */

#include "pbnsolve.h"

int count_colors= 0;	/* Should we count colors in each line? */
int score_adjust= 0;	/* Subtraction from line score when cell is solved */
int bookkeeping= 0;	/* Is bookkeeping for the above currently on? */


/* ----------------- LINE SCORE INITIALIZATION FUNCTIONS ----------------- */

/* These functions evaluate lines from the puzzle, giving LOWER scores
 * to lines that are more promising to work on.  These are called one
 * time during initialization to assign initial values.  If line scores
 * are updated at all during the run, then it is done by the solved_a_cell()
 * function.  Sometimes no line score initialization function is used.
 */

/* LINE_SCORE_ADHOC - My original adhoc line scoring system.  Prefers
 * lines with low slack and few clues.
 *
 *    score = S + 2*N
 *
 * where
 *     S = slack in line
 *     N = number of clues
 */

float line_score_adhoc(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    Clue *clue= &puz->clue[k][i];
    return clue->slack + 2*clue->n;
}


/* LINE_SCORE_MATH - Line scoring based on number of possible solutions
 * to the line.  Prefers lines with fewer possible solutions, which, for
 * an unsolved line is:
 *
 *    score = (S+N choose N)
 *
 * where
 *     S = slack in line
 *     N = number of clues
 */

float line_score_math(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    Clue *clue= &puz->clue[k][i];
    return bicoln(clue->slack + clue->n, clue->n);
}


/* LINE_SCORE_SIMPSON - A version of Steve Simpson's line scoring system.
 * Near as I can tell, Simpson's heuristic function is:
 *
 *    score =   L                            if B = L (line is full)
 *              B*(N+1) + N * (N - L - 1)    otherwise
 *
 * where
 *     L = line length
 *     N = number of clues
 *     B = sum of clue numbers
 *
 * Makes no particular sense to me, but there it is.
 *
 * We make a few changes.  First, we negate it, since we always minimize
 * instead of maximizing like Steve does.
 *
 * Second, since we already have computed slack but not B.  We rewrite the
 * formula to use that instead, Slack is L - B - N + 1 in the case of a two
 * color puzzle.  If we negate Simpson's formula and plug in slack (S) instead
 * of B, and do some algebra, we get:
 *
 *     score= -L			if S = 0
 *  	      (S+1)(N+1) - L - 2	otherwise
 *
 * I don't think the -L parts here make any difference. Since when we use this
 * with cell_score_add() we always add a row score and a column score, so
 * this, like the 2, is just a constant.  Seems like the main part of this is
 * the (S+1)(N+1) bit, which is not so very different from my algorithms.
 */

float line_score_simpson(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    Clue *clue= &puz->clue[k][i];
/*printf(" Scoring %s %d slack=%d n=%d len=%d score=%d\n",
cluename(puz->type,k),i,clue->slack,clue->n,clue->linelen,
    (clue->slack + 1)*(clue->n + 1) - clue->linelen - 2);*/
    return (clue->slack + 1)*(clue->n + 1) - clue->linelen - 2;
}


/* This points to the line_score function currently being used.  It can
 * be NULL if we aren't using a line score function.
 */

static float (*line_score)(Puzzle *puz, Solution *sol, dir_t k, line_t i);


/* ------------ CELL RATING FUNCTIONS ------------ */

/* These functions generate a rating for a cell, a number that LOWER if
 * the cell is a better candidate for making a guess on.  Many of them
 * simply combine the line scores in different ways to get a cell score.
 *
 * We cah have two cell rating functions.  The second one is used to break
 * ties.
 */


/* CELL_SCORE_MIN - Score is MIN(line scores).  We want at a cell in at
 * one line that is good as possible, never mind the other line.
 */
float cell_score_min(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    float si= puz->clue[0][i].score;
    float sj= puz->clue[1][j].score;
    return (si < sj) ? si : sj;
}


/* CELL_SCORE_SUM - Score is sum of line scores.  This is like selecting
 * for the line with the best average line score.
 */
float cell_score_sum(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    float si= puz->clue[0][i].score;
    float sj= puz->clue[1][j].score;
/*printf("   Cell (%d,%d) row%dscore=%.0f col%dscore=%.0f sum=%.0f\n",i,j,
	i,si,j,sj,si+sj);*/
    return si+sj;
}


/* CELL_SCORE_ADHOC - Score is 3*MIN(line scores) + MAX(line scores)
 * This is a compromise between taking the minimum, which completely
 * ignores the other crossing line, and taking the sum, which may take
 * a cell with two mediocre lines over one with one excellent line.
 */

float cell_score_adhoc(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    float si= puz->clue[0][i].score;
    float sj= puz->clue[1][j].score;
    return (si < sj) ? 3*si+sj : 3*sj+si;
}


/* CELL_SCORE_NEIGHBORS - Score is the number of unsolved neigbhor cells
 * surrounding this cell.
 */

float cell_score_neighbor(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    int count= 4;

    /* Count number of solved neighbors or edges */
    if (i == 0 || sol->line[0][i-1][j]->n == 1) count--;
    if (i == sol->n[0]-1 || sol->line[0][i+1][j]->n == 1) count--;
    if (j == 0 || sol->line[0][i][j-1]->n == 1) count--;
    if (sol->line[0][i][j+1]==NULL || sol->line[0][i][j+1]->n == 1) count--;

    return count;
}


/* These point to the cell_score functions currently being used.  The
 * second one can be NULL.  If there are two, the second is used to break
 * ties in the first
 */

float (*cell_score_1)(Puzzle *, Solution *, line_t, line_t);
float (*cell_score_2)(Puzzle *, Solution *, line_t, line_t);


/* ------------ COLOR SELECTION FUNCTIONS ------------ */

/* Once we have picked a cell to guess on, we need to pick a color to guess
 * for it.  These functions do that.
 */

/* PICK_COLOR_MAX - Pick maximum possible color as guess */

color_t pick_color_max(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c;
    for(c= puz->ncolor-1; c >= 0 && !may_be(cell,c); c--)
	    ;
    if (c <= 0) fail("Picked a cell to guess on with one color\n");
    return c;
}


/* PICK_COLOR_MIN - Pick minimum possible color as guess */

color_t pick_color_min(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c;
    for(c= 0; c < puz->ncolor && !may_be(cell,c); c++)
	    ;
    if (c >= puz->ncolor-1) fail("Picked a cell to guess on with one color\n");
    return c;
}


/* PICK_COLOR_RAND - Pick random color as guess
 */

color_t pick_color_rand(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c, bestc, n= 0;

    for(c= 0; c < puz->ncolor; c++)
	if (may_be(cell,c))
	{
	    n++;
	    if (rand() < RAND_MAX/n)
	    	bestc= c;
	}
    if (n <= 1) fail("Picked a cell to guess on with one color\n");
    return bestc;
}


/* PICK_COLOR_CONTRAST - Pick a color different from the neighboring colors.
 * This is a strategy aimed a making a guess that will lead to a lot of
 * consequnces.
 */

color_t pick_color_contrast(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c, bestc, n, bestn= -1;
    line_t i= cell->line[0];
    line_t j= cell->line[1];

    for(c= 0; c < puz->ncolor; c++)
	if (may_be(cell,c))
	{
	    n= 0;
	    if (i > 0)
	    {
		if (!may_be(sol->line[0][i-1][j], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (i < sol->n[0]-2)
	    {
	    	if (!may_be(sol->line[0][i+1][j], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (j > 0)
	    {
	    	if (!may_be(sol->line[0][i][j-1], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (j < sol->n[1]-2)
	    {
	    	if (!may_be(sol->line[0][i][j+1], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (n > bestn)
	    {
		bestc= c;
	     	bestn= n;
	    }
	}
    return bestc;
}


/* PICK_COLOR_PROB - Pick the most likely color for this cell, based on the
 * numbers of each color we know need to be in it's row and column, and the
 * numbers of each color that have already been marked in it's row and column.
 * This is based on Simpson.  This is a goal seeking strategy, aiming to
 * make a guess likely to be correct
 *
 * This requires count_colors to be true.
 */

color_t pick_color_prob(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c, bestc;
    line_t n, bestn= -9999;
    int k;

    for(c= 0; c < puz->ncolor; c++)
	if (may_be(cell,c))
	{
	    /* Add up number of cells of this color remaining in all the lines
	     * crossing this cell.  */
	    n= 0;
	    for (k= 0; k < puz->nset; k++)
		n+= puz->clue[k][cell->line[k]].colorcnt[c];

	    /* Largest number is the one we want */
	    if (n > bestn)
	    {
		bestn= n;
		bestc= c;
	    }
	}
    return bestc;
}


/* This points to the pick_color function currently being used */

color_t (*pick_color)(Puzzle *puz, Solution *sol, Cell *cell);

/* ---------------------------------------------------------------- */

/* BOOKKEEPING_ON - Start continuously updating the color count and score
 * arrays in the Clue data structure.  
 */


void bookkeeping_on(Puzzle *puz, Solution *sol)
{
    dir_t k;
    line_t i, j, nsolve;
    color_t c;
    Clue *clue;
    Cell *cell;

    /* Update the bookkeeping flag */
    if (bookkeeping) return;
    bookkeeping= 1;

    for (k= 0; k < puz->nset; k++)
    {
	for (i= 0; i < puz->n[k]; i++)
	{
	    clue= &puz->clue[k][i];
	    nsolve= 0;

	    /* initialize color count array */
	    if (count_colors)
	    {
		/* Add up clue values to get number of cells of each color */
		clue->colorcnt[0]= clue->linelen;
		for (j= 0; j < clue->n; j++)
		{
		    clue->colorcnt[0]-= clue->length[j];
		    clue->colorcnt[clue->color[j]]+= clue->length[j];
		}
	    }

	    if (count_colors || score_adjust)
	    {
		/* Scan line for solved cells. Subtract one from each color
		 * count for each solved cell of that color */
		for (j= 0; (cell= sol->line[k][i][j]) != NULL; j++)
		    if (cell->n == 1)
		    {
			nsolve++;
			if (count_colors)
			{
			    /* Find color of the cell */
			    for(c= 0; c < puz->ncolor && !may_be(cell,c); c++)
				;
			    clue->colorcnt[c]--;
			}

		    }
	    }

	    /* If we have a line scoring function, run it, now that color
	     * counts are correct */
	    if (line_score != NULL)
		clue->score= (*line_score)(puz, sol, k, i);

	    /* Now apply adjustments for solved cells st line score */
	    if (score_adjust)
		clue->score-= score_adjust * nsolve;
	}
    }
}


/* BOOKKEEPING_OFF - Shut down bookkeeping.  Actually we only really do
 * this if we are doing much.
 */

void bookkeeping_off()
{
    if (count_colors || score_adjust)
	bookkeeping= 0;
}


/* PICK_A_CELL - Pick a cell using the defined cell_score functions.  It prefers
 * the cells with the lowest value for cell_score_1.  If there is a tie, and
 * cell_score_2 is defined, it uses that to break ties.
 */

Cell *pick_a_cell(Puzzle *puz, Solution *sol)
{
    line_t i, j;
    float score1, minscore1;
    float score2=0, minscore2;
    int first= 1;
    Cell *cell, *favcell;

    if (puz->type != PT_GRID)
    	fail("pick_a_cell() only works for grid puzzles");

    for (j= 0; j < sol->n[1]; j++)
    {
    	for (i= 0; (cell= sol->line[1][j][i]) != NULL; i++)
	{
	    /* Not interested in solved cells */
	    if (cell->n == 1) continue;

	    score1= (*cell_score_1)(puz,sol,i,j);

	    if (!first && score1 > minscore1)
		continue;

	    if (cell_score_2 != NULL)
	    {
		score2= (*cell_score_2)(puz,sol,i,j);
		if (!first && score1 == minscore1 && score2 >= minscore2)
		    continue;
	    }
	    else if (!first && score1 == minscore1)
		continue;

	    favcell= cell;
	    first= 0;
	    minscore1= score1;
	    minscore2= score2;
	    if (VG) printf("G: MAX CELL %d,%d SCORE=%f/%f\n",
		i,j,score1,score2);
	}
    }

    if (first)
    {
    	if (VA) printf("Called pick-a-cell on complete puzzle\n");
	return NULL;
    }

    return favcell;
}

/* SOLVED_A_CELL - Update the solved/unsolved status of a cell.  A cell is
 * solved if it has only one possible color.
 *
 * If way is 1, then the cell is newly solved.  This should be called
 *   AFTER the new value has been set.
 *
 * If way is -1, then we are about to undo a previously solved cell.  This
 *   BEFORE the new value is set.
 *
 * If way is anything else, you are making a big mistake.
 */

void solved_a_cell(Puzzle *puz, Cell *cell, int way)
{
    int k;
    color_t c;

    /* Update our master count of number of solved cells */
    puz->nsolved+= way;

    if (!bookkeeping) return;

    if (count_colors)
    {
	/* If we are counting colors we need the color of the cell */
	for(c= 0; c < puz->ncolor && !may_be(cell,c); c++)
	    ;
    }
    else if (!score_adjust)
	return;

    /* If either count_colors or score_adjust is set, we need to do some
     * work on all lines containing this cell */
    for (k= 0; k < puz->nset; k++)
    {
	Clue *clue= &(puz->clue[k][cell->line[k]]);

	/* Update the score of the line, if we are doing that */
	clue->score-= way*score_adjust;

	/* Update the line's count of colors remaining */
	if (count_colors)
	    clue->colorcnt[c]+= way;
    }
}


/* SET_SCORING_RULE - set the guessing algorithm used for heuristic search (not
 * probing) as specified by an index number.  Returns 1 on success, 0 otherwise.
 *
 *  1 = Select cell randomly from among those with the highest number of
 *      neigbors.
 *
 *  2 = Among the cells with the highest number of neighbors, prefer those with
 *      low slack and/or low numbers of clues.
 *
 *  3 = Among the cells with the highest number of neighbors, prefer those whose
 *      unsolved lines have the lowest number of possible solutions.
 *
 *  4 = Simpson's heuristic, more or less.
 *
 * This should always be called if we plan to use any heuristic functions.
 * If may_override is false, then the setting passed in will not override any
 * settings made by previous calls to this function.
 */

static int have_set_scoring_rule= 0;

int set_scoring_rule(int n, int may_override)
{
    if (have_set_scoring_rule && !may_override) return 1;
    have_set_scoring_rule= 1;

    count_colors= 0;
    score_adjust= 0;
    switch (n)
    {
    case 1:
	/* Old SIMPLE algorithm */
	cell_score_1= &cell_score_neighbor;
	cell_score_2= NULL;
	line_score= NULL;
	pick_color= &pick_color_contrast;
	return 1;

    case 2:
	/* Old ADHOC algorithm */
	cell_score_1= &cell_score_neighbor;
	cell_score_2= &cell_score_adhoc;
	line_score= &line_score_adhoc;
	pick_color= &pick_color_contrast;
	return 1;

    case 3:
	/* Old MATH algorithm */
	cell_score_1= &cell_score_neighbor;
	cell_score_2= &cell_score_min;
	line_score= &line_score_math;
	pick_color= &pick_color_contrast;
	return 1;

    case 4: 
	/* Simpson's algorithm - approximately */
	cell_score_1= &cell_score_sum;
	cell_score_2= NULL;
	line_score= &line_score_simpson;
	pick_color= &pick_color_prob; count_colors= 1;
	score_adjust= 1;
	return 1;
    }

    return 0;
}
