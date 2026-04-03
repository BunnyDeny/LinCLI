#include "tVector.h"

void vectorInit(struct vector *v, _u8 *buf, int buf_size)
{
	v->buf_size = buf_size;
	v->_buf = buf;
	v->head = 0;
	v->tail = -1;
	v->size = 0;
}

_u8 at(struct vector *v, int pos)
{
	if (pos >= v->size || pos < 0) {
		return (_u8)0xff;
	}
	return v->_buf[(v->head + pos) % v->buf_size];
}

bool pop_front(struct vector *v, int n)
{
	if (v->size < n || n <= 0) {
		return false;
	}
	v->head = (v->head + n) % v->buf_size;
	v->size -= n;
	return true;
}

bool pop_back(struct vector *v, int n)
{
	if (v->size < n || n <= 0) {
		return false;
	}
	v->tail = (v->tail - n + v->buf_size) % v->buf_size;
	v->size -= n;
	return true;
}

bool push_font(struct vector *v, _u8 *date, int n)
{
	if (v->size + n > v->buf_size || n <= 0) {
		return false;
	}
	for (int i = n - 1; i >= 0; --i) {
		v->head = (v->head - 1 + v->buf_size) % v->buf_size;
		v->_buf[v->head] = date[i];
	}
	v->size += n;
	return true;
}

bool push_back(struct vector *v, _u8 *date, int n)
{
	if (v->size + n > v->buf_size || n <= 0) {
		return false;
	}
	for (int i = 0; i < n; ++i) {
		v->tail = (v->tail + 1) % v->buf_size;
		v->_buf[v->tail] = date[i];
	}
	v->size += n;
	return true;
}
