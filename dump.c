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

char *typename(int type)
{
    if (type == PT_GRID) return "grid";
    if (type == PT_TRID) return "triddler";
    return "strange";
}

char *cluename(int type, int k)
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

char *CLUENAME(int type, int k)
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
    int c;
    for (c= 0; c < puz->ncolor; c++)
    	if (bit_test(bit,c))
	    putc(puz->color[c].ch, fp);
	else
	    putc(' ', fp);
}

void dump_line(FILE *fp, Puzzle *puz, Solution *sol, int k, int i)
{
    Cell *cell;
    int j;

    for (j= 0; (cell= sol->line[k][i][j]) != NULL; j++)
    {
	fputs(" (",fp);
	dump_bits(fp, puz, cell->bit);
	fputc(')', fp);
    }
    fputc('\n', fp);
}


void dump_solution(FILE *fp, Puzzle *puz, Solution *sol, int once)
{
    Cell *cell;
    int k, i, j, l;
    int max= once ? 1 : sol->nset;

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

void print_solution(FILE *fp, Puzzle *puz, Solution *sol)
{
    Cell *cell;
    int i, j, l;

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


void dump_puzzle(FILE *fp, Puzzle *puz)
{
    int k,i,j;
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
    for (i= 0; i < puz->ncolor; i++)
    {
        fprintf(fp,"%-3d: %s %s (%c)\n",
		i,
		puz->color[i].name,
		puz->color[i].rgb,
		puz->color[i].ch);
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
    	fprintf(fp,"Job #%d: %s %d (%d)\n",
	    n, cluename(puz->type,job->dir), job->n, job->priority);
    }

    if (puz->njob == 0) fprintf(fp,"No Jobs\n");
}

void dump_history(FILE *fp, Puzzle *puz, int full)
{
    Hist *h;
    int k;
    int nhist= 0;
    int nbranch= 0;

    for (h= puz->history; h != NULL; h= h->prev)
    {
    	if (full)
	{
	    fprintf(fp,"Cell");
	    for (k= 0; k < puz->nset; k++)
		printf(" %d",h->cell->line[k]);
	    fprintf(fp," was '");
	    dump_bits(fp, puz, h->bit);
	    fprintf(fp,h->branch ? "' BRANCH\n" : "'\n");
	}
	nhist++;
	if (h->branch) nbranch++;
    }
    fprintf(fp,"History Length=%d Branches=%d\n",nhist,nbranch);
}
