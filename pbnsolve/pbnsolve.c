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

char *version= "1.1";

#include "pbnsolve.h"

#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef MEMDEBUG
#include <mcheck.h>
#endif

int verb[NVERB];
int maybacktrack= 1, mayexhaust= 1, maycontradict= 0;
int mayprobe= 1, mergeprobe= 0, maylinesolve= 1;
int contradepth= 2;
int checkunique= 0;
int checksolution= 0;
int http= 0, terse= 0;

int nlines, probes, guesses, backtracks, merges, exh_runs, exh_cells;
int contratests, contrafound;


void timeout(int sig)
{
    if (http)
        puts("<data>\n<status>TIMEOUT</status>\n</data>");
    else if (terse)
        puts("timeout");
    else
    {
	fputs("CPU time limit exceeded\n", stderr);
        puts("CPU time limit exceeded");
    }
    exit(1);
}


void setcpulimit(int secs)
{
    struct rlimit rlim;
    rlim.rlim_cur= secs;
    signal(SIGXCPU,timeout);
    setrlimit(RLIMIT_CPU, &rlim);
}

int setalg(char ch)
{
    switch (ch)
    {
    case 'L':
	/* LRO Line Solving */
	maylinesolve= 1;
    	break;
    case 'E':
	/* Exhaustive Checking */
	mayexhaust= 1;
    	break;
    case 'C':
	/* Contradiction Checking */
	maycontradict= 1;
    	break;
    case 'G':
	/* Guessing */
	maybacktrack= 1;
	mayprobe= 0;
	mergeprobe= 0;
    	break;
    case 'P':
	/* Probing without Merging */
	maybacktrack= 1;
	mayprobe= 1;
	mergeprobe= 0;
    	break;
    case 'M':
	/* Probing with Merging */
	maybacktrack= 1;
	mayprobe= 1;
	mergeprobe= 1;
    	break;
    case 0:
	/* Called to turn everything off */
	maylinesolve= 0;
	mayexhaust= 0;
	maybacktrack= 0;
	mayprobe= 0;
	mergeprobe= 0;
	maycontradict= 0;
    	break;
    default:
    	return 0;
    }
    return 1;
}


#define SN_NONE 0
#define SN_START 1
#define SN_INDEX 2
#define SN_CPU 3
#define SN_CDEPTH 4

int main(int argc, char **argv)
{
    char *filename= NULL;
    Puzzle *puz;
    SolutionList *sl= NULL;
    Solution *sol= NULL;
    int setnumber= SN_NONE;
    char *format= NULL, *vi, *vchar= VCHAR;
    char *altsoln= NULL, *goal= NULL;
    int pindex= 1;	/* if input file has multiple puzzles, which to do */
    int cpulimit= DEFAULT_CPULIMIT;
    int i,j, vflag= 0, aflag= 0;
    int startsol= 0;	/* solution to start from, 0 means none */
    int setformat= 0, dump= 0, statistics= 0;
    int fmt, isunique, iscomplete;
    int totallines, rc;
    clock_t sclock, eclock;
#ifdef DUMP_FILE
    FILE *dfp;
#endif
#ifdef LINEWATCH
#define MAXWATCH 20
    struct { dir_t dir; line_t i; } linewatch[MAXWATCH];
    int nwatch= 0;
#endif

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
	 * puzzle file image from the 'image' cgi variable and puzzle format
	 * from "format".  We run as if the -hc flags were given.
	 */

	char *image;
	char *cgi_query= get_query();
    	http= 1;
	checksolution= 1;
	checkunique= 1;

#if CGI_CPULIMIT > 0
	setcpulimit(CGI_CPULIMIT);
#endif

	puts("Content-type: application/xml\n");

	image= query_lookup(cgi_query, "image");
	if (image == NULL)
	    fail("No puzzle description included in CGI query\n");

	format= query_lookup(cgi_query, "format");
	if (format == NULL)
	    fmt= FF_UNKNOWN;
	else
	{
	    fmt= fmt_code(format);
	    free(format);
	}

#ifdef DUMP_FILE
	if ((dfp= fopen(DUMP_FILE,"w")) != NULL)
	{
	    fputs(image,dfp);
	    fclose(dfp);
	}
#endif
	puz= load_puzzle_mem(image, fmt, 1);

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
		    /* Set flags to turn on debugging for various sections */
		    if (vflag && (vi= index(vchar,argv[i][j])))
		    {
		    	verb[vi-vchar]= 1;
			continue;
		    }
		    else
		    	vflag= 0;

		    if (aflag && setalg(argv[i][j]))
			continue;
		    else
		    	aflag= 0;

		    /* Parse numeric arguments to -n -s or -x */
		    if (isdigit(argv[i][j]))
		    {
			switch (setnumber)
			{
			case SN_START:
			    startsol= 10*startsol + argv[i][j] - '0';
			    continue;

			case SN_INDEX:
			    pindex= 10*pindex + argv[i][j] - '0';
			    continue;

			case SN_CPU:
			    cpulimit= 10*cpulimit + argv[i][j] - '0';
			    continue;

			case SN_CDEPTH:
			    contradepth= 10*contradepth + argv[i][j] - '0';
			    continue;
			}
			goto usage;
		    }
		    else
		    	setnumber= SN_NONE;

		    switch (argv[i][j])
		    {
		    case 'b':
		    	terse= 1;
			break;
		    case 'c':
			checksolution= 1;
			checkunique= 1;
			break;
		    case 'o':
			dump= 1;
			break;
		    case 't':
			statistics= 1;
			http= 0;
			break;
		    case 'v':
			vflag= 1;
			break;
		    case 'a':
			aflag= 1;
			setalg(0);
			break;
		    case 'n':
			setnumber= SN_INDEX;
			pindex= 0;
			break;
		    case 'd':
			setnumber= SN_CDEPTH;
			contradepth= 0;
			break;
		    case 's':
			setnumber= SN_START;
			startsol= 0;
			break;
		    case 'x':
			setnumber= SN_CPU;
			cpulimit= 0;
			break;
		    case 'h':
			http= 1;
			statistics= 0;
			break;
		    case 'u':
			checkunique= 1;
			break;
		    case 'f':
		    	if (argv[i][j+1] != '\0')
			{
			    format= &(argv[i][j+1]);
			    goto optdone;
			}
			setformat= 1;
			break;
#ifdef LINEWATCH
		    case 'w':
			if (nwatch >= MAXWATCH) goto usage;
			j++;
		    	if (argv[i][j] == 'R' || argv[i][j] == 'r')
			    linewatch[nwatch].dir= D_ROW;
			else if (argv[i][j] == 'C' || argv[i][j] == 'c')
			    linewatch[nwatch].dir= D_COL;
			else
			    goto usage;
			linewatch[nwatch].i= 0;
			for (j++; isdigit(argv[i][j]); j++)
			   linewatch[nwatch].i= 10*linewatch[nwatch].i +
			   	argv[i][j] - '0';
			nwatch++;
			j--;
			break;
#endif
		    default:
			goto usage;
		    }
		}
		optdone:;

		if ( (setnumber == SN_START && startsol > 0) ||
		     (setnumber == SN_INDEX && pindex > 0) ||
		     (setnumber == SN_CPU && cpulimit > 0) ||
		     (setnumber == SN_CDEPTH && contradepth > 0) )
			setnumber= SN_NONE;
	    }
	    else if (setformat)
	    {
	    	format= argv[i];
		setformat= 0;
	    }
	    else if (setnumber != SN_NONE && atoi(argv[i]) > 0)
	    {
		int n= atoi(argv[i]);
		if (setnumber == SN_START) startsol= n;
		else if (setnumber == SN_INDEX) pindex= n;
		else if (setnumber == SN_CPU) cpulimit= n;
		else if (setnumber == SN_CDEPTH) contradepth= n;
		setnumber= SN_NONE;
	    }
	    else if (filename == NULL)
		filename= argv[i];
	    else
		goto usage;
	}
	if (pindex < 1) pindex= 1;

	/* Uniqueness checking (ie, looking to see if there is another
	 * solution if the first one we found wasn't logically arrived at)
	 * is only meaningful if we are backtracking
	 */
	if (!maybacktrack) checksolution= checkunique= 0;

	if (!maylinesolve && !mayexhaust)
		fail("Need -aL or -aE to be able to solve puzzles.\n");

	if (setformat && !format) goto usage;
	if (format)
	{   
	    fmt= fmt_code(format);
	    if (fmt == FF_UNKNOWN) fail("Unknown file format: %s\n", format);
	}
	else
	    fmt= (filename == NULL) ? FF_XML : FF_UNKNOWN;

	if (cpulimit > 0)
	    setcpulimit(cpulimit);

	if (http) puts("Content-type: application/xml\n");

	if (filename == NULL)
	    puz= load_puzzle_stdin(fmt, pindex);
	else
	    puz= load_puzzle_file(filename, fmt, pindex);
    }

#ifdef LINEWATCH
    /* Set the watch flags on the lines selected to be watched */
    for (j= 0; j < nwatch; j++)
    {
	dir_t k= linewatch[j].dir;
	i= linewatch[j].i;
    	if (i < puz->n[k])
	    puz->clue[k][i].watch= 1;
	else
	    printf("Can't watch %s %d - no such line\n",
	    	cluename(puz->type,k), i);
    }
#endif

    /* Initialize the bitstring handling code for puzzles of our size */
    fbit_init(puz->ncolor);

    /* preallocate some arrays */
    init_line(puz);
    if (mergeprobe) init_merge(puz);

    if (VA) printf("A: pbnsolve version %s\n", version);

    /* Print the name of the puzzle */
    if (!http && (puz->id != NULL || puz->title != NULL))
    {
	if (puz->id != NULL) printf("%s: ", puz->id);
	if (puz->seriestitle != NULL) printf("%s ",puz->seriestitle);
	if (puz->title != NULL) fputs(puz->title, stdout);
	putchar('\n');
    }

    if (dump) dump_puzzle(stdout,puz);

    if (startsol > 0 || checksolution)
    {
	for (i= 0, sl= puz->sol; sl != NULL; sl= sl->next)
	{
	    if (startsol > 0 && sl->type == STYPE_SAVED && ++i == startsol)
	    {
		sol= &sl->s;
		puz->nsolved= count_solved(sol);
		if (!checksolution || goal != NULL) break;
	    }
	    if (checksolution && sl->type == STYPE_GOAL)
	    {
	    	goal= solution_string(puz, &sl->s);
		if (startsol <= 0 || sol != NULL) break;
	    }
	}
	if (startsol > 0 && sol == NULL)
	    fail("Saved solution #%d not found\n", startsol);
	if (checksolution && goal == NULL)
	    fail("Cannot check solution when there is none given\n");
    }

    /* Start from a blank grid if we didn't start from a saved game */
    if (sol == NULL)
	sol= new_solution(puz);

    if (statistics) sclock= clock();
    init_jobs(puz, sol);
    if (VJ)
    {
    	puts("J: INITIAL JOBS:");
	dump_jobs(stdout,puz);
    }
    nlines= probes= guesses= backtracks= merges= exh_runs= exh_cells= 0;
    contratests= contrafound= 0;
    while (1)
    {
	rc= solve(puz,sol);
	iscomplete= rc && (puz->nsolved == puz->ncells); /* true unless -l */
	if (!checkunique || !rc || puz->nhist == 0 || puz->found != NULL)
	{
	    /* Time to stop searching.  Either
	     *  (1) we aren't checking for uniqueness
	     *  (2) the last search didn't find any solution
	     *  (3) the last search involved no guessing
	     *  (4) a previous search found a solution.
	     * The solution we found is unique if (3) is true and (4) is false.
	     */
	    isunique= (iscomplete && puz->nhist==0 && puz->found==NULL);

	    /* If we know the puzzle is not unique, then it is because we
	     * previously found another solution.  If checksolution is true,
	     * and we went on to search more, then the first one must have
	     * been the goal, so this one isn't.
	     */
	    if (checksolution && !isunique)
	    	altsoln= solution_string(puz,sol);
	    break;
	}

	/* If we are checking for uniqueness, and we found a solution, but
	 * we aren't sure it is unique and we haven't found any others before
	 * then we don't know yet if the puzzle is unique or not, so we still
	 * have some work to do.  Start by saving the solution we found.
	 */
    	puz->found= solution_string(puz, sol);

	/* If we have the expected goal, check if the solution we found that.
	 * if not, we can take non-uniqueness as proven without further
	 * searching.
	 */
	if (goal != NULL && strcmp(puz->found, goal))
	{
	    if (VA) puts("A: FOUND A SOLUTION THAT DOES NOT MATCH GOAL");
	    isunique= 0;
	    altsoln= puz->found;
	    break;
	}
	/* Otherwise, there is nothing to do but to backtrack from the current
	 * solution and then resume the search to see if we can find a
	 * differnt one.
	 */
	if (VA) printf("A: FOUND ONE SOLUTION - CHECKING FOR MORE\n%s",
	    puz->found);
	backtrack(puz,sol);
    }
    if (statistics) eclock= clock();

    if (statistics || http || terse)
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
	    printf("<logic>%d</logic>\n", contrafound == 0 ? 1 : 2);
	printf("<difficulty>%d</difficulty>\n",nlines*100/totallines);
	puts("</data>");
    }
    else if (terse)
    {
	if (!iscomplete && puz->found == NULL)
	{
	    puts(maybacktrack ? "contradiction" : "stalled");
	}
	else if (rc)
	{
	    if (isunique)
	    {
		if (nlines <= totallines) printf("trivial ");
		if (guesses == 0 && probes == 0)
		{
		    if (contrafound > 0)
			printf("unique depth-%d\n",contradepth);
		    else
			puts("unique logical");
		}
		else
		    puts("unique");
	    }
	    else if (puz->found == NULL)
	    {
		puts("solvable");
	    }
	    else
	    {
		puts("multiple");
	    }
	}
	else if (puz->found != NULL)
	{
	    puts("unique");
	}
	else
	    puts("contradition");
    }
    else
    {
	if (!iscomplete && puz->found == NULL)
	{
	    if (maybacktrack)
		printf("NO SOLUTION.\n");
	    else
	    {
		/* Found incomplete solution.  Only with -l */
		printf("STALLED WITH PARTIAL SOLUTION:\n");
		print_solution(stdout, puz, sol);
	    }
	}
	else if (rc)
	{
	    if (isunique)
	    {
		/* Found a solution without ever having to guess */
		if (guesses == 0 && probes == 0)
		{
		    if (contrafound > 0)
			printf("UNIQUE DEPTH-%d SOLUTION:\n",contradepth);
		    else
			printf("UNIQUE LINE SOLUTION:\n");
		}
		else
		    printf("UNIQUE SOLUTION:\n");
		print_solution(stdout, puz, sol);
	    }
	    else if (puz->found == NULL)
	    {
		/* Found one solution, but aren't checking uniqueness */
		printf("STOPPED WITH SOLUTION:\n");
		print_solution(stdout, puz, sol);
	    }
	    else if (altsoln != NULL)
	    {
		/* Found a solution different from the goal (-c only) */
		printf("FOUND NON-GOAL SOLUTION:\n%s",altsoln);
	    }
	    else
	    {
		/* Found two solutions */
		printf("FOUND MULTIPLE SOLUTIONS:\n");
		print_solution(stdout, puz, sol);
		printf("ALTERNATE SOLUTION\n%s",puz->found);
	    }
	}
	else if (puz->found != NULL)
	{
	    /* Found one solution by guessing, but hit a contradiction when
	     * looking for another.
	     */
	    printf("UNIQUE SOLUTION:\n%s",puz->found);
	    puz->nsolved= puz->ncells;
	}
	else
	    /* Hit a contradiction without finding any solution */
	    puts("NO SOLUTION");
    }

    if (statistics)
    {
	totallines= 0;
	for (i= 0; i < puz->nset; i++)
	    totallines+= puz->n[i];
	printf("Cells Solved: %d of %d\n",puz->nsolved, puz->ncells);
	printf("Lines in Puzzle: %d\n",totallines);
	printf("Lines Processed: %d (%d%%)\n",nlines,nlines/totallines*100);
	if (exh_runs > 0 || mayexhaust)
	    printf("Exhaustive Search: %d cell%s in %d pass%s\n",
	    	exh_cells, (exh_cells == 1) ?"":"s",
		exh_runs, (exh_runs == 1) ?"":"es");
	if (maycontradict)
	    printf("Contradiction Testing: %d tests, %d found\n",
	    	contratests, contrafound);
	if (!mayprobe)
	    printf("Backtracking: %d guesses, %d backtracks\n",
	    	guesses,backtracks);
	else if (!mergeprobe)
	    printf("Backtracking: %d probes, %d guesses, %d backtracks\n",
	    	probes,guesses,backtracks);
	else
	    printf("Backtracking: %d probes, %d merges, %d guesses, "
	    	   "%d backtracks\n", probes,merges,guesses,backtracks);
	printf("Processing Time: %f sec \n",
		(float)(eclock - sclock)/CLOCKS_PER_SEC);
    }

    if (sl != NULL && sl->note) printf("%s\n",sl->note);

    if (sl == NULL) free_solution(sol);
    free_puzzle(puz);

    exit(0);

usage:
    fprintf(stderr,"usage: %s [-cdehu] [-s#] [-n#] [-x#] [-aLEGPM] [-vABEGJLMPUSV] [<filename>]\n",
    	argv[0]);
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
