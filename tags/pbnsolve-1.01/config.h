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

/* #define CPULIMIT 180 /**/
#define CPULIMIT 2

/* NICENESS - If NICENESS is defined, we run at diminished CPU priority to
 * minimize interference with other applications running on the computer.
 */

#define NICENESS /**/


/* DUMP FILE - IF DUMP_FILE is defined, a copy of the input is dumped to that
 * file before starting.  Mostly useful for debugging CGI.
 */

/* #define DUMP_FILE "/tmp/pbnsolve.dump"  /**/
