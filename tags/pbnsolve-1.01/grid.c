/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */

#include "pbnsolve.h"
#include "read.h"

/* NEW_CELL - make an unknown cell for puzzle with the given number of colors
 * The line[] array is not initialized.  The cell memory allocated can actually
 * be more than the size of the 'struct' if the bit string needs to be longer
 * to hold all our colors.  On most machines, this will only happen if there
 * are more than 32 colors.
 */

Cell *new_cell(int ncolor)
{
    Cell *cell= (Cell *)malloc(sizeof(Cell) + bit_size(ncolor) - bit_size(1));
    bit_clearall(cell->bit, ncolor)
    cell->n= ncolor;
}


/* INIT_SOLUTION - Initialize a solution grid.  If "set" is true, then all
 * cells are initialized to allow any color.  Otherwise, they are initialized
 * to allow no colors.
 */

void init_solution(Puzzle *puz, Solution *sol, int set)
{
    int i, j, col;
    Cell *c;

    puz->nsolved= 0;

    /* Copy number of directions from puzzle */
    sol->nset= puz->nset;

    if (puz->type == PT_GRID)
    {
	/* Build the array of rows */
	sol->line[D_ROW]= (Cell ***)malloc(sizeof(Cell **) * sol->n[D_ROW]);
	puz->ncells= sol->n[D_ROW] * sol->n[D_COL];

	for (i= 0; i < sol->n[D_ROW]; i++)
	{
	    sol->line[D_ROW][i]=
	    	(Cell **)malloc(sizeof(Cell *) * (sol->n[D_COL] + 1));

	    for (j= 0; j < sol->n[D_COL]; j++)
	    {
		sol->line[D_ROW][i][j]= c= new_cell(puz->ncolor);
		if (set)
		    for (col= 0; col < puz->ncolor; col++)
		    	bit_set(c->bit, col);
		c->line[D_ROW]= i;
		c->line[D_COL]= j;
	    }
	    sol->line[D_ROW][i][sol->n[D_COL]]= NULL;
	}

	/* Build the redundant array of cols, pointing to the same cells */
	sol->line[D_COL]= (Cell ***)malloc(sizeof(Cell **) * sol->n[D_COL]);

	for (i= 0; i < sol->n[D_COL]; i++)
	{
	    sol->line[D_COL][i]=
	    	(Cell **)malloc(sizeof(Cell *) * (sol->n[D_ROW] + 1));

	    for (j= 0; j < sol->n[D_ROW]; j++)
		sol->line[D_COL][i][j]= sol->line[D_ROW][j][i];
	    sol->line[D_COL][i][sol->n[D_ROW]]= NULL;
	}
    }
    else
    {
    	/* This will be much like the above, except we build two redundant
	 * arrays, and the lines are not all the same length */

	fail("Not yet implemented for triddlers!\n");
    }
}


/* NEW_SOLUTION - generate a solution structure for the given puzzle.
 * All cells start unknown.
 */

Solution *new_solution(Puzzle *puz)
{
    Solution *sol= (Solution *)malloc(sizeof(Solution));
    int i;

    /* Copy size from puzzle */
    for (i= 0; i < puz->nset; i++)
    	sol->n[i]= puz->n[i];

    init_solution(puz, sol, 1);

    return sol;
}


/* Deallocated stuff pointed to by the given solution, but not he solution
 * itself.
 */

void free_subsolution(Solution *sol)
{
    int i,j,k;

    for (k= 0; k < sol->nset; k++)
    {
        for (i= 0; i < sol->n[k]; i++)
	{
	    if (k == 0)
	    {
		/* Only free cells for the first grid, since they are shared */
	        for (j= 0; sol->line[k][i][j] != NULL; j++)
		    free(sol->line[k][i][j]);
	    }
	    free(sol->line[k][i]);
	}
	free(sol->line[k]);
    }
}

/* FREE_SOLUTION - deallocate a solution data structure */

void free_solution(Solution *sol)
{
    free_subsolution(sol);
    free(sol);
}

/* FREE_SOLUTIONLIST - deallocate a solution list data structure */

void free_solution_list(SolutionList *sl)
{
    free_subsolution(&sl->s);
    free(sl);
}


/* COUNT_CELLS - return number of cells in the given line */

int count_cells(Puzzle *puz, Solution *sol, int k, int i)
{
    int ncell;
    Cell **cell;

    if (puz->type == PT_GRID)
    	return puz->n[1-k];

    /* Count the number of cells in the line */
    cell= sol->line[k][i];
    for (ncell= 0; cell[ncell] != NULL; ncell++)
    	;

    return ncell;
}


/* Compute and return slack for the i-th line in direction k.  */

int count_slack(Puzzle *puz, Solution *sol, int k, int i)
{
    int fill= 0;
    Clue *clue= &puz->clue[k][i];
    int j;

    if (clue->slack >= 0) return clue->slack;

    for (j= 0; j < clue->n; j++)
    {
    	if (j > 0 && clue->color[j-1] == clue->color[j]) fill++;
	fill+= clue->length[j];
    }
    return clue->slack= (count_cells(puz,sol,k,i) - fill);
}

/* Compute and return the number of cells that would obviously be painted
 * on a blank grid for the i-th line in direction k.
 */

int count_paint(Puzzle *puz, Solution *sol, int k, int i)
{
    int paint= 0;
    Clue *clue= &puz->clue[k][i];
    int slack= count_slack(puz,sol,k,i);
    int j;

    for (j= 0; j < clue->n; j++)
    	if (clue->length[j] > slack)
	    paint+= clue->length[j] - slack;
    return paint;
}

/* update the color count in a cell based on the current bit string */

void count_cell(Puzzle *puz, Cell *cell)
{
    int c;

    cell->n= 0;
    for (c= 0; c < puz->ncolor; c++)
    	if (may_be(cell, c)) cell->n++;
}


/* Generate a string version of a solution, pretty much in the same format
 * as print_solution() outputs.  The string is returned in malloced memory.
 */

char *solution_string(Puzzle *puz, Solution *sol)
{
    Cell *cell;
    int i,j,l;
    char *buf= (char *)malloc(sol->n[0] * (sol->n[1] + 1) + 2);
    char *str= buf;

    if (puz->type != PT_GRID)
    	fail("solution_string only works for grid puzzles\n");

    for (i= 0; i < sol->n[0]; i++)
    {
	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    if (cell->n > 1)
		(*str++)= '?';
	    else
		for (l= 0; l < puz->ncolor; l++)
		    if (bit_test(cell->bit, l))
		    {
			(*str++)= puz->color[l].ch;
			break;
		    }
	}
	(*str++)= '\n';
    }
    (*str++)= '\0';

    return buf;
}
