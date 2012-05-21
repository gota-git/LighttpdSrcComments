#ifndef _BITSET_H_
#define _BITSET_H_

#include <stddef.h>
/* setdef.h 里面定义了size_t */

typedef struct {
	size_t *bits;
	size_t nbits;
} bitset;

bitset *bitset_init(size_t nbits); /* 定义一个nbits大小的位集合对象 */
void bitset_reset(bitset *set);    /* 将一个位集合对像清零 */
void bitset_free(bitset *set);	   /* 释放一个位集合对象所占内存 */

void bitset_clear_bit(bitset *set, size_t pos);  /* 将一个位集合对象的某位清零 */
void bitset_set_bit(bitset *set, size_t pos); 	 /* 将一个位集合对象的某位置位 */
int bitset_test_bit(bitset *set, size_t pos);	 /* 测试一个位集合对象某位是否置位 */

#endif
