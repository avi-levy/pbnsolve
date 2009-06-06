/* Copyright (c) 2009, Jan Wolter, All Rights Reserved */

char *version= "1.1";

#include "pbnsolve.h"
#include <time.h>

int verb[NVERB];
int http= 0;
int mayprobe= 1, mergeprobe= 1;
int nlines, probes, guesses, backtracks, merges;


int main(int argc, char **argv)
{
    char *filename;
    Puzzle *puz;
    SolutionList *sl;
    Solution *sol= NULL;
    int i,k,left;
    int dump= 0;
    int *s;

    if (argc != 5)
    {
    	printf("usage: %s <file> [R|C] <n> [L|R]\n", argv[0]);
	exit(1);
    }
    VL= 1;

    filename= argv[1];
    k= ((argv[2][0] == 'r' || argv[2][0] == 'R') ? 0 : 1);
    i= atoi(argv[3]);
    left= (argv[4][0] == 'l' || argv[4][0] == 'L');

    puz= load_puzzle_file(filename, FF_UNKNOWN, 1);

    /* Print the name of the puzzle */
    if (puz->id != NULL || puz->title != NULL)
    {
	if (puz->id != NULL) printf("%s: ", puz->id);
	if (puz->seriestitle != NULL) printf("%s ",puz->seriestitle);
	if (puz->title != NULL) fputs(puz->title, stdout);
	putchar('\n');
    }

    if (dump) dump_puzzle(stdout,puz);

    for (sl= puz->sol; sl != NULL; sl= sl->next)
    {
	if (sl->type == STYPE_SAVED)
	{
	    sol= &sl->s;
	    printf("starting from saved grid\n");
	    break;
	}
    }

    /* Start from a blank grid if we didn't start from a saved game */
    if (sol == NULL)
    {
	printf("starting from blank grid\n");
	sol= new_solution(puz);
    }

    if (left)
    {
	printf("LEFT SOLVING:\n");
	s= left_solve(puz, sol, k, i);
    }
    else
    {
	printf("RIGHT SOLVING:\n");
	s= right_solve(puz, sol, k, i, count_cells(puz, sol, k, i));
    }

    if (s == NULL)
    	printf("CONTRADICTION\n");
    else
    {
	printf("SOLUTION:\n");
	    for (i= 0; s[i] >= 0; i++)
	            printf("Block %d at %d\n",i, s[i]);
    }

    exit(0);
}

void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    if (http)
    {
	fputs("<data>\n<status>FAIL: ",stdout);
	vfprintf(stdout,fmt, ap);
	fputs("</status>\n</data>\n",stdout);
    }
    else
	vfprintf(stderr,fmt, ap);
    va_end(ap);
    exit(1);
}
