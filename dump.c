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

char *typename(byte type)
{
    if (type == PT_GRID) return "grid";
    if (type == PT_TRID) return "triddler";
    return "strange";
}

char *cluename(byte type, dir_t k)
{
    if (type == PT_TRID)
    {
	if (k == D_HORIZ) return "horiz";
	if (k == D_UP) return "up";
	if (k == D_DOWN) return "down";
    }
    else
    {
	if (k == D_ROW) return "row";
	if (k == D_COL) return "col";
    }
    return "strange";
}

char *CLUENAME(byte type, dir_t k)
{
    if (type == PT_TRID)
    {
	if (k == D_HORIZ) return "HORIZONTAL";
	if (k == D_UP) return "UP DIAGONAL";
	if (k == D_DOWN) return "DOWN DIAGONAL";
    }
    else
    {
	if (k == D_ROW) return "ROW";
	if (k == D_COL) return "COLUMN";
    }
    return "strange";
}


/* Dump a bit string, writing the color character for any bit that is set,
 * and a blank for the rest.
 */

void dump_bits(FILE *fp, Puzzle *puz, bit_type *bit)
{
    color_t c;
    for (c= 0; c < puz->ncolor; c++)
    	if (bit_test(bit,c))
	    putc(puz->color[c].ch, fp);
	else
	    putc(' ', fp);
}

/* Dump a bit string consisting of len bit_types.  Prints as binary string */

void dump_binary(FILE *fp, bit_type *bit, int len)
{
    int i,b;
    for (i= 0; i < len; i++)
    {
	for (b= _bit_intsiz-1; b >= 0; b--)
	    putc(bit_test(bit+i, b) ? '1' : '0', fp);
	putc(' ',fp);
    }
}

/* Print the current state of a line of the puzzle solution */

void dump_line(FILE *fp, Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    Cell *cell;
    line_t j;

    fputc('|', fp);
    for (j= 0; (cell= sol->line[k][i][j]) != NULL; j++)
    {
	dump_bits(fp, puz, cell->bit);
	fputc('|', fp);
    }
    fputc('\n', fp);
}

/* Dump dup a solution returned from left_solve or right_solve */

void dump_pos(FILE *fp, line_t *pos, line_t *len)
{
    int i;

    for (i= 0; pos[i] >= 0; i++)
    	fprintf(fp,"B%dL%d@%d ",i, len[i], pos[i]);
    fputc('\n', fp);
}


/* Print the coordinates of a cell */
void print_coord(FILE *fp, Puzzle *puz, Cell *cell)
{
    dir_t k;
    for (k= 0; k < puz->nset; k++)
    {
	fputc(k==0?'(':',', fp);
	fprintf(fp,"%d",cell->line[k]);
    }
    fputc(')',fp);
}


void dump_solution(FILE *fp, Puzzle *puz, Solution *sol, int once)
{
    Cell *cell;
    dir_t k;
    line_t i;
    dir_t max= once ? 1 : sol->nset;

    for (k= 0; k < max; k++)
    {
	fprintf(fp, "  DIRECTION %d (%s)\n", k, cluename(puz->type, k));
	for (i= 0; i < sol->n[k]; i++)
	{
	    fputs("   ",fp);
	    dump_line(fp, puz, sol, k, i);
	}
    }
}

/* Print an unadorned solution grid. */
void print_solution(FILE *fp, Puzzle *puz, Solution *sol)
{
    Cell *cell;
    line_t i, j;
    color_t l;

    for (i= 0; i < sol->n[0]; i++)
    {
	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    if (cell->n > 1)
	    	fputc('?', fp);
	    else
		for (l= 0; l < puz->ncolor; l++)
		    if (bit_test(cell->bit, l))
		    	fputc(puz->color[l].ch, fp);
	}
	fputc('\n', fp);
    }
}


/* Print a solution grid adorned with row and column numbers. If bits is
 * true, we show all color bits for each cell, instead of just printing a
 * ? if there is more than one possible color. */

void print_snapshot(FILE *fp, Puzzle *puz, Solution *sol, int bits)
{
    Cell *cell;
    line_t i, j;
    color_t l;
    line_t ncol= sol->n[D_COL];
    line_t nrow= sol->n[D_ROW];
    int w= bits ? puz->ncolor+1 : 1;

    /* Print column numbers */
    if (ncol > 9)
    {
	fputs("    ",fp);
	for (l= 1; l < w/2; l++) fputc(' ',fp);
	for (j= 0; j < ncol; j++)
	{
	    fputc(j < 9 ? ' ' : '0'+(((j+1)/10)%10), fp);
	    for (l= 1; l < w; l++) fputc(' ',fp);
	}
	fputc('\n',fp);
    }
    fputs("    ",fp);
    for (l= 1; l < w/2; l++) fputc(' ',fp);
    for (j= 0; j < ncol; j++)
    {
	fputc('0'+((j+1)%10), fp);
	for (l= 1; l < w; l++) fputc(' ',fp);
    }
    fputc('\n',fp);
    fputs("   +",fp);
    for (j= 0; j < ncol*w; j++) fputc('-', fp);
    fputc('\n',fp);

    for (i= 0; i < nrow; i++)
    {
	fprintf(fp,"%3d|",i+1);
	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    if (bits)
	    {
		for (l= 0; l < puz->ncolor; l++)
		    fputc(bit_test(cell->bit,l)?puz->color[l].ch:' ', fp);
		fputc(' ',fp);
	    }
	    else
	    {
		if (cell->n > 1)
		    fputc('?', fp);
		else
		    for (l= 0; l < puz->ncolor; l++)
			if (bit_test(cell->bit, l))
			    fputc(puz->color[l].ch, fp);
	    }
	}
	fputc('\n', fp);
    }
}


/* DUMP_BACKTRACK - Print a history of how we got to where we are from the
 * last branch point.  This is complex because we want to do it without
 * actually backtracking, so we need to use a separate separate scratch
 * pad version of the board to backtrack in.
 */

void dump_backtrack(FILE *fp, Puzzle *puz, Solution *sol)
{
    color_t c;
    char *cantbe= malloc(puz->ncolor+1);
    char buf[1024];
    int k,n;
    bit_type *bit;
    char *out= NULL;
    int gridsize= puz->n[D_ROW]*puz->n[D_COL];
    /* I should have used a simple array indexed by the cell.id fields, but
     * I forgot that they existed and it's not worth rewriting this now */
    bit_type **grid= (bit_type **)
	calloc(sizeof(bit_type *),gridsize);
#define GRID(i,j) grid[i*puz->n[D_COL]+j]

    /* Count the number of items to be printed */
    for (k= puz->nhist-1, n= 0; k > 0 && !HIST(puz,k)->branch; k--, n++)
	;

    for (k= puz->nhist-1; k > 0; k--)
    {
	Hist *h= HIST(puz, k);
	if (h->branch) break;
	line_t i= h->cell->line[D_ROW];
	line_t j= h->cell->line[D_COL];
	int canbe= -1;
	int cb= 0;
	cantbe[cb]= '\0';

	/* Copy current solution cells into our grid */
	if ((bit= GRID(i,j)) == NULL)
	{
	    GRID(i,j)= bit= malloc(sizeof(bit_type)*fbit_size);
	    fbit_cpy(bit, h->cell->bit);
	}

	/* See what has changed */
	for (c= 0; c < puz->ncolor; c++)
	{
	    if (bit_test(bit,c))
	    {
	    	if (canbe == -1)
		    canbe= c;
		else
		    canbe= -2;
	    }
	    else if (bit_test(h->bit,c))
	    {
		cantbe[cb++]= puz->color[c].ch;
		cantbe[cb]= '\0';
	    }
	}
	if (canbe >= 0)
	    sprintf(buf,"r%dc%d is %c",i+1,j+1,puz->color[canbe].ch);
	else
	    sprintf(buf,"r%dc%d not %s",i+1,j+1,cantbe);
	if (out == NULL)
	{
	    out= strdup(buf);
	}
	else
	{
	    char *new= malloc(strlen(buf)+10+strlen(out));
	    strcpy(new,buf);
	    n= (n-1)%5;
	    strcat(new,n == 0 ?" ;\n   ":" ; ");
	    strcat(new,out);
	    free(out);
	    out= new;
	}
    }

    fputs("   ",fp);
    fputs(out,fp);
    fputs(" ;\n",fp);

    /* Deallocate dynamic memory */
    free(out);
    int x;
    for (x= 0; x < gridsize; x++)
	if (grid[x] != NULL) free(grid[x]);
    free(grid);
}

void dump_puzzle(FILE *fp, Puzzle *puz)
{
    color_t c;
    dir_t k;
    line_t i,j;
    SolutionList *sl;
    char *p;

    fprintf(fp, "TYPE: %s (%d clue sets)\n\n", typename(puz->type), puz->nset);

    fprintf(fp, "SOURCE: %s\n",
    	puz->source ? puz->source : "UNKNOWN");
    fprintf(fp, "ID: %s\n",
    	puz->id ? puz->id : "UNKNOWN");
    fprintf(fp, "SERIES TITLE: %s\n",
    	puz->seriestitle ? puz->seriestitle : "UNKNOWN");
    fprintf(fp, "TITLE: %s\n",
    	puz->title ? puz->title : "UNKNOWN");
    fprintf(fp, "AUTHOR: %s\n",
    	puz->author ? puz->author : "UNKNOWN");
    fprintf(fp, "COPYRIGHT: %s\n",
    	puz->copyright ? puz->copyright : "UNKNOWN");
    fprintf(fp, "DESCRIPTION: %s\n",
    	puz->description ? puz->description : "UNKNOWN");

    fprintf(fp, "\nCOLORS %d (allocated for %d):\n",puz->ncolor,puz->scolor);
    for (c= 0; c < puz->ncolor; c++)
    {
        fprintf(fp,"%-3d: %s %s (%c)\n",
		c,
		puz->color[c].name,
		puz->color[c].rgb,
		puz->color[c].ch);
    }

    for (k= 0; k < puz->nset; k++)
    {
	fprintf(fp, "\nCLUE SET %d (%s): %d clues:\n",
	    k, cluename(puz->type,k), puz->n[k]);
	for (i= 0; i < puz->n[k]; i++)
	{
	    fprintf(fp," %-3d:", i);
	    for (j= 0; j< puz->clue[k][i].n; j++)
	    {
		fprintf(fp," %d(%d)",
		    puz->clue[k][i].length[j],
		    puz->clue[k][i].color[j]);
	    }
	    fprintf(fp,"\n");
	}
    }

    for (sl= puz->sol; sl != NULL; sl= sl->next)
    {
    	fprintf(fp, "SOLUTION TYPE=%s ID=%s\n",
		sl->type == STYPE_GOAL ? "GOAL" :
		   (sl->type == STYPE_SOLUTION ? "SOLUTION" : "SAVED"),
		sl->id ? sl->id : "none");

    	if (sl->note != NULL)
	{
	    fputs("  NOTE:\n    ",fp);
	    for (p= sl->note; *p != '\0'; *p++)
	    {
		fputc(*p, fp);
	        if (*p == '\n') fputs("    ",fp);
	    }
	    fputc('\n', fp);
	}

	dump_solution(fp, puz, &sl->s, 0);
    }
}


void dump_jobs(FILE *fp, Puzzle *puz)
{
    Job *job;
    int n;

    for (n= 1; n <= puz->njob; n++)
    {
	job= &puz->job[n];
    	fprintf(fp,"Job #%d: %s %d (prior=%d,depth=%d)\n",
	    n, cluename(puz->type,job->dir), job->n,
	    job->priority, job->depth);
    }

    if (puz->njob == 0) fprintf(fp,"No Jobs\n");
}


void dump_history(FILE *fp, Puzzle *puz, int full)
{
    Hist *h;
    int i;
    int nbranch= 0;

    for (i= 0; i < puz->nhist; i++)
    {
	h= HIST(puz,i);
    	if (full)
	{
	    fprintf(fp,"Cell ");
	    print_coord(fp,puz,h->cell);
	    fprintf(fp," was '");
	    dump_bits(fp, puz, h->bit);
	    fprintf(fp,h->branch ? "' BRANCH\n" : "'\n");
	}
	if (h->branch) nbranch++;
    }
    fprintf(fp,"History Length=%d Branches=%d\n",puz->nhist,nbranch);
}
