#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "gunzip.h"

const static int LENGTH_EXTRA_BITS[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99
};

const static int LENGTH_VALUES[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

const static int DISTANCE_EXTRA_BITS[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

const static int DISTANCE_VALUES[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};

const static int DYNAMIC_LENGTH_ORDER[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2,
    14, 1, 15
};

static void trace(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
#ifdef DEBUG
    vfprintf(fp, fmt, ap);
#endif
    va_end(ap);
}

/***************************************************************************
 * 需要改造成为flash读写的函数
 ***************************************************************************/

/**
 * 从文件的指定偏移量处读入1个字节
 * @param fd 文件描述符
 * @param offset 偏移量
 * @return 文件指定偏移量处读入的字节值
 */
int read_byte(int fd, int offset)
{
    byte b;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &b, 1) != 1) {
    	return -1;
    } else {
    	return b & 0xFF;
    }
}

/**
 * 从压缩文件指定偏移量处拷贝指定长度的字节到解压缩文件指定偏移量处
 * @param zfd 压缩文件的文件描述符
 * @param zoff 压缩文件的偏移量，从这里开始拷贝数据。
 * @param ufd 解压缩文件的文件描述符
 * @param uoff 解压缩文件的偏移量，数据拷贝到这里。
 * @param len 拷贝的数据长度
 * @return 拷贝的字节数
 */
int copy_bytes(int zfd, int zoff, byte *ubuf, int uoff, int len)
{
    byte b;
    int i;

    lseek(zfd, zoff, SEEK_SET);
    return read(zfd, ubuf + uoff, len);
}
/***************************************************************************/

static void create_huffman_tree(byte *bits, int max_code, int *tree) {
    int bl_count[MAX_BITS + 1];
    int code = 0, value = 0, left = 0, right = 0, node = 0;
    int next_code[MAX_BITS + 1];
    int len = 0, i = 0, bit = 0, bv;
    int treeInsert = 1;
    
    memset(bl_count, 0, sizeof(bl_count));
    memset(next_code, 0, sizeof(next_code));
    
    for (i = 0; i < max_code + 1; i++) {
        bv = bits[i];
        trace(stderr, "(%d %d)", bv, bl_count[bv]);
        bl_count[bv]++;
    }
    trace(stderr, "\n");
    bl_count[0] = 0;
    
    trace(stderr, "maxCode=%d bitsLength=%d bl_count[%d]: ", max_code, max_code + 1, MAX_BITS + 1);
    for (i = 0; i < MAX_BITS + 1; ++i) {
        trace(stderr, "%d,", bl_count[i]);
    }
    trace(stderr, "\n");
    
    for (i = 1; i <= MAX_BITS; i++) {
        bv = bl_count[i - 1];
        code = (code + bv) << 1;
        next_code[i] = code;
    }
    
    trace(stderr, "next_code[%d]: ", MAX_BITS + 1);
    for (i = 0; i < MAX_BITS + 1; ++i) {
        trace(stderr, "%d,", next_code[i]);
    }
    trace(stderr, "\n");
    
    for (i = 0; i <= max_code; i++) {
        len = bits[i];
        if (len != 0) {
            code = next_code[len]++;
            node = 0;
            for (bit = len - 1; bit >= 0; bit--) {
                value = code & (1 << bit);
                if (value == 0) {
                    left = (tree[node] >> 16) & 0xFFFF;
                    if (left == 0) {
                        tree[node] |= (treeInsert << 16);
                        node = treeInsert++;
                    } else {
                        node = left;
                    }
                } else {
                    right = tree[node] & 0xFFFF;
                    if (right == 0) {
                        tree[node] |= treeInsert;
                        node = treeInsert++;
                    } else {
                        node = right;
                    }
                }
            }

            tree[node] = 0x80000000 | i;
        }
    }
}

static int read_code(int zfd, int *tree, struct inflate_context *context) {
    int node = tree[0];
    
    while (node >= 0) {
        if (context->bit == 0) {
            context->byte = read_byte(zfd, context->index++);
        }
        
        node = (((context->byte & (1 << context->bit)) == 0) ? tree[node >> 16] : tree[node & 0xFFFF]);
        context->bit = (context->bit + 1) & 7;
    }
    
    return (node & 0xFFFF);
}

static int read_bits(int zfd, struct inflate_context *context, const int n) {
    int data = (context->bit == 0 ? (context->byte = read_byte(zfd, context->index++)) : (context->byte >> context->bit));
    int i;
    
    for (i = (8 - context->bit); i < n; i += 8) {
        context->byte = read_byte(zfd, context->index++);
        data |= (context->byte << i);
    }
    
    context->bit = (context->bit + n) & 7;
    return (data & ((1 << n) - 1));
}

static void decode_code_lengths(int zfd, struct inflate_context *context, int count, byte *bits) {
    int i, code = 0, last = 0, repeat = 0;
    
    for (i = 0, code = 0, last = 0; i < count; ) {
        code = read_code(zfd, context->lengths_tree, context);
        if (code >= 16) {
            repeat = 0;
            if (code == 16) {
                repeat = 3 + read_bits(zfd, context, 2);
                code = last;
            } else {
                if (code == 17) {
                    repeat = 3 + read_bits(zfd, context, 3);
                } else {
                    repeat = 11 + read_bits(zfd, context, 7);
                }
                
                code = 0;
            }
            
            while (repeat-- > 0) {
                bits[i++] = (byte) code;
            }
        } else {
            bits[i++] = (byte) code;
        }
        
        last = code;
    }
}

/**
 * 以gzip算法解压缩文件
 * @param zfd 压缩文件的文件描述符
 * @param zlen 压缩文件的长度
 * @param ufd 解压缩文件的文件描述符
 * @return 成功时返回解压缩后的长度。如果失败，返回负数：
 *         -1表示不支持的压缩格式
 *         -2表示literal个数超限
 *         -3表示distance个数超限
 */
int gunzip(int zfd, int zlen, byte *ubuf) {
    int ulen = 0; // 解压缩后的长度
    int uindex = 0; // 解压缩数据偏移量索引
    struct inflate_context context;
    
    int flg = 0, bfinal = 0, btype = 0, len = 0, bv;
    int hlit = 0, hdist = 0, hclen = 0, i = 0;
    byte distance_bits[MAX_CODE_DISTANCES + 1];
    byte length_bits[MAX_CODE_LENGTHS + 1];
    byte literal_bits[MAX_CODE_LITERALS + 1];
    int code = 0, leb = 0, deb = 0;
    int length = 0, distance = 0, offset = 0, save_index;
    
    memset(&context, 0, sizeof(struct inflate_context));
    if (read_bits(zfd, &context, 16) != 0x8B1F || read_bits(zfd, &context, 8) != 8) {
        return ERR_FORMAT;
    }
    
    flg = read_bits(zfd, &context, 8);
    context.index += 6;
    
    if ((flg & FEXTRA_MASK) != 0) {
        context.index += read_bits(zfd, &context, 16);
    }
    
    if ((flg & FNAME_MASK) != 0) {
        while (read_byte(zfd, context.index++) != 0);
    }
    
    if ((flg & FCOMMENT_MASK) != 0) {
        while (read_byte(zfd, context.index++) != 0);
    }
    
    if ((flg & FHCRC_MASK) != 0) {
        context.index += 2;
    }
    
    save_index = context.index; // 保存index当前位置
    context.index = zlen - 4;
    ulen = read_bits(zfd, &context, 16) | (read_bits(zfd, &context, 16) << 16);
    if (ulen > SEGMENT_LEN) {
    	fprintf(stderr, "uncompressed length is greater than %d\n", SEGMENT_LEN);
    	return -1;
    }

    uindex = 0;
    context.index = save_index; // 恢复index位置
    bfinal = 0;
    btype = 0;
    
    do {
        memset(distance_bits, 0, sizeof(distance_bits));
        memset(length_bits, 0, sizeof(length_bits));
        memset(literal_bits, 0, sizeof(literal_bits));
        
        bfinal = read_bits(zfd, &context, 1);
        btype = read_bits(zfd, &context, 2);
        
        if (btype == BTYPE_NONE) { // 无压缩
            trace(stderr, "--------\n");
            context.bit = 0;
            len = read_bits(zfd, &context, 16);
            read_bits(zfd, &context, 16);
            copy_bytes(zfd, context.index, ubuf, uindex, len);
            context.index += len;
            uindex += len;
        } else {
            if (btype == BTYPE_DYNAMIC) { // 动态Huffman
                trace(stderr, "********\n");
                hlit = read_bits(zfd, &context, 5) + 257;
                if (hlit - 1 > MAX_CODE_LITERALS) {
                    return ERR_LITERALS_NUM;
                }
                
                hdist = read_bits(zfd, &context, 5) + 1;
                if (hdist - 1 > MAX_CODE_DISTANCES) {
                    return ERR_DISTANCES_NUM;
                }
                
                hclen = read_bits(zfd, &context, 4) + 4;
                trace(stderr, "hclen=%d: ", hclen);
                for (i = 0; i < hclen; i++) {
                    bv = read_bits(zfd, &context, 3);
                    length_bits[DYNAMIC_LENGTH_ORDER[i]] = (byte) bv;
                    trace(stderr, "[%d=%d]", DYNAMIC_LENGTH_ORDER[i], bv);
                }
                trace(stderr, "\n");
                
                context.lengths_num = MAX_CODE_LENGTHS;
                memset(context.lengths_tree, 0, sizeof(context.lengths_tree));
                create_huffman_tree(length_bits, MAX_CODE_LENGTHS, context.lengths_tree);
                
                context.literals_num = hlit - 1;
                decode_code_lengths(zfd, &context, hlit, literal_bits);
                memset(context.literals_tree, 0, sizeof(context.literals_tree));
                create_huffman_tree(literal_bits, hlit - 1, context.literals_tree);
                
                context.distances_num = hdist - 1;
                decode_code_lengths(zfd, &context, hdist, distance_bits);
                memset(context.distances_tree, 0, sizeof(context.distances_tree));
                create_huffman_tree(distance_bits, hdist - 1, context.distances_tree);
            } else { // 静态Huffman
                trace(stderr, "########\n");
                for (int i = 0; i < 144; i++) {
                    literal_bits[i] = 8;
                }
                
                for (int i = 144; i < 256; i++) {
                    literal_bits[i] = 9;
                }
                
                for (int i = 256; i < 280; i++) {
                    literal_bits[i] = 7;
                }
                
                for (int i = 280; i < 288; i++) {
                    literal_bits[i] = 8;
                }
                
                create_huffman_tree(literal_bits, MAX_CODE_LITERALS, context.literals_tree);
                
                for (int i = 0; i < MAX_CODE_DISTANCES + 1; i++) {
                    distance_bits[i] = 5;
                }
                
                create_huffman_tree(distance_bits, MAX_CODE_DISTANCES, context.distances_tree);
            }
            
            code = leb = deb = 0;
            while ((code = read_code(zfd, context.literals_tree, &context)) != EOB_CODE) {
                if (code > EOB_CODE) {
                    code -= 257;
                    length = LENGTH_VALUES[code];
                    if ((leb = LENGTH_EXTRA_BITS[code]) > 0) {
                        length += read_bits(zfd, &context, leb);
                    }
                    
                    code = read_code(zfd, context.distances_tree, &context);
                    distance = DISTANCE_VALUES[code];
                    if ((deb = DISTANCE_EXTRA_BITS[code]) > 0) {
                        distance += read_bits(zfd, &context, deb);
                    }
                    
                    offset = uindex - distance;
                    while (distance < length) {
                    	memcpy(ubuf + uindex, ubuf + offset, distance);
                        uindex += distance;
                        length -= distance;
                        distance <<= 1;
                    }
                    
                    memcpy(ubuf + uindex, ubuf + offset, length);
                    uindex += length;
                } else {
                	ubuf[uindex++] = (byte) code;
                }
            }
        }
    } while (bfinal == 0);
    
    return ulen;
}
