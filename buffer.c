
/**
 * 记住一条:buffer用ptr存放具体内容
 * used 和size 分别存放已用长度和总长度
 */

#include "buffer.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <assert.h>
#include <ctype.h>



static const char hex_chars[] = "0123456789abcdef";


/**
 * init the buffer
 * lihttpd的特色吧，采用返回式的初始化，不想kernel有多种
 * 初始化方式，初始话代码比较直观
 */


buffer* buffer_init(void) {
	buffer *b;

	b = malloc(sizeof(*b));
	assert(b);

	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}

/**
 * 相当于C++里面的赋值初始化
 * src为初值
 */
buffer *buffer_init_buffer(buffer *src) {
	buffer *b = buffer_init();
	buffer_copy_string_buffer(b, src);
	return b;
}

/**
 * free the buffer
 *
 */

void buffer_free(buffer *b) {
	if (!b) return; /* 不报错？ */

	free(b->ptr);
	free(b);
}

/**
 * 重置buffer,置buffer->used=0,buffer->ptr不动，
 * 仅将首字节置'/0'
 * 如果buffer->size>最大值
 * 那么将释放buffer->ptr,并置结构至buffer_init状态
 */
void buffer_reset(buffer *b) {
	if (!b) return;

	/* limit don't reuse buffer larger than ... bytes */
	if (b->size > BUFFER_MAX_REUSE_SIZE) {
		free(b->ptr);
		b->ptr = NULL;
		b->size = 0;
	} else if (b->size) {
		b->ptr[0] = '\0';
	}

	b->used = 0;
}


/**
 * 调整b的大小
 * 如果b->size够大则不做调整，否则调整b->size至
 * size大小，同时为b->ptr分配足够的大小
 *
 * allocate (if neccessary) enough space for 'size' bytes and
 * set the 'used' counter to 0
 *
 */

#define BUFFER_PIECE_SIZE 64

int buffer_prepare_copy(buffer *b, size_t size) {
	if (!b) return -1;

	if ((0 == b->size) ||
	    (size > b->size)) {
		if (b->size) free(b->ptr);

		b->size = size;

		/* 分配BUFFER_PICE_SIZE整数倍。always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		assert(b->ptr);
	}
	b->used = 0;
	return 0;
}

/**
 * 调整b的大小符合将要用的需求
 * 如果b->size ==0 则为b分配size大小
 * 否则判断b->size是否能满足新增size大小后的b->used+size大小的需求
 * 如果不够的的话，用realloc进行重新分配
 *
 * increase the internal buffer (if neccessary) to append another 'size' byte
 * ->used isn't changed
 *
 */

int buffer_prepare_append(buffer *b, size_t size) {
	if (!b) return -1;

	if (0 == b->size) {
		b->size = size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = malloc(b->size);
		b->used = 0;
		assert(b->ptr);
	} else if (b->used + size > b->size) { /* 将要使用的大小 */
		b->size += size;

		/* always allocate a multiply of BUFFER_PIECE_SIZE */
		b->size += BUFFER_PIECE_SIZE - (b->size % BUFFER_PIECE_SIZE);

		b->ptr = realloc(b->ptr, b->size);
		assert(b->ptr);
	}
	return 0;
}

/** 
 * 将字符串s复制到b->prt
 * 首先用buffer_prepare_copy为b调整大小
 * 然后memcpy进行复制，并设置b->used大小
 */
int buffer_copy_string(buffer *b, const char *s) {
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s) + 1; /* 因为strlen()结果不包括'/0'在内 */
	buffer_prepare_copy(b, s_len);

	memcpy(b->ptr, s, s_len);
	b->used = s_len;

	return 0;
}

/** 
 * 将字符串s的前s_len个字符复制到b->prt
 * 首先用buffer_prepare_copy为b调整大小
 * 然后memcpy进行复制,并在字符串尾部加上'/0'，
 * 同时设置b->used大小,包括字符串尾部的'/0'
 */
int buffer_copy_string_len(buffer *b, const char *s, size_t s_len) {
	if (!s || !b) return -1;
#if 0
	/* removed optimization as we have to keep the empty string
	 * in some cases for the config handling
	 *
	 * url.access-deny = ( "" )
	 */
	if (s_len == 0) return 0;
#endif
	buffer_prepare_copy(b, s_len + 1);

	memcpy(b->ptr, s, s_len);
	b->ptr[s_len] = '\0';
	b->used = s_len + 1;

	return 0;
}

/**
 * buffer类型对象的复制
 * 用src对象复制一个b对象
 * src->used=0时，不保证size大小相同
 */
int buffer_copy_string_buffer(buffer *b, const buffer *src) {
	if (!src) return -1;

	if (src->used == 0) {
		buffer_reset(b); /* src->used=0时，不保证size大小相同 */
		return 0;
	}
	/**
	 * 此时已经表明源是字符串，所有不复制其结尾的'/0'
	 */
	return buffer_copy_string_len(b, src->ptr, src->used - 1);  
}

/**
 * 在b中增加字符串s
 * 将字符串s附加到b->ptr的结尾，覆盖已有'/0'，组成新的字符串
 */
int buffer_append_string(buffer *b, const char *s) {
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s);
	buffer_prepare_append(b, s_len + 1);
	if (b->used == 0)
		b->used++;

	/** 
	 * 此时表明操作的是字符串，所有要覆盖b中的已有的'/0'
	 * 同时strlen+1将*的末尾的'/0'复制过来
	 */
	memcpy(b->ptr + b->used - 1, s, s_len + 1); 
	b->used += s_len;

	return 0;
}

/**
 * 将字符串s追加到b中
 * 如果s的长度大于malen则只追加其前maxlen个字符
 * 但是在分配空间的时候，会为其准备maxlen个字符
 * 此时，未用的将填充为' '。其实蛮奇怪这里为什么这么做
 */
int buffer_append_string_rfill(buffer *b, const char *s, size_t maxlen) {
	size_t s_len;

	if (!s || !b) return -1;

	s_len = strlen(s);
	if (s_len > maxlen)  s_len = maxlen;
	buffer_prepare_append(b, maxlen + 1);
	if (b->used == 0)
		b->used++; /* 构建原有的'/0'占位符  */

	memcpy(b->ptr + b->used - 1, s, s_len);
	if (maxlen > s_len) {
		memset(b->ptr + b->used - 1 + s_len, ' ', maxlen - s_len);
	}

	b->used += maxlen;
	b->ptr[b->used - 1] = '\0';
	return 0;
}

/**
 * 将一个字符串追加到一个buffer上面
 *
 * 将字符串s的前s_len个字符追加到buffer
 * 对象b的 b->ptr上面，并以'/0'结尾
 * 同时覆盖原b中最后一个字符'/0'
 *
 * @param b 要追加到的buffer对象
 * @param s 追加的字符串
 * @param s_len 字符串s中要追加的字符数
 * 
 * @return 成功返回0，否则返回-1
 *
 * append a string to the end of the buffer
 *
 * the resulting buffer is terminated with a '\0'
 * s is treated as a un-terminated string (a \0 is handled a normal character)
 *
 * @param b a buffer
 * @param s the string
 * @param s_len size of the string (without the terminating \0)
 */

int buffer_append_string_len(buffer *b, const char *s, size_t s_len) {
	if (!s || !b) return -1;
	if (s_len == 0) return 0;

	buffer_prepare_append(b, s_len + 1);
	if (b->used == 0)
		b->used++;

	memcpy(b->ptr + b->used - 1, s, s_len);
	b->used += s_len;
	b->ptr[b->used - 1] = '\0';

	return 0;
}

/**
 * 在一个buffer对象后面增加另个buffer对象
 *
 * 将buffer对象src中的字符串追加到buffer对象b上
 * 构成新的字符串
 *
 * @param b 追加到的buffer对象
 * @param src 要追加的源buffer对象
 *
 * @return 成功返回0，否则返回-1
 */

int buffer_append_string_buffer(buffer *b, const buffer *src) {
	if (!src) return -1;
	if (src->used == 0) return 0;

	return buffer_append_string_len(b, src->ptr, src->used - 1);
}

/**
 * 在一个buffer对象后面追加一个字符串
 * 
 * 在buffer对象b后面追加字符串s的前s_len个字符，
 * 此时将b当作一块内存看待而不是像buffer_append_string
 * 中当作字符串，因此不消除 b中最后一个字符
 *
 * 奇怪的是怎么不做是否超过字符串大小的检查
 * 以及最后位‘/0'的检查，如果出错怎么办
 * 而且该函数和buffer_append_string的参数格式也不对称
 * 难道是有特殊的意义？
 *
 * @param b 被追加的对象
 * @param s 要追加的字符串
 * @param s_len 要追加字符串s的前s_len个字符
 * 
 * @return 成功返回0，否则返回-1
 */

int buffer_append_memory(buffer *b, const char *s, size_t s_len) {
	if (!s || !b) return -1;
	if (s_len == 0) return 0;

	buffer_prepare_append(b, s_len);
	memcpy(b->ptr + b->used, s, s_len);
	b->used += s_len;

	return 0;
}

/**
 * 将字符串内容拷贝到buffer对象
 * 
 * 将字符串s的前s_len个字符内容拷贝到buffer对象b中，
 * 这里不将b最为字符串对待，可能操作完后b的结尾不是
 * 以'/0'结尾，即不一定能当字符串对待
 * 
 * 和上面一样，怎么不做检查
 * 
 * @param b 要追加到ffer对象
 * @param s 要追加的字符串
 * @param s_len 要追加的字符长度
 *
 * @return 成功返回0，否则返回-1
 */

int buffer_copy_memory(buffer *b, const char *s, size_t s_len) {
	if (!s || !b) return -1;

	b->used = 0;

	return buffer_append_memory(b, s, s_len);
}

/** 
 * 将一个unsigned long 型的16进制数追加到buffer对象b上
 * 
 * 将16进制数value，按字面值追加到buffer对象b上，此时将b当作字符串
 * 会覆盖其末尾的'\0'，同时产生的新字符串以'\0'结尾。但是不以'0x'开始
 * 程序中采用了一个将整数转换成16进制数字面字符值的算法，
 * 值得学习
 *
 * 该函数总是返回成功，为什么？
 *
 * @param b 要追加到的buffer对象
 * @param value 16进制数
 * 
 * @return 总是成功，返回0
 */

int buffer_append_long_hex(buffer *b, unsigned long value) {
	char *buf;
	int shift = 0;
	unsigned long copy = value;

	while (copy) {
	/**
	 * 得到16进制数的位数
	 */
		copy >>= 4;
		shift++;
	}
	if (shift == 0)
		shift++;
	if (shift & 0x01)
	/* 位数为奇数的话，加一成偶数 保证输出16进制是偶数位,如0x02*/
		shift++;

	buffer_prepare_append(b, shift + 1);
	if (b->used == 0)
		b->used++;
	buf = b->ptr + (b->used - 1);
	b->used += shift;

	shift <<= 2; /* 将shift值乘以2的2次方，即将其扩展为bit数目 */
	while (shift > 0) {
	/**
	 * 让value16进制数右移去掉一位，通过与操作取得高位数
	 * 在hex_chars中的索引
	 */
		shift -= 4;
		*(buf++) = hex_chars[(value >> shift) & 0x0F];
	}
	*buf = '\0';

	return 0;
}

/**
 * 将一个长整形数转换成字符串
 *
 * 将一个长整形数从个位依次到高位取出放在
 * 字符串buf中，然后做顺序颠倒，完成任务
 * 用户需负责buf内存的分配
 *
 * @param buf 结构字符串
 * @param val 整形数
 * 
 * @return  返回字符串的长度
 */
int LI_ltostr(char *buf, long val) {
	char swap;
	char *end;
	int len = 1;

	if (val < 0) {
	/**
	 * 处理负数
	 */
		len++;
		*(buf++) = '-';
		val = -val;
	}

	end = buf;
	while (val > 9) {
	/** 
	 * 将对应位上的值转换成对应的字符字面值
	 * 从个位开始。最高位未处理
	 */
		*(end++) = '0' + (val % 10);
		val = val / 10;
	}
	*(end) = '0' + val;
	*(end + 1) = '\0';
	len += end - buf;

	while (buf < end) {
	/**
	 * 由于之前先填写的个位，这里要把高位和地位顺序
	 * 调换
	 */
		swap = *end;
		*end = *buf;
		*buf = swap;

		buf++;
		end--;
	}

	return len;
}

/**
 * 给buffer对象b增加整形数val
 * 
 * 将long对象val转换成字符串后，追加到
 * buffer对象b上
 * 
 * @param b 要追加到的buffer对象
 * @param val 整形数
 * 
 * @return 成功返回0，否则返回-1
 */
int buffer_append_long(buffer *b, long val) {
	if (!b) return -1;

	buffer_prepare_append(b, 32);
	if (b->used == 0)
		b->used++;

	b->used += LI_ltostr(b->ptr + (b->used - 1), val);
	return 0;
}

/**
 * 复制整形数到buffer对象
 *
 * 将整形数val转换成字符串后复制到buffer对象
 * b中
 *
 * @param b 要复制到的buffer对象
 * @val 整形数
 * 
 * @return 成功返回0，否则返回-1
 */
int buffer_copy_long(buffer *b, long val) {
	if (!b) return -1;

	b->used = 0;
	return buffer_append_long(b, val);
}

/**
 * 将off_t类型添加到buffer对象
 * 
 * 将off_t对象val转换成字符串并添加到buffer对象b上
 * 此时b当字符串对待，覆盖其最后的'\0'
 *
 * @param b 要追缴到的buffer对象
 * @param val off_t对象
 * 
 * return  成功返回0，否则返回-1
 */
#if !defined(SIZEOF_LONG) || (SIZEOF_LONG != SIZEOF_OFF_T)
int buffer_append_off_t(buffer *b, off_t val) {
	char swap;
	char *end;
	char *start;
	int len = 1;

	if (!b) return -1;

	buffer_prepare_append(b, 32);
	if (b->used == 0)
		b->used++;

	start = b->ptr + (b->used - 1);
	if (val < 0) {
		len++;
		*(start++) = '-';
		val = -val;
	}

	end = start;
	while (val > 9) {
		*(end++) = '0' + (val % 10);
		val = val / 10;
	}
	*(end) = '0' + val;
	*(end + 1) = '\0';
	len += end - start;

	while (start < end) {
		swap   = *end;
		*end   = *start;
		*start = swap;

		start++;
		end--;
	}

	b->used += len;
	return 0;
}

/**
 * 将off_t对象拷贝到buffer对象
 *
 * 将off_t对象val 复制到buffer对象b中，内存大小
 * 的控制有buffer_append_off_t调整。
 *
 * @param b 要复制到的buffer对象
 * @param val 要赋值的off_t对象
 *
 * @return 成功返回0，否则返回-1
 */
int buffer_copy_off_t(buffer *b, off_t val) {
	if (!b) return -1;

	b->used = 0;
	return buffer_append_off_t(b, val);
}
#endif /* !defined(SIZEOF_LONG) || (SIZEOF_LONG != SIZEOF_OFF_T) */
/**
 * 将8位整形数转换成16进制数对应的字面值
 */
char int2hex(char c) {
	return hex_chars[(c & 0x0F)];
}

/**
 * 将一个十六进制字面值转换成整形数
 * 
 * @return 返回一个8位的字符型数，
 * @return 非法输入时返回0xFF
 *
 * converts hex char (0-9, A-Z, a-z) to decimal.
 * returns 0xFF on invalid input.
 */
char hex2int(unsigned char hex) {
	hex = hex - '0';
	if (hex > 9) {
		hex = (hex + '0' - 1) | 0x20;
		hex = hex - 'a' + 11;
	}
	if (hex > 15)
		hex = 0xFF;

	return hex;
}


/**
 * init the buffer
 *
 */

/**
 * 初始话buffer对象数组
 *
 * @return 返回buffer_array * 指针对象
 */

buffer_array* buffer_array_init(void) {
	buffer_array *b;

	b = malloc(sizeof(*b));

	assert(b);
	b->ptr = NULL;
	b->size = 0;
	b->used = 0;

	return b;
}

/**
 * 重置buffer_array 对象
 *
 * 使用buffer_reset重置bufrer_array对象里的所有成员
 *
 */
void buffer_array_reset(buffer_array *b) {
	size_t i;

	if (!b) return;

	/* if they are too large, reduce them */
	for (i = 0; i < b->used; i++) {
		buffer_reset(b->ptr[i]);
	}

	b->used = 0;
}


/**
 * free the buffer_array
 *
 */

void buffer_array_free(buffer_array *b) {
	size_t i;
	if (!b) return;

	for (i = 0; i < b->size; i++) {
		if (b->ptr[i]) buffer_free(b->ptr[i]);
	}
	free(b->ptr);
	free(b);
}

/**
 * 在buffer_array对象中得到下一个可以使用的buffer位置
 *
 * 在buffer_array对象中寻找下一个可以使用的buffer位置，
 * 如果空间不够，则一次性分配16个单位，如果够，则初始化
 * 该位置空间，并返回指向其的指针
 *
 * @param b buffer_arrya对象b
 * 
 * return 返回下一个可用 的buffer对象位置。
 */
buffer *buffer_array_append_get_buffer(buffer_array *b) {
	size_t i;

	if (b->size == 0) {
	/* 对空数组进行特殊处理 */
		b->size = 16;
		b->ptr = malloc(sizeof(*b->ptr) * b->size);
		assert(b->ptr);
		for (i = 0; i < b->size; i++) {
		/**
		 * 将新分配的空间全部置空
		 */
			b->ptr[i] = NULL;
		}
	} else if (b->size == b->used) {
		b->size += 16;
		b->ptr = realloc(b->ptr, sizeof(*b->ptr) * b->size);
		assert(b->ptr);
		for (i = b->used; i < b->size; i++) {
			b->ptr[i] = NULL;
		}
	}

	if (b->ptr[b->used] == NULL) {
		b->ptr[b->used] = buffer_init();
	}

	b->ptr[b->used]->used = 0;

	return b->ptr[b->used++];
}

/**
 * 在buffer对象中搜匹配字符串前len个字符的位置
 *
 * 在buffer对象b中寻找第一个匹配字符串needle前
 * len个字符的首位置，并返回该位置
 * 如果len为0，或者目标字符串为空，或者b里的字符数目小于
 * 目标搜索的字符数，均返回NULL表示失败
 *
 * @param b 要搜索的buffer对象
 * @param needle 要搜索的目标字符串
 * @param len 目标字符串的长度
 *
 * @return 成功时返回匹配的首字符位置否则返回NULL
 */
char * buffer_search_string_len(buffer *b, const char *needle, size_t len) {
	size_t i;
	if (len == 0) return NULL;
	if (needle == NULL) return NULL;

	if (b->used < len) return NULL;

	for(i = 0; i < b->used - len; i++) {
		if (0 == memcmp(b->ptr + i, needle, len)) {
			return b->ptr + i;
		}
	}

	return NULL;
}
 
/**
 * 用字符串初始话buffer对象
 * 
 * 用字符串str初始话buffer对象，将其作为字符串使用
 *
 * @param str 用于初始话的字符串
 * 
 * @return 返回初始话后的buffer对象
 */

buffer *buffer_init_string(const char *str) {
	buffer *b = buffer_init();

	buffer_copy_string(b, str);

	return b;
}
/**
 * 判断buffer对象是否为空
 */
int buffer_is_empty(buffer *b) {
	if (!b) return 1;
	return (b->used == 0);
}

/**
 * check if two buffer contain the same data
 *
 * HISTORY: this function was pretty much optimized, but didn't handled
 * alignment properly.
 */

/**
 * 比较两个buffer对象内包含的内容是否一样
 * 
 * 比较两个buffer对象里ptr所指向内容是否一致
 * 并没有比较used和size属性
 * 
 * @param a 参与比较的buffer对象之一
 * @param b 参与比较的buffer对象之一
 * 
 * @return 相等则返回1，否则返回0
 */
int buffer_is_equal(buffer *a, buffer *b) {
	if (a->used != b->used) return 0;
	if (a->used == 0) return 1;

	return (0 == strcmp(a->ptr, b->ptr));
}

/**
 * 判断buffer对象内容是否和字符串相等
 * 
 * 用已有字符串构造一个临时的buffer对象，然后和
 * 需要比较的对象进行比较，判断
 * 
 * 很奇怪，为什么要构造一个新的对象，直接strcmp不是很直接么？
 * 而且需要用户维护字符串的长度和'\0'的位置，怎么
 * 自己不做个检查？
 * 
 * @param a 要比较的buffer对象
 * @param s 要比较的字符串
 * @param b_len 要比较的字符串的长度
 * 
 * @return 相等则返回1，否则返回0
 */
int buffer_is_equal_string(buffer *a, const char *s, size_t b_len) {
	buffer b;

	b.ptr = (char *)s;
	b.used = b_len + 1;

	return buffer_is_equal(a, &b);
}

/* simple-assumption:
 *
 * most parts are equal and doing a case conversion needs time
 *
 */

/**
 * 忽略大小写比较两个字符串的大小
 * 
 * 忽略大小写比较两个字符串的大小关系，比较的规则按C语言方式进行
 * 比较，返回0，负数，整数。字符串的长度有相应的参数给出
 * 程序首先通过判断两个字符串位置是否按size_t对齐，对齐的话则进行加速
 * 处理，但是比较过程没有做大小写相等处理
 * 对于不想等的部分以及不对齐的比较，则按字符逐个进行比较，其比较过程
 * 做了大小写相等性处理，使得忽略大小写比较
 * 
 * @param a 参与比较的第一个字符串
 * @param a_len 第一个参与字符串的长度
 * @param b 参与比较的第二个字符串
 * @param b_len 第二个参与字符串的长度
 *
 * @return 相等则返回0，否则按C中比较方式，进行字符串比较。小于则为负数，否则为正数
 */
int buffer_caseless_compare(const char *a, size_t a_len, const char *b, size_t b_len) {
	size_t ndx = 0, max_ndx;
	size_t *al, *bl;
	size_t mask = sizeof(*al) - 1; /* 这样，其后续几个bit就为1，做&运算相应的0会使其结果为0 */

	al = (size_t *)a;
	bl = (size_t *)b;

	/* is the alignment correct ? */
	/* 非常漂亮的用对齐做了一个加速过程 */
	if ( ((size_t)al & mask) == 0 &&
	     ((size_t)bl & mask) == 0 ) { /* 对齐为其地址为sizeof(*al)的倍数 */

		/* 取较小的长度并使其向下取整为掩码原体的倍数 */
		max_ndx = ((a_len < b_len) ? a_len : b_len) & ~mask;

		for (; ndx < max_ndx; ndx += sizeof(*al)) {
		/* 没有对大小写进行相等性处理 */
			if (*al != *bl) break;
			al++; bl++;

		}

	}

	a = (char *)al;
	b = (char *)bl;

	max_ndx = ((a_len < b_len) ? a_len : b_len);
	

	for (; ndx < max_ndx; ndx++) {
	/**
	 * 一个字符一个字符的比较
	 */
		char a1 = *a++, b1 = *b++;

		if (a1 != b1) {
			if ((a1 >= 'A' && a1 <= 'Z') && (b1 >= 'a' && b1 <= 'z'))
				a1 |= 32;
			else if ((a1 >= 'a' && a1 <= 'z') && (b1 >= 'A' && b1 <= 'Z'))
				b1 |= 32;
			if ((a1 - b1) != 0) return (a1 - b1);

		}
	}

	/* all chars are the same, and the length match too
	 *
	 * they are the same */
	if (a_len == b_len) return 0;

	/* if a is shorter then b, then b is larger */
	return (a_len - b_len);
}


/**
 * check if the rightmost bytes of the string are equal.
 */


/**
 * 比较两个字符串前len个字符是否相等 
 *
 * 比较两个字符串前len个字符是否相等，len为0时，返回1，表示不等
 * 两个的used都小于len的返回1表示不等。
 * 
 * 不明白，如果两个相等但是都小于len 的话怎么办？没有比较？
 *
 * @param b1 第一个参与比较的buffer对象
 * @param b2 第二个参与比较的buffer对象
 * @param len 参与比较的前len个字符
 *
 * @return  相等返回0，否则返回1
 */
int buffer_is_equal_right_len(buffer *b1, buffer *b2, size_t len) {
	/* no, len -> equal */
	if (len == 0) return 1;

	/* len > 0, but empty buffers -> not equal */
	if (b1->used == 0 || b2->used == 0) return 0;

	/* buffers too small -> not equal */
	if (b1->used - 1 < len || b1->used - 1 < len) return 0;

	if (0 == strncmp(b1->ptr + b1->used - 1 - len,
			 b2->ptr + b2->used - 1 - len, len)) {
		return 1;
	}

	return 0;
}

/**
 * 将字符串前in_len个字符表示成16进制数字面值追加到buffer对象上
 *
 * 将一个字符串中的前in_len个字符中每个8bit的字符表示成相应的两位16进制
 * 数，并追加到buffer对象b上，最后以'\0'结束表示新的字符
 * 串
 * 
 * 实在是不理解BO测试表示的什么意思
 *
 * @param b 要追加到的buffer对象
 * @param in 目的字符串
 * @param in_len 字符串的前in_len个字符
 *
 * @return  成功则返回0，否则返回-1
 */
int buffer_copy_string_hex(buffer *b, const char *in, size_t in_len) {
	size_t i;

	/* BO protection */
	if (in_len * 2 < in_len) return -1;

	buffer_prepare_copy(b, in_len * 2 + 1);

	for (i = 0; i < in_len; i++) {
		b->ptr[b->used++] = hex_chars[(in[i] >> 4) & 0x0F];
		b->ptr[b->used++] = hex_chars[in[i] & 0x0F];
	}
	b->ptr[b->used++] = '\0';

	return 0;
}

/* everything except: ! ( ) * - . 0-9 A-Z _ a-z */
const char encoded_chars_rel_uri_part[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,  /*  20 -  2F space " # $ % & ' + , / */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; < = > ? */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F @ */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  50 -  5F [ \ ] ^ */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F ` */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  /*  70 -  7F { | } ~ DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

/* everything except: ! ( ) * - . / 0-9 A-Z _ a-z */
const char encoded_chars_rel_uri[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,  /*  20 -  2F space " # $ % & ' + , */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  /*  30 -  3F : ; < = > ? */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F @ */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  /*  50 -  5F [ \ ] ^ */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F ` */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  /*  70 -  7F { | } ~ DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_html[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_minimal_xml[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F & */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  /*  30 -  3F < > */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  /*  70 -  7F DEL */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};

const char encoded_chars_hex[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  00 -  0F control chars */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  10 -  1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  20 -  2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  30 -  3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  40 -  4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  50 -  5F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  60 -  6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  70 -  7F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  80 -  8F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  90 -  9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  A0 -  AF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  B0 -  BF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  C0 -  CF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  D0 -  DF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  E0 -  EF */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /*  F0 -  FF */
};

const char encoded_chars_http_header[] = {
	/*
	0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,  /*  00 -  0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  10 -  1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  20 -  2F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  30 -  3F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  40 -  4F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  50 -  5F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  60 -  6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  70 -  7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  80 -  8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  90 -  9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  A0 -  AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  B0 -  BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  C0 -  CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  D0 -  DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  E0 -  EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /*  F0 -  FF */
};


/**
 * 根据提供的编码格式(buffer.h )对字符串进行编码，并添加到buffer对象
 *
 *
 */
int buffer_append_string_encoded(buffer *b, const char *s, size_t s_len, buffer_encoding_t encoding) {
	unsigned char *ds, *d;
	size_t d_len, ndx;
	const char *map = NULL;

	if (!s || !b) return -1;

	if (b->ptr[b->used - 1] != '\0') {
		SEGFAULT();
	} /* b不表示字符串时报段错误 */

	if (s_len == 0) return 0;

	switch(encoding) {
	/* 选择编码表 */
	case ENCODING_REL_URI:
		map = encoded_chars_rel_uri;
		break;
	case ENCODING_REL_URI_PART:
		map = encoded_chars_rel_uri_part;
		break;
	case ENCODING_HTML:
		map = encoded_chars_html;
		break;
	case ENCODING_MINIMAL_XML:
		map = encoded_chars_minimal_xml;
		break;
	case ENCODING_HEX:
		map = encoded_chars_hex;
		break;
	case ENCODING_HTTP_HEADER:
		map = encoded_chars_http_header;
		break;
	case ENCODING_UNSET:
		break;
	}

	assert(map != NULL);

	/* count to-be-encoded-characters */
	for (ds = (unsigned char *)s, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++) {
	/**
	 * 根据每个字符所在的位置，判断其是否需要进行编码，
	 * 并计算编码所需要的总长度
	 */
		if (map[*ds]) {
			switch(encoding) {
			case ENCODING_REL_URI:
			case ENCODING_REL_URI_PART:
				d_len += 3;
				break;
			case ENCODING_HTML:
			case ENCODING_MINIMAL_XML:
				d_len += 6;
				break;
			case ENCODING_HTTP_HEADER:
			case ENCODING_HEX:
				d_len += 2;
				break;
			case ENCODING_UNSET:
				break;
			}
		} else {
			d_len ++;
		}
	}

	buffer_prepare_append(b, d_len);

	for (ds = (unsigned char *)s, d = (unsigned char *)b->ptr + b->used - 1, d_len = 0, ndx = 0; ndx < s_len; ds++, ndx++) {
	/** 
	 * 根据采用的编码格式对字符串进行编码，编码为相应的符号位加上该字符的两位16进制数表示位。
	 * 并添加到b的后面
	 */
		if (map[*ds]) {
			switch(encoding) {
			case ENCODING_REL_URI:
			case ENCODING_REL_URI_PART:
				d[d_len++] = '%';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTML:
			case ENCODING_MINIMAL_XML:
				d[d_len++] = '&';
				d[d_len++] = '#';
				d[d_len++] = 'x';
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				d[d_len++] = ';';
				break;
			case ENCODING_HEX:
				d[d_len++] = hex_chars[((*ds) >> 4) & 0x0F];
				d[d_len++] = hex_chars[(*ds) & 0x0F];
				break;
			case ENCODING_HTTP_HEADER:
				d[d_len++] = *ds;
				d[d_len++] = '\t';
				break;
			case ENCODING_UNSET:
				break;
			}
		} else {
			d[d_len++] = *ds;
		}
	}

	/* terminate buffer and calculate new length */
	b->ptr[b->used + d_len - 1] = '\0';

	b->used += d_len;

	return 0;
}


/* decodes url-special-chars inplace.
 * replaces non-printable characters with '_'
 */
/**
 * 
 * 
 * 表示没有看懂
 */

static int buffer_urldecode_internal(buffer *url, int is_query) {
	unsigned char high, low;
	const char *src;
	char *dst;

	if (!url || !url->ptr) return -1;

	src = (const char*) url->ptr;
	dst = (char*) url->ptr;

	while ((*src) != '\0') {
		if (is_query && *src == '+') {
			*dst = ' ';
		} else if (*src == '%') {
			*dst = '%';

			high = hex2int(*(src + 1));
			if (high != 0xFF) {
				low = hex2int(*(src + 2));
				if (low != 0xFF) {
					high = (high << 4) | low;

					/* map control-characters out */
					if (high < 32 || high == 127) high = '_';

					*dst = high;
					src += 2;
				}
			}
		} else {
			*dst = *src;
		}

		dst++;
		src++;
	}

	*dst = '\0';
	url->used = (dst - url->ptr) + 1;

	return 0;
}

int buffer_urldecode_path(buffer *url) {
	return buffer_urldecode_internal(url, 0);
}

int buffer_urldecode_query(buffer *url) {
	return buffer_urldecode_internal(url, 1);
}

/* Remove "/../", "//", "/./" parts from path.
 *
 * /blah/..         gets  /
 * /blah/../foo     gets  /foo
 * /abc/./xyz       gets  /abc/xyz
 * /abc//xyz        gets  /abc/xyz
 *
 * NOTE: src and dest can point to the same buffer, in which case,
 *       the operation is performed in-place.
 */
/** 
 * 简化目录
 * 
 * 将../以及./表示的相对路径转换成简单的绝对路径
 * 具体的完整的思想我没有完全理解，有理解的童鞋可以分享下
 * email:gotawork@163.com
 */

int buffer_path_simplify(buffer *dest, buffer *src)
{
	int toklen; /* 已经处理长度 */
	char c, pre1;
	char *start, *slash, *walk, *out;
	unsigned short pre; /* Linux 3.0.x 下unsigned short为16bit */

	if (src == NULL || src->ptr == NULL || dest == NULL)
		return -1;

	if (src == dest)
	/* 原地处理 */
		buffer_prepare_append(dest, 1);
	else
		buffer_prepare_copy(dest, src->used + 1);

	walk  = src->ptr;
	start = dest->ptr;  /* 目的位置的其实处 */
	out   = dest->ptr;  /* 输出到目的地址的当期处 */
	slash = dest->ptr;


#if defined(__WIN32) || defined(__CYGWIN__)
	/* cygwin is treating \ and / the same, so we have to that too
	 */
/**
 * 将win下的'\\'转换成'/'
 */

	for (walk = src->ptr; *walk; walk++) {
		if (*walk == '\\') *walk = '/';
	}
	walk = src->ptr;
#endif

	while (*walk == ' ') {
	/* 跳过前导空白 */
		walk++;
	}

	/**
	 * 如果不以'/'开头，则为其添加'/'
	 */
	pre1 = *(walk++);
	c    = *(walk++);
	pre  = pre1;
	if (pre1 != '/') {
		pre = ('/' << 8) | pre1;
		*(out++) = '/';
	}
	*(out++) = pre1;

	if (pre1 == '\0') {
		dest->used = (out - start) + 1;
		return 0;
	}

	while (1) {
		if (c == '/' || c == '\0') { 
		/* 只在字符串结束或者以'/'开头的字串，才有可能要简化 */
			toklen = out - slash;
			if (toklen == 3 && pre == (('.' << 8) | '.')) {
				out = slash;   /* 先消去".." */
				if (out > start) {
				/* 再回到父目录 */
					out--;
					while (out > start && *out != '/') {
						out--;
					}
				}

				if (c == '\0')
					out++;
			} else if (toklen == 1 || pre == (('/' << 8) | '.')) {
				out = slash; /* 直接消去"."即可 */
				if (c == '\0')
					out++;
			}

			slash = out;
		}

		if (c == '\0')
			break;

		pre1 = c;
		pre  = (pre << 8) | pre1;
		c    = *walk;
		*out = pre1;

		out++;
		walk++;
	}

	*out = '\0';
	dest->used = (out - start) + 1;

	return 0;
}

/**
 * 判断int c是否为字符'0'到‘9’
 *
 * @return  是则返回1.否则返回0
 */
int light_isdigit(int c) {
	return (c >= '0' && c <= '9');
}

/**
 * 判断字符是否是16进制字符字面值
 *
 * @return  是则返回1.否则返回0
 */
int light_isxdigit(int c) {
	if (light_isdigit(c)) return 1;

	c |= 32;
	return (c >= 'a' && c <= 'f');
}

/**
 * 判断字符是否为字母
 * 
 * @return  是则返回1.否则返回0
 */
int light_isalpha(int c) {
	c |= 32;
	return (c >= 'a' && c <= 'z');
}

/**
 * 判断字符c是否为数字或字母字面值
 * 
 * @return  是则返回1.否则返回0
 */
int light_isalnum(int c) {
	return light_isdigit(c) || light_isalpha(c);
}

/**
 * buffer对象内容转换成小写
 *
 * @return  总是成功，返回0
 */
int buffer_to_lower(buffer *b) {
	char *c;

	if (b->used == 0) return 0;

	for (c = b->ptr; *c; c++) {
		if (*c >= 'A' && *c <= 'Z') {
			*c |= 32;
		}
	}

	return 0;
}


/**
 * buffer对象内容转换成大写
 *
 * @return  总是成功，返回0
 */
int buffer_to_upper(buffer *b) {
	char *c;

	if (b->used == 0) return 0;

	for (c = b->ptr; *c; c++) {
		if (*c >= 'a' && *c <= 'z') {
			*c &= ~32;
		}
	}

	return 0;
}
