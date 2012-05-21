#include "buffer.h"
#include "bitset.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/**
 *用于得到size_t位的大小 n_Bytes * Byte.size CAHR_BIT在limits.h中
 * 定义，其表示一个字符所占多少位，一般为8
 */
#define BITSET_BITS \
	( CHAR_BIT * sizeof(size_t) )

/**
 * 将一个size_t类型的第pos位置位
 */
#define BITSET_MASK(pos) \
	( ((size_t)1) << ((pos) % BITSET_BITS) )
/**
 * 取bitset类型set的pos所在size_t单位
 */
#define BITSET_WORD(set, pos) \
	( (set)->bits[(pos) / BITSET_BITS] )
/**
 * 得到应该分配几个BITSET_BITS块
 */
#define BITSET_USED(nbits) \
	( ((nbits) + (BITSET_BITS - 1)) / BITSET_BITS )

/**
 * 这个不仅初始话，同时也是声明或创造一个含有nbits个位的
 * bitset 的数据结构的set变量
 * 
 * @param nbits 初始话bit的位数
 * 
 * @return  成功时返回bitset对象
 */
bitset *bitset_init(size_t nbits) {
	bitset *set;

	set = malloc(sizeof(*set));
	assert(set);
/* -> 的优先级高于*,所以*set->bits表示set的bits成员所指的对象  */
	set->bits = calloc(BITSET_USED(nbits), sizeof(*set->bits));
	set->nbits = nbits;

	assert(set->bits);

	return set;
}

/** 
 * 将bitset型变量set里bits所指内容全部清零，
 * 并没有释放内存
 * 
 * @param set 要清零的对象
 */
void bitset_reset(bitset *set) {
	memset(set->bits, 0, BITSET_USED(set->nbits) * sizeof(*set->bits));
}

/** 
 * 释放一个bitset型变量set所用内存
 *
 * @param 要释放内存的对象
 */
void bitset_free(bitset *set) {
	free(set->bits);
	free(set);
}

/**
 * 将bitset类型的set变量中的bits的第pos位清零
 * 
 * @param set 要操作的对象
 * @param pos 第pos位
 */
void bitset_clear_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	/* 先取出pos所在的size_t单位，然后将其与一个size_t进行与，可修改该位 */
	BITSET_WORD(set, pos) &= ~BITSET_MASK(pos);
}

/**
 * 将bitset类型的set变量中的bits的第pos位置位
 * 
 * @param set 要操作的对象
 * @param pos 要操作的第pos位
 */
void bitset_set_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	BITSET_WORD(set, pos) |= BITSET_MASK(pos);
}

/** 
 * 判断bitset类型变量set中bits的第pos位是否为1
 *
 * @param set 要操作的对象
 * @param pos 要检测的第pos位
 *
 * @return  为1时返回1，否则返回0
 */
int bitset_test_bit(bitset *set, size_t pos) {
	if (pos >= set->nbits) {
	    SEGFAULT();
	}

	return (BITSET_WORD(set, pos) & BITSET_MASK(pos)) != 0;
}
