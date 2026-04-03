#ifndef TVECTOR_H
#define TVECTOR_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_u8)
#undef _u8
typedef volatile uint8_t _u8;
#else
typedef volatile uint8_t _u8;
#endif

typedef struct vector {
	volatile int maxSize;
	volatile int head;
	volatile int tail;
	volatile int size;
	volatile _u8 *_buf;
} vector_t;

void vectorInit(vector_t *v, _u8 *buf, int maxSize);
_u8 at(vector_t *v, int index);
bool pop_font(vector_t *v, int numToPop);
bool pop_back(vector_t *v, int numToPop);
bool push_font(vector_t *v, _u8 *dataToPush, int sizeToPush);
bool push_back(vector_t *v, _u8 *dataToPush, int sizeToPush);
void vectorPrint(vector_t *v);
void vectorTest(void);

#endif
