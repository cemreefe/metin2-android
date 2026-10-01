#ifndef __INC_ETERBASE_RANDOM_H__
#define __INC_ETERBASE_RANDOM_H__

extern void				srandom(unsigned int seed);
extern long			random();
extern float			frandom(float flLow, float flHigh);
extern long				random_range(long from, long to);

#endif