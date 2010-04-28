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

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "config.h"
#include "bitstring.h"

/* Types */
typedef short line_t;	/* a row/column number - must be signed */
typedef char color_t;   /* a color number, an index into a color bit string */
typedef char dir_t;     /* A direction */
typedef char byte;	/* various small numbers */

#define MAXLINE SHRT_MAX  /* Max value that can be stored in line_t */

/* Puzzle solution - Representing a partial solution of a puzzle.
 *
 *  For each cell in the puzzle, we have a Cell structure  This contains
 *  a bitstring with one bit for each color in the puzzle.  Bit i is 1 if
 *  it is possible that the puzzle could be color i.  The cell structure also
 *  keeps a count of the number of bits set in the bitstring.
 *
 *  The solution structure contains arrays of pointers to Cells.  For grid
 *  puzzles we have sol->line[D_ROW] and sol->line[D_COL].  For triddlers
 *  we have sol->line[D_HORIZ], sol->line[D_UP] and sol->line[D_DOWN].
 *  Each of these will point to an array of pointers to arrays of pointers
 *  to cells.  So sol->line[D_ROW][3] is an array representing row 3 of the
 *  grid, and sol->line[D_COL][0] is an array representing the first column
 *  of the puzzle.   Each of these line arrays is NULL terminated (because
 *  for triddlers they will be all different lengths.
 *
 *  sol->line[D_ROW][3][0] and sol->line[D_COL][0][3] will point to the same
 *  Cell object, the cell for column 0, row 3.  In that cell object,
 *  (cell->line[D_ROW],cell->index[D_ROW]) is (3,0), and
 *  (cell->line[D_COL],cell->index[D_COL]) is (0,3).
 *
 *  This somewhat redundant array structure is meant to generalize to things
 *  like triddlers more easily, and simplify a lot of the solver coding by
 *  making rows and columns work exactly alike.
 */

typedef struct {
    line_t line[3];	/* 2 or 3 line numbers of this cell */
    line_t index[3];	/* 2 or 3 indexes of this cell in those lines */
    color_t n;		/* Number of bits set in the bit string */
    bit_decl(bit,1);	/* bit string with 1 for each possible color */
    line_t id;		/* A unique number in (0,rows*cols-1) for this cell */

    /* Do not define any fields after 'bit'.  When we allocate memory for this
     * data structure, we will actually be allocating more if we need longer
     * bitstrings.  Though actually that would only happen if we had a puzzle
     * with more than 32 colors, which isn't too likely.
     */
} Cell;

/* Background color is always color zero */
#define BGCOLOR 0

/* Macro to test if a cell may be a given color */
#define may_be(cell,color)  bit_test(cell->bit,color)
/* Macro to test if a bitstring may be background color */
#define bit_bg(bit) (*(bit) & 1)
/* Macro to test if a cell may be background color */
#define may_be_bg(cell) bit_bg(cell->bit)


typedef struct {
    dir_t nset;		/* Number of directions 2 for grids, 3 for triddlers */
    Cell ***line[3];	/* 2 or 3 roots for the cell array */
    line_t n[3];	/* Length of the line[] arrays */
    Cell **spiral;	/* An array pointing to all cells in spiral pattern */
} Solution;


/* Solution List - A list of solutions, loaded from the XML file */

#define STYPE_GOAL 0
#define STYPE_SOLUTION 1
#define STYPE_SAVED 2

typedef struct solution_list {
    char *id;		/* ID string for solution (or NULL) */
    byte type;		/* Solution type (STYPE_* values) */
    char *note;		/* Any text describing the solution */
    Solution s;		/* The solution */
    struct solution_list *next;
} SolutionList;


/* Clue Structure - describes a row or column clue.  We cache the last
 * left-most and right-most solutions to this row, to use as a starting point
 * the next time we need to find such solutions for the row.  These can't
 * be used anymore if we backtrack past the point where they were created,
 * so we remember the history array index at that time.  If we backtrack,
 * we reset the timestamps to LINEMAX so they will continue to look invalid
 * as we go forward again. */

typedef struct {
    line_t n,s;		/* Number of clues and size of array */
    line_t *length;	/* Array of n clue lengths */
    color_t *color;	/* Array of n clue colors (indexes into puz->color) */
    line_t linelen;	/* Number of cells in this line */
    int jobindex;	/* Where is this clue on the job list? -1 if not. */
    line_t slack;	/* Amount of slack in this clue */
    float score;	/* A heuristic score for this line */
    line_t *colorcnt;	/* Counts of each color in this line */
    line_t *lpos,*rpos;	/* Last result from left_solve() and right_solve() */
    line_t *lcov,*rcov;	/* Coverage arrays that go with lpos, and rpos */
    line_t lbadb,rbadb;	/* Bad interval index in lpos,rpos. LINEMAX if none */
    line_t lbadi,rbadi;	/* Cell index spoiling lpos,rcov.  LINEMAX if none  */
    int lstamp,rstamp;	/* nhist value at time that lpos,rpos were computed */
#ifdef LINEWATCH
    byte watch;		/* True if we are watching this line */
#endif
} Clue;


/* Color Definition - definition of one color. */

typedef struct {
    char *name;		/* Color name */
    char *rgb;		/* RGB color value */
    char ch;		/* A character to represent color */
} ColorDef;


/* Element of heap of lines that need working on */

typedef struct {
    int priority;	/* High number for more promissing jobs */
    int depth;		/* Used in contradiction search only */
    byte dir;		/* Direction of line that needs work (D_ROW/D_COL) */
    line_t n;		/* Index of line that needs work */
} Job;

/* History of things set, used for backtracking */

typedef struct hist_list {
    byte branch;	/* Was this a branch point? */
    Cell *cell;		/* The cell that was set */
    color_t n;		/* Old n value of cell */
    bit_decl(bit,1);	/* Old bit string of cell */
    /* Do not define any fields after 'bit'.  When we allocate memory for this
     * data structure, we will actually be allocating more if we need longer
     * bitstrings.
     */
} Hist;

/* Size of a history element */
#define HISTSIZE(puz) (sizeof(Hist) + fbit_size - bit_size(1))

/* i-th element of the history array */
#define HIST(puz,i) (Hist *)(((char *)puz->history)+(i)*HISTSIZE(puz))

/* Probe Merge List - settings that have been made for all probes on the
 * current cell.
 */

typedef struct merge_elem {
    struct merge_elem *next;	/* Link to next cell in list */
    Cell *cell;			/* The cell that was set */
    color_t maxc;		/* Maximum guess number */
    bit_decl(bit,1);	 	/* each eliminated color has a 1 bit */
    /* Do not define any fields after 'bit'.  When we allocate memory for this
     * data structure, we will actually be allocating more if we need longer
     * bitstrings.
     */
} MergeElem;


/* Puzzle definition - Describes a puzzle (not it's solution).
 *
 * Color table.  puz->color is an array of color definitions used in the
 *  puz.  puz->color[0] is always the background color.  All others are
 *  clue colors.  ncolor gives the number of colors, including the background
 *  color.
 *
 * Clue sets.  The number of clue sets is given by nset, and will be either
 *  2 for a grid or 3 for a griddler.  So, for a grid puzzle, the side clues
 *  are given by puz->clue[D_ROW] and the top clues are given by
 *  puz->clue[D_TOP].  Each of these will point to an array of Clue objects,
 *  as defined above which give the clues for the corresponding rows or
 *  columns.
 */

/* Puzzle types stored in puz->type */

#define PT_GRID 0	/* Standard Grid puzzle */
#define PT_TRID 1	/* "triddler" */

/* Indices into puz->clue[] array used in grid puzzles */
#define D_ROW 0		/* Clues on side of puzzle */
#define D_COL 1		/* Clues on top of puzzle */

/* Indices into puz->clue[] array used in triddler puzzles */
#define D_HORIZ 0	/* Clues for - horizontal rows */
#define D_UP    1	/* Clues for / lines that climb from left to right */
#define D_DOWN  2	/* Clues for \ lines that descend from left to right */

typedef struct {
    byte type;		/* Puzzle type.  Some PT_ value */
    dir_t nset;		/* Number of directions 2 for grids, 3 for triddlers */
    color_t ncolor,scolor; /* Number of colors used, size of color array */
    Clue *clue[3];	/* Arrays of clues (nset of which are used) */
    line_t n[3];	/* Length of the clue[] arrays */
    ColorDef *color;	/* Array of color definitions */
    char *source;
    char *id;
    char *title;
    char *seriestitle;
    char *author;
    char *copyright;
    char *description;
    SolutionList *sol;	/* List of solutions loaded from the file */
    int ncells;		/* Number of cells in the puzzle */
    int nsolved;	/* Number of cells with only one possible color */
    Job *job;		/* Pointer to priority queue of jobs */
    int sjob, njob;	/* Allocated and current size of job array */
    Hist *history;	/* Undo history, if any */
    int nhist,shist;	/* Number of things in history, and size of history */
    char *found;	/* A stringified solution we have found, if any */
} Puzzle;

/* Various file formats that we can read */

#define FF_UNKNOWN	0	/* File type unknown */
#define FF_XML		1	/* Our own XML file format */
#define FF_MK		2	/* The Olsak's MK file format */
#define FF_NIN		3	/* Wilk's NIN file format */
#define FF_NON		4	/* Simpson's NON file format */
#define FF_PBM		5	/* netpbm PBM image file */
#define FF_LP		6	/* Bosch's format for LP solver */
#define FF_OLSAK	7	/* The Olsak's G multicolor file format */


/* Debug Flags - You can disable any of these completely by just defining them
 * to zero.  Then the optimizer will throw out those error messages and things
 * will run a tiny bit faster.
 */

#define VA verb[0]	/* Top Level Messages */
#define VB verb[1]	/* Backtracking */
#define VC verb[2]	/* Contradiction Search */
#define VE verb[3]	/* Try Everything */
#define VG verb[4]	/* Guessing */
#define VH verb[5]	/* Hash */
#define VJ verb[6]	/* Job Management */
#define VL verb[7]	/* Line Solver Details */
#define VM verb[8]	/* Merging */
#define VP verb[9]	/* Probing */
#define VQ verb[10]	/* Probing statistics */
#define VU verb[11]	/* Undo Information from Job Management */
#define VS verb[12]	/* Cell State Changes */
#define VV verb[13]	/* Report with extra verbosity */
#define VCHAR "ABCEGHJLMPQUSV"
#define NVERB 13

extern int verb[];

#if DEBUG_LEVEL < 3
#undef VL
#define VL 0
#endif

#if DEBUG_LEVEL < 2
#undef VB
#define VB 0
#undef VC
#define VC 0
#undef VE
#define VE 0
#undef VH
#define VH 0
#undef VG
#define VG 0
#undef VJ
#define VJ 0
#undef VM
#define VM 0
#undef VP
#define VP 0
#undef VQ
#define VQ 0
#define NO_VQ
#undef VU
#define VU 0
#undef VS
#define VS 0
#undef VV
#define VV 0
#endif

#if DEBUG_LEVEL < 1
#undef VA
#define VA 0
#endif

/* Macros */

#define safedup(x) (x ? strdup(x) : NULL)
#define safefree(x) if (x) free(x)


/* Global variables */

extern int maylinesolve;
extern int maybacktrack;
extern int mayprobe;
extern int mayguess;
extern int mergeprobe;
extern int checkunique;
extern int checksolution;
extern int mayexhaust;
extern int maycontradict;
extern int contradepth;
extern int maycache, cachelines;
extern long nsprint, nplod;

/* pbnsolve.c functions */

void fail(const char *fmt, ...);

/* read.c functions */

Puzzle *load_puzzle_file(char *filename, int fmt, int index);
Puzzle *load_puzzle_mem(char *image, int fmt, int index);
Puzzle *load_puzzle_stdin(int fmt, int index);

/* puzz.c functions */

Puzzle *new_puzzle(void);
void free_puzzle(Puzzle *puz);
color_t new_color(Puzzle *puz);
color_t find_color(Puzzle *puz, char *name);
color_t find_color_char(Puzzle *puz, char ch);
color_t find_or_add_color(Puzzle *puz, char *name);
int add_color(Puzzle *puz, char *name, char *rgb, char ch);

/* dump.c functions */
char *cluename(byte type, dir_t k);
char *CLUENAME(byte type, dir_t k);
void dump_bits(FILE *fp, Puzzle *puz, bit_type *bits);
void dump_binary(FILE *fp, bit_type *bit, int len);
void print_solution(FILE *fp, Puzzle *puz, Solution *sol);
void dump_pos(FILE *fp, line_t *pos, line_t *len);
void print_coord(FILE *fp, Puzzle *puz, Cell *cell);
void dump_line(FILE *fp, Puzzle *puz, Solution *sol, byte k, line_t i);
void dump_solution(FILE *fp, Puzzle *puz, Solution *sol, int once);
void dump_puzzle(FILE *fp, Puzzle *puz);
void dump_jobs(FILE *fp, Puzzle *puz);
void dump_history(FILE *fp, Puzzle *puz, int full);

/* grid.c functions */
Cell *new_cell(color_t ncolor);
Solution *new_solution(Puzzle *puz);
int count_solved(Solution *sol);
void init_solution(Puzzle *puz, Solution *sol, int set);
void free_solution(Solution *sol);
void free_solution_list(SolutionList *sl);
line_t count_paint(Puzzle *puz, Solution *sol, dir_t k, line_t i);
void count_cell(Puzzle *puz, Cell *cell);
char *solution_string(Puzzle *puz, Solution *sol);
int check_nsolved(Puzzle *puz, Solution *sol);
void make_spiral(Solution *sol);
int count_neighbors(Solution *sol, line_t i, line_t j);

/* line_lro.c functions */
void init_line(Puzzle *puz);
void dump_lro_solve(Puzzle *puz, dir_t k, line_t i, bit_type *col);
int left_check(Clue *clue, line_t i, bit_type *bit);
int right_check(Clue *clue, line_t i, bit_type *bit);
void left_undo(Puzzle *puz, Clue *clue, Cell **line, line_t i, bit_type *new);
void right_undo(Puzzle *puz, Clue *clue, Cell **line, line_t i, bit_type *new);
line_t *left_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i, int savepos);
line_t *right_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i, int savepos);
bit_type *lro_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i);
int apply_lro(Puzzle *puz, Solution *sol, dir_t k, line_t i, int depth);

/* job.c functions */
void flush_jobs(Puzzle *puz);
void init_jobs(Puzzle *puz, Solution *sol);
int next_job(Puzzle *puz, dir_t *k, line_t *i, int *depth);
void add_job(Puzzle *puz, dir_t k, line_t i, int depth, int bonus);
void add_jobs(Puzzle *puz, Solution *sol, int except, Cell *cell, int depth, bit_type *old);
Hist *add_hist(Puzzle *puz, Cell *cell, int branch);
Hist *add_hist2(Puzzle *puz, Cell *cell, color_t oldn, bit_type *oldbit, int branch);
int backtrack(Puzzle *puz, Solution *sol);
int newedge(Puzzle *puz, Cell **line, line_t i, bit_type *old, bit_type *new);

/* solve.c functions */
extern long nlines, guesses, backtracks, probes, merges;
extern long contratests, contrafound;
void guess_cell(Puzzle *puz, Solution *sol, Cell *cell, color_t c);
int logic_solve(Puzzle *puz, Solution *sol, int contradicting);
int solve(Puzzle *puz, Solution *sol);

/* score.c function */
void bookkeeping_on(Puzzle *puz, Solution *sol);
void bookkeeping_off();
Cell *pick_a_cell(Puzzle *puz, Solution *sol);
void solved_a_cell(Puzzle *puz, Cell *cell, int way);
extern color_t (*pick_color)(Puzzle *puz, Solution *sol, Cell *cell);
extern float (*cell_score_1)(Puzzle *, Solution *, line_t, line_t);
extern float (*cell_score_2)(Puzzle *, Solution *, line_t, line_t);
int set_scoring_rule(int n, int may_override);

/* probe.c functions */
extern int probing;
extern bit_type *probepad;
#define propad(cell) (probepad+(cell->id)*fbit_size)
void probe_init(Puzzle *puz, Solution *sol);
int probe(Puzzle *puz, Solution *sol, line_t *besti, line_t *bestj, color_t *bestc);
void probe_stats(void);
float probe_rate(void);
int set_probing(int n);

/* contradict.c functions */
int contradict(Puzzle *puz, Solution *sol);

/* exhaust.c functions */
extern long exh_runs, exh_cells;
int try_everything(Puzzle *puz, Solution *sol, int check);

/* http.c functions */
char *get_query(void);
char *query_lookup(char *query, char *var);

/* clue.c functions */
void clue_init(Puzzle *puz, Solution *sol);
void make_clues(Puzzle *puz, Solution *sol);

/* merge.c functions */
extern int merging;
void merge_cancel(void);
void merge_guess(void);
void merge_set(Puzzle *puz, Cell *cell, bit_type *bit);
int merge_check(Puzzle *puz, Solution *sol);

/* line_cache.c function */
void init_cache(Puzzle *puz);
bit_type *line_cache(Puzzle *puz,Solution *sol,dir_t k,line_t i);
void add_cache(Puzzle *puz, Solution *sol, dir_t k, line_t i);
extern long cache_hit, cache_req, cache_add, cache_flush;
