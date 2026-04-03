#include "tVector.h"

#include <assert.h>
#include <stdio.h>

void vectorInit(vector_t *v, _u8 *buf, int maxSize)
{
	v->maxSize = maxSize;
	v->_buf = buf;
	v->head = 0;
	v->tail = -1;
	v->size = 0;
}

_u8 at(vector_t *v, int index)
{
	if (index >= v->size || index < 0) {
		return (_u8)0xff;
	}
	return v->_buf[(v->head + index) % v->maxSize];
}

bool pop_font(vector_t *v, int numToPop)
{
	if (v->size < numToPop || numToPop <= 0) {
		return false;
	}
	v->head = (v->head + numToPop) % v->maxSize;
	v->size -= numToPop;
	return true;
}

bool pop_back(vector_t *v, int numToPop)
{
	if (v->size < numToPop || numToPop <= 0) {
		return false;
	}
	v->tail = (v->tail - numToPop + v->maxSize) % v->maxSize;
	v->size -= numToPop;
	return true;
}

bool push_font(vector_t *v, _u8 *dataToPush, int sizeToPush)
{
	if (v->size + sizeToPush > v->maxSize || sizeToPush <= 0) {
		return false;
	}
	for (int i = sizeToPush - 1; i >= 0; --i) {
		v->head = (v->head - 1 + v->maxSize) % v->maxSize;
		v->_buf[v->head] = dataToPush[i];
	}
	v->size += sizeToPush;
	return true;
}

bool push_back(vector_t *v, _u8 *dataToPush, int sizeToPush)
{
	if (v->size + sizeToPush > v->maxSize || sizeToPush <= 0) {
		return false;
	}
	for (int i = 0; i < sizeToPush; ++i) {
		v->tail = (v->tail + 1) % v->maxSize;
		v->_buf[v->tail] = dataToPush[i];
	}
	v->size += sizeToPush;
	return true;
}

void vectorPrint(vector_t *v)
{
	printf("{ ");
	for (int i = 0; i < v->size; ++i) {
		printf("0x%02x ", at(v, i));
	}
	printf("}\n");
}

void vectorTest(void)
{
	vector_t v;
	_u8 vBuf[10];
	vectorInit(&v, vBuf, 10);

	_u8 tx1[4] = { 1, 2, 3, 4 };
	push_back(&v, tx1, 4);
	vectorPrint(&v); // { 0x01 0x02 0x03 0x04 }

	_u8 tx2[3] = { 5, 6, 7 };
	push_back(&v, tx2, 3);
	vectorPrint(&v); // { 0x01 0x02 0x03 0x04 0x05 0x06 0x07 }

	_u8 tx3[2] = { 8, 9 };
	push_font(&v, tx3, 2);
	vectorPrint(&v); // { 0x08 0x09 0x01 0x02 0x03 0x04 0x05 0x06 0x07 }

	pop_font(&v, 4);
	vectorPrint(&v); // { 0x03 0x04 0x05 0x06 0x07 }

	pop_back(&v, 4);
	vectorPrint(&v); // { 0x03 }
}
