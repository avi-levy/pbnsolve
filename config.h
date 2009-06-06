/* Configuration Settings */

/* GUESS RATING ALGORITHM.  When we get stuck, we need to choose a square to
 * guess on.  Different algorithms have been implemented to choose a square.
 * Define one of the following.  These are used only if probing is not used.
 */

/* #define GR_SIMPLE /**/
/* #define GR_ADHOC /**/
#define GR_MATH /**/

/* GUESS COLOR ALGORITHM.  When we make a guess, what color should it be?
 * Define one of the following.  These are used only if probing is not used.
 */

/* #define GC_MAX /**/
/* #define GC_MIN /**/
/* #define GC_RAND /**/
#define GC_CONTRAST /**/

/* CPU TIME LIMIT - If CPULIMIT is defined it is the maximum number of seconds
 * for which pbnsolve will run.  If it exceeds this limit, it will terminate
 * with a brief error message.  If it is not defined, then there is no limit.
 */

#define CPULIMIT 1 /**/

/* NICENESS - If NICENESS is defined, we run at deminished CPU priority to
 * minimize interference with other applications running on the computer.
 */

#define NICENESS /**/


/* DUMP FILE - IF DUMP_FILE is defined, a copy of the input is dumped to that
 * file before starting.  Mostly useful for debugging CGI.
 */

#define DUMP_FILE "/tmp/pbnsolve.dump"  /**/


/* Define VERBOSITY to the number of levels of verbosity to support.  Less
 * verbosity makes the program a smidgeon faster by needing to test the
 * verbose flag less often.
 */

#define VERBOSITY 3

#if VERBOSITY > 0
#define V1 verbose
#else
#define V1 0
#endif

#if VERBOSITY > 1
#define V2 verbose > 1
#else
#define V2 0
#endif

#if VERBOSITY > 2
#define V3 verbose > 2
#else
#define V3 0
#endif
