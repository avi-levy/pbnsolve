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


/* DUMP FILE - IF DUMP_FILE is defined, a copy of the input is dumped to that
 * file before starting.  Mostly useful for debugging CGI versions of the
 * program.
 */

/* #define DUMP_FILE "/tmp/pbnsolve.dump"  /**/

/* NO XML - If you don't have libxml2, then you can define NOXML and pbnsolve
 * will be built that can read only the non-xml file formats.  Currently all
 * the supported non-xml file formats are pathetic, but that's life.
 */

/* #define NOXML /**/
