
#ifndef Py_BITSET_H
#define Py_BITSET_H
#ifdef __cplusplus
extern "C" {
#endif

/* Bitset interface */

#include <stdint.h>
#ifndef BYTE
#define BYTE uint8_t
#endif

typedef BYTE *pybitset;

pybitset newbitset(int nbits);
void delbitset(pybitset bs);
#define testbit(ss, ibit) (((ss)[BIT2BYTE(ibit)] & BIT2MASK(ibit)) != 0)
int addbit(pybitset bs, int ibit); /* Returns 0 if already set */
int samebitset(pybitset bs1, pybitset bs2, int nbits);
void mergebitset(pybitset bs1, pybitset bs2, int nbits);

#define BITSPERBYTE	(8*sizeof(BYTE))
#define NBYTES(nbits)	(((nbits) + BITSPERBYTE - 1) / BITSPERBYTE)

#define BIT2BYTE(ibit)	((ibit) / BITSPERBYTE)
#define BIT2SHIFT(ibit)	((ibit) % BITSPERBYTE)
#define BIT2MASK(ibit)	(1 << BIT2SHIFT(ibit))
#define BYTE2BIT(ibyte)	((ibyte) * BITSPERBYTE)

#ifdef __cplusplus
}
#endif
#endif /* !Py_BITSET_H */
