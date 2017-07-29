//
//  gunzip.h
//

#ifndef __gunzip_h_
#define __gunzip_h_

#define FHCRC_MASK 2
#define FEXTRA_MASK 4
#define FNAME_MASK 8
#define FCOMMENT_MASK 16

#define BTYPE_NONE 0
#define BTYPE_DYNAMIC 2

#define MAX_BITS 16
#define MAX_CODE_LITERALS 287
#define MAX_CODE_DISTANCES 31
#define MAX_CODE_LENGTHS 18
#define EOB_CODE 256

#define MAX_LENGTHS_NUM     ((MAX_CODE_LENGTHS << 1) + MAX_BITS)
#define MAX_LITERALS_NUM    ((MAX_CODE_LITERALS << 1) + MAX_BITS)
#define MAX_DISTANCES_NUM   ((MAX_CODE_DISTANCES << 1) + MAX_BITS)

typedef unsigned char byte;

#define ERR_FORMAT          -1  // 不支持的压缩格式
#define ERR_LITERALS_NUM    -2  // literal个数超限
#define ERR_DISTANCES_NUM   -3  // distance个数超限

struct inflate_context {
//    int error;
    int index;
    int byte;
    int bit;
    
    int compressed_len;
//    byte *compressed;
    
    int lengths_tree[MAX_LENGTHS_NUM];
    int lengths_num;
    
    int literals_tree[MAX_LITERALS_NUM];
    int literals_num;
    
    int distances_tree[MAX_DISTANCES_NUM];
    int distances_num;
};

extern int gunzip(int zfd, int compressed_len, int ufd);

#endif /* __gunzip_h_ */
