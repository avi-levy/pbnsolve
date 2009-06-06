/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */

#include "pbnsolve.h"

#include <time.h>
#ifdef CPULIMIT
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#ifdef MEMDEBUG
#include <mcheck.h>
#endif

int verbose= 0;
int maybacktrack= 1;
int mayprobe= 1;
int checkunique= 0;
int checksolution= 0;
int http= 0;

int nlines, probes, guesses, backtracks;


#ifdef CPULIMIT
void timeout(int sig)
{
    if (http)
        puts("<data>\n<status>TIMEOUT</status>\n</data>");
    else
	fputs("CPU time limit exceeded\n", stderr);
    exit(1);
}
#endif

int main(int argc, char **argv)
{
    char *filename= NULL;
    Puzzle *puz;
    SolutionList *sl= NULL;
    Solution *sol= NULL;
    char *altsoln= NULL, *goal= NULL;
    int index= -1;
    int i,j;
    int setsol= 0, nsol= -1;
    int dump= 0, statistics= 0;
    int isunique;
    int totallines, rc;
    clock_t sclock, eclock;
#ifdef DUMP_FILE
    FILE *dfp;
#endif

#ifdef CPULIMIT
    struct rlimit rlim;
    rlim.rlim_cur= CPULIMIT;
    signal(SIGXCPU,timeout);
    setrlimit(RLIMIT_CPU, &rlim);
#endif /* CPULIMIT */
#ifdef NICENESS
    nice(2);
#endif

#ifdef MEMDEBUG
    mtrace();
#endif

#ifdef GR_MATH
    init_factln();
#endif
#ifdef GC_RAND
    srand(time(NULL));
#endif

    if (strstr(argv[0],"pbnsolve.cgi") != NULL)
    {
	/* If the program is named 'pbnsolve.cgi' then run as a CGI, getting
	 * puzzle file image from the 'image' cgi variable.
	 */

	char *image;
	char *cgi_query= get_query();
    	http= 1;
	checksolution= 1;
	checkunique= 1;

	puts("Content-type: application/xml\n");

	image= query_lookup(cgi_query, "image");
	if (image == NULL)
	    fail("No puzzle description included in CGI query\n");

#ifdef DUMP_FILE
	if ((dfp= fopen(DUMP_FILE,"w")) != NULL)
	{
	    fputs(image,dfp);
	    fclose(dfp);
	}
#endif
	puz= load_puzzle_mem(image, FF_UNKNOWN, 1);

	free(image);
	free(cgi_query);
    }
    else
    {
	for (i= 1; i < argc; i++)
	{
	    if (argv[i][0] == '-')
	    {
		for (j= 1; argv[i][j] != '\0'; j++)
		{
		    switch (argv[i][j])
		    {
		    case 'c':
			checksolution= 1;
			checkunique= 1;
			maybacktrack= 1;
			break;
		    case 'd':
			dump= 1;
			break;
		    case 't':
			statistics= 1;
			http= 0;
			break;
		    case 'v':
			verbose++;
			break;
		    case 's':
			setsol= 1;
			nsol= 0;
			break;
		    case 'h':
			http= 1;
			statistics= 0;
			break;
		    case 'l':
			maybacktrack= 0;
			checkunique= 0;
			checksolution= 0;
			break;
		    case 'p':
			mayprobe= 0;
			break;
		    case 'u':
			maybacktrack= 1;
			checkunique= 1;
			break;
		    default:
			if (isdigit(argv[i][j]))
			{
			    if (setsol)
			    {
				nsol= 10*nsol + argv[i][j] - '0';
				continue;
			    }
			}
			goto usage;
		    }
		}
		if (setsol)
		{
		    if (nsol == 0)
			nsol= -1;
		    else
			setsol= 0;
		}
	    }
	    else if (setsol && atoi(argv[i]) > 0)
	    {
		nsol= atoi(argv[i]);
		setsol= 0;
	    }
	    else if (filename == NULL)
		filename= argv[i];
	    else if (index == -1)
		index= atoi(argv[i]);
	    else
		goto usage;
	}
	if (filename == NULL) goto usage;
	if (index == -1) index= 1;

	if (http) puts("Content-type: application/xml\n");
	puz= load_puzzle_file(filename, FF_UNKNOWN, index);
    }

    /* Print the name of the puzzle */
    if (!http && (puz->id != NULL || puz->title != NULL))
    {
	if (puz->id != NULL) printf("%s: ", puz->id);
	if (puz->seriestitle != NULL) printf("%s ",puz->seriestitle);
	if (puz->title != NULL) fputs(puz->title, stdout);
	putchar('\n');
    }

    if (dump) dump_puzzle(stdout,puz);

    if (nsol > 0 || checksolution)
    {
	for (i= 0, sl= puz->sol; sl != NULL; sl= sl->next)
	{
	    if (nsol > 0 && sl->type == STYPE_SAVED && ++i == nsol)
	    {
		sol= &sl->s;
		if (!checksolution || goal != NULL) break;
	    }
	    if (checksolution && sl->type == STYPE_GOAL)
	    {
	    	goal= solution_string(puz, &sl->s);
		if (nsol == 0 || sol != NULL) break;
	    }
	}
	if (nsol > 0 && sol == NULL)
	    fail("Saved solution #%d not found\n", nsol);
	if (checksolution && goal == NULL)
	    fail("Cannot check solution when there is none given\n");
    }

    /* Start from a blank grid if we didn't start from a saved game */
    if (sol == NULL) sol= new_solution(puz);

    if (statistics) sclock= clock();
    init_jobs(puz, sol);
    if (V3)
    {
    	puts("INITIAL JOBS:");
	dump_jobs(stdout,puz);
    }
    nlines= probes= guesses= backtracks= 0;
    while (1)
    {
	rc= solve(puz,sol);
	if (!checkunique || !rc || puz->history == NULL || puz->found != NULL)
	{
	    isunique= (puz->history == NULL && puz->found == NULL);
	    if (checksolution && !isunique)
	    	altsoln= solution_string(puz,sol);
	    break;
	}
	/* If we are checking for uniqueness, and we found a solution, but
	 * we aren't sure it is unique and we haven't found any others before
	 * then save that solution, and go looking for another.
	 */
    	puz->found= solution_string(puz, sol);
	if (goal != NULL && strcmp(puz->found, goal))
	{
	    if (V1) puts("FOUND A SOLUTION THAT DOES NOT MATCH GOAL");
	    isunique= 0;
	    altsoln= puz->found;
	    break;
	}
	if (V1) puts("FOUND ONE SOLUTION - CHECKING FOR OTHERS");
	backtrack(puz,sol);
    }
    if (statistics) eclock= clock();

    if (statistics || http)
    {
	totallines= 0;
	for (i= 0; i < puz->nset; i++)
	    totallines+= puz->n[i];
    }

    if (http)
    {
	puts("<data>");
	if (rc)
	{
	    printf("<status>OK</status>\n<unique>%d</unique>\n",
	    	isunique ? 1 : 0);
	    if (altsoln != NULL)
		printf("<alt>\n%s</alt>\n", altsoln);
	}
	else if (puz->found != NULL)
	    puts("<status>OK</status>\n<unique>1</unique>");
	else
	    puts("<status>FAIL: Puzzle has no solution</status>");
	if (guesses == 0 && probes == 0)
	    puts("<logic>1</logic>");
	printf("<difficulty>%d</difficulty>\n",nlines*100/totallines);
	puts("</data>");
    }
    else
    {
	if (rc)
	{
	    printf("STOPPED AT %s SOLUTION:\n", isunique ? "UNIQUE" : "A");
	    print_solution(stdout, puz, sol);
	    if (puz->found != NULL)
		printf("ALTERNATE SOLUTION\n%s",puz->found);
	}
	else if (puz->found != NULL)
	    printf("UNIQUE SOLUTION:\n%s",puz->found);
	else
	    puts("NO SOLUTION");
    }

    if (statistics)
    {
	totallines= 0;
	for (i= 0; i < puz->nset; i++)
	    totallines+= puz->n[i];
	printf("Cells Solved: %d of %d\n",puz->nsolved, puz->ncells);
	printf("Lines in Puzzle: %d\n",totallines);
	printf("Lines Processed: %d (%d%%)\n",nlines,nlines*100/totallines);
	if (mayprobe)
	    printf("Backtracking: %d probes, %d guesses, %d backtracks\n",
	    	probes,guesses,backtracks);
	else
	    printf("Backtracking: %d guesses, %d backtracks\n",
	    	guesses,backtracks);
	printf("Processing Time: %f sec \n",
		(float)(eclock - sclock)/CLOCKS_PER_SEC);
    }

    if (sl != NULL && sl->note) printf("%s\n",sl->note);

    if (sl == NULL) free_solution(sol);
    free_puzzle(puz);

    exit(0);

usage:
    fprintf(stderr,"usage: %s [-dvvvl] [-s#] <filename> [<index>]\n", argv[0]);
    exit(1);
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
