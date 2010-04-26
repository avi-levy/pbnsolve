/* Configuration Settings */

/* CPU TIME LIMITS - Limits on CPU time can be set from the command line with
 * the -x option.  If pbnsolve uses more than that number of CPU seconds, then
 * it will terminate abruptly.  A CPU limit of zero means no limit.
 *
 * If CGI_CPULIMIT greater than zero, then that will be the CPU limit
 * whenever pbnsolve is run as a CGI program.  If it is zero then there would
 * be no time limit, which would probably be a bad idea on a webserver.  Most
 * puzzles that pbnsolve is likely to solve at all take under a second, so
 * this can be low.
 *
 * If DEFAULT_CPULIMIT is nonzero, then that will be the time limit when run
 * from the command line without a -x flag.
 */

#define CGI_CPULIMIT 1
#define DEFAULT_CPULIMIT 0

/* NICENESS - If NICENESS is defined, we run at diminished CPU priority to
 * minimize interference with other applications running on the computer.
 * This is virtually always a good idea.
 */

#define NICENESS /**/

/* PLOD/SPRINT MODE - If both the hueristic and probing search strategies are
 * enabled, then we use a plod/sprint mode, where we start out probing.  If
 * we reach a point where PLOD_INIT probes have failed to find a contradiction
 * (or solve the puzzle), then we switch over to using heuristic search for
 * the next SPRINT_LENGTH times when logic failing stalls.  The we revert
 * to probing, until PLOD_LENGTH probes fail to find a contradiction and
 * the cycle repeats.
 */

#define PLOD_INIT 40
#define SPRINT_LENGTH 4000
#define PLOD_LENGTH 40

/* DUMP FILE - IF DUMP_FILE is defined, a copy of the input is dumped to that
 * file before starting.  Mostly useful for debugging CGI versions of the
 * program.
 */

/* #define DUMP_FILE "/tmp/pbnsolve.dump"  /**/

/* LIMIT COLORS - If LIMITCOLORS is defined, pbnsolve will only be able to
 * handle puzzles with 32 colors or less (maybe 64 if your computer has
 * 64 bit long ints).  If this is not defined unlimited colors can be used,
 * at least in theory.  That's never been tested.  The advantage of limiting
 * colors is that it improves performance.
 */

#define LIMITCOLORS /**/

/* NO XML - If you don't have libxml2, then you can define NOXML and pbnsolve
 * will be built that can read only the non-xml file formats.  Currently all
 * the supported non-xml file formats are pathetic, but that's life.
 */

/* #define NOXML /**/

/* DEBUG LEVEL - This controls how much debugging code is compiled into the
 * program.  If debugging code is included in the program, then it is turned
 * on with the -v flag.  However, it slows down the solver very slightly even
 * when it is turned off, so for production use you probably want it off.
 * The levels are:
 *
 *   DEBUG_LEVEL == 0            No debugging output is turned on.
 *   DEBUG_LEVEL == 1            Everything off except -vA.
 *   DEBUG_LEVEL == 2            Everything on except -vL.
 *   DEBUG_LEVEL == 3            Everything on.
 */

#define DEBUG_LEVEL 2

/* LINE WATCH - If this flag is enabled, then you can give arguments like
 * -wR12 to watch row 12 or -wC0 to watch column zero.  Lots of diagnostics
 * about everything that happens to that row or column will be printed.
 * Disabling this at compile time makes the program significantly faster.
 */

/*#define LINEWATCH /**/
