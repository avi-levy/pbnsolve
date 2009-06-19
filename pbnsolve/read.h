/* The current input */
extern FILE *srcfp;
extern char *srcimg;
extern int srcptr;
extern char *srcname;

#define MAXBUF 1024

/* Routines to read from the input */
int sgetc(void);
int sungetc(int c);
void srewind(void);
int skipwhite(void);
void skiptoeol(void);
int sread_pint(int newline);
char *sread_keyword(void);

Puzzle *new_puzzle(void);
void init_bw_colors(Puzzle *puz);
Puzzle *init_bw_puzzle(void);

Puzzle *load_xml_puzzle(int index);
Puzzle *load_mk_puzzle(void);
Puzzle *load_nin_puzzle(void);
Puzzle *load_non_puzzle(void);
Puzzle *load_pbm_puzzle(void);
Puzzle *load_lp_puzzle(void);
Puzzle *load_g_puzzle(void);
