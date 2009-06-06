/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */

#include "pbnsolve.h"

/* MAKE_CLUES - Given a goal solution, generate corresponding puzzle clues.
 */

void append_clue(Clue *clue, int length, int color)
{
    if (clue->n >= clue->s)
    {
    	clue->s += 8;
	clue->length= (int *) realloc(clue->length, clue->s * sizeof(int));
	clue->color= (int *) realloc(clue->color, clue->s * sizeof(int));
    }
    clue->length[clue->n]= length;
    clue->color[clue->n]= color;
    clue->n++;
}

void make_clues(Puzzle *puz, Solution *sol)
{
    Clue *clue;
    Cell *cell;
    int k, i, j, c;
    int color, newcolor, count;

    if (puz->type != PT_GRID)
    	fail("Don't know how to generate clues for non-grid puzzles\n");

    for (k= 0; k < puz->nset; k++)
    {
	/* Allocate array for this direction */
	puz->n[k]= sol->n[k];
	puz->clue[k]= (Clue *)malloc(puz->n[k] * sizeof(Clue));

	for (i= 0; i < puz->n[k]; i++)
	{
	    clue= &puz->clue[k][i];

	    /* Allocate clue arrays for this line */
	    clue->n= 0;
	    clue->s= 8;		/* Initial size - may resize later */
	    clue->length= (int *) malloc(clue->s * sizeof(int));
	    clue->color= (int *) malloc(clue->s * sizeof(int));
	    clue->jobindex= -1;
	    clue->slack= -1;

	    /* Scan the line of the solution - this scans one past EOL */
	    color= 0;
	    j= 0;
	    do {
		cell= sol->line[k][i][j];

		/* Get color of current cell */
		newcolor= 0;
		if (cell != NULL)
		    for (c= 0; c < puz->ncolor; c++)
		    	if (may_be(cell,c))
			{
			    newcolor= c;
			    break;
			}
		if (color > 0)
		{
		    if (newcolor != color)
		    {
		    	append_clue(clue, count, color);
			color= newcolor;
			count= 1;
		    }
		    else
		    	count++;
		}
		else if (newcolor > 0)
		{
		    count= 1;
		    color= newcolor;
		}

		j++;
	    } while (cell != NULL);	/* Testing previous cell! */
	}
    }
}
