/* Some bitstring manipulation macros.  These were written long ago by
 * Paul A. Vixie and were posted to mod.sources.  I'm assuming they are
 * open source.
 *
 * We have added the fbit_* functions which take advantage of the fact that
 * all bitstrings in pbnsolve are the same size and expect to find those
 * sizes in fbit_n (number of bits) and fbit_size (number of ints to store
 * those bits).  I tried caching _bit_intn(N) and _bit_mask(N) in arrays,
 * but that actually made things slower.
 *
 * If the LIMITCOLORS is defined at compile time, then the macros
 * are further optimized under the assumption that all bit strings fit in
 * one long int.
 */

/* bitstring.h - bit string manipulation macros
 * vix 26feb87 [written]
 * vix 03mar87 [fixed stupid bug in setall/clearall]
 * vix 25mar87 [last-minute cleanup before mod.sources gets it]
 */

#ifndef	_bitstring_defined
#define	_bitstring_defined

/*
 * there is something like this in 4.3, but that's licensed source code that
 * I'd rather not depend on, so I'll reinvent the wheel (incompatibly).
 */

/*
 * except for the number of bits per int, and the other constants, this should
 * port painlessly just about anywhere.  please #ifdef any changes with your
 * compiler-induced constants (check the CC man page, it'll be something like
 * 'vax' or 'mc68000' or 'sun' or some such).  also please mail any changes
 * back to me (ucbvax!dual!ptsfa!vixie!paul) for inclusion in future releases.
 */

/*
 * (constants used internally -- these can change from machine to machine)
 */
			/*
			 * how many bits in the unit returned by sizeof ?
			 */
#define	_bit_charsize	8

			/*
			 * what type will the bitstring be an array of ?
			 */
#define	bit_type	unsigned long

			/*
			 * how many bits in an int ?
			 */
#define	_bit_intsiz	(sizeof(bit_type) * _bit_charsize)

			/*
			 * an int of all '0' bits
			 */
#define	_bit_0s		((bit_type)0)

			/*
			 * an int of all '1' bits
			 */
#define	_bit_1s		((bit_type)~0)

/* Fast bit string stuff */

#ifdef LIMITCOLORS
#define fbit_n _bit_intsiz
#define fbit_size 1
#else
extern int fbit_n;
extern int fbit_size;
#endif

/*
 * (macros used internally)
 */
	/*
	 * how many int's in a string of N bits?
	 */
#define	bit_size(N) \
	((N / _bit_intsiz) + ((N % _bit_intsiz) ? 1 : 0))

	/*
	 * which int of the string is bit N in?
	 */
#define	_bit_intn(N) \
	((N) / _bit_intsiz)

	/*
	 * mask for bit N in it's int
	 */
#define	_bit_mask(N) \
	(((bit_type)1) << ((N) % _bit_intsiz))


        /*
	 * Return a mask with zeros followed by N ones
	 */
#define bit_zeroone(N) \
    	(_bit_1s >> (_bit_intsiz + N))

/*
 * (macros used externally)
 */
	/*
	 * declare (create) Name as a string of N bits
	 */
#define	bit_decl(Name, N) \
	bit_type Name[bit_size(N)]

	/*
	 * declare (reference) Name as a bit string
	 */
#define	bit_ref(Name) \
	bit_type *Name

	/*
	 * is bit N of string Name set?
	 */
#ifdef LIMITCOLORS
#define bit_test(Name,N) \
	(*(Name) & _bit_mask(N))
#else
#define	bit_test(Name, N) \
	((Name)[_bit_intn(N)] & _bit_mask(N))
#endif

	/*
	 * set bit N of string Name
	 */
#ifdef LIMITCOLORS
#define	bit_set(Name, N) \
	{ *(Name) |= _bit_mask(N); }
#else
#define	bit_set(Name, N) \
	{ (Name)[_bit_intn(N)] |= _bit_mask(N); }
#endif

	/*
	 * clear bit N of string Name
	 */
#ifdef LIMITCOLORS
#define	bit_clear(Name, N) \
	{ *(Name) &= ~_bit_mask(N); }
#else
#define	bit_clear(Name, N) \
	{ (Name)[_bit_intn(N)] &= ~_bit_mask(N); }
#endif

	/*
	 * set bits 0..N in string Name
	 */
#define	bit_setall(Name, N) \
	{	register _bit_i; \
		for (_bit_i = bit_size(N)-1; _bit_i >= 0; _bit_i--) \
			Name[_bit_i]=_bit_1s; \
	}

	/*
	 * set all bits in string Name
	 */
#ifdef LIMITCOLORS
#define	fbit_setall(Name) \
	{ *(Name)= _bit_1s; }
#else
#define	fbit_setall(Name) \
	{	register _bit_i; \
		for (_bit_i = fbit_size-1; _bit_i >= 0; _bit_i--) \
			Name[_bit_i]=_bit_1s; \
	}
#endif

	/*
	 * clear bits 0..N in string Name
	 */
#define	bit_clearall(Name, N) \
	{	register _bit_i; \
		for (_bit_i = bit_size(N)-1; _bit_i >= 0; _bit_i--) \
			Name[_bit_i]=_bit_0s; \
	}

	/*
	 * clear all bits in string Name
	 */
#ifdef LIMITCOLORS
#define	fbit_clearall(Name) \
	{ *(Name)= _bit_0s; }
#else
#define	fbit_clearall(Name) \
	{	register _bit_i; \
		for (_bit_i = fbit_size-1; _bit_i >= 0; _bit_i--) \
			Name[_bit_i]=_bit_0s; \
	}
#endif

	/*
	 * set bit N of string Name to one, and all others to zero
	 */

#ifdef LIMITCOLORS
#define fbit_setonly(Name, N) \
	{ *(Name)= _bit_mask(N); }
#else
#define fbit_setonly(Name, N) \
	{ fbit_clearall(Name); bit_set(Name, N); }
#endif

	/*
	 * Copy bit string
	 */
#define	bit_cpy(Dest, Src, N) \
	{	register _bit_i; \
		for (_bit_i = bit_size(N)-1; _bit_i >= 0; _bit_i--) \
			Dest[_bit_i]= Src[_bit_i]; \
	}

#ifdef LIMITCOLORS
#define	fbit_cpy(Dest, Src) \
	{ *(Dest)= *(Src); }
#else
#define	fbit_cpy(Dest, Src) \
	{	register _bit_i; \
		for (_bit_i = fbit_size-1; _bit_i >= 0; _bit_i--) \
			Dest[_bit_i]= Src[_bit_i]; \
	}
#endif

	/*
	 * OR the source bit string into the destination
	 */
#ifdef LIMITCOLORS
#define	fbit_or(Dest, Src) \
	{ *(Dest)|= *(Src); }
#else
#define	fbit_or(Dest, Src) \
	{	register _bit_i; \
		for (_bit_i = fbit_size-1; _bit_i >= 0; _bit_i--) \
			Dest[_bit_i]|= Src[_bit_i]; \
	}
#endif

#endif
