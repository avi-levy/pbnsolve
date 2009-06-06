extern FILE *srcfp;
extern char *srcimg;
extern int srcptr;
extern char *srcname;

int sgetc(void);
int sungetc(int c);
void srewind(void);
int skipwhite(void);

Puzzle *new_puzzle(void);
Puzzle *load_xml_puzzle(int index);
