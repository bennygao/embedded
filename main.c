#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include "gunzip.h"
#include "aes.h"

void print_binary(FILE *fp, void *ptr, int len)
{
    unsigned char *bptr = (unsigned char*) ptr;
    int i;
    char left[64], right[64], tmp[64];
    
    if (!bptr || len <= 0)
        return;
    
    bzero(left, sizeof(left));
    bzero(right, sizeof(right));
    
    fprintf(fp, "-----+---------------------------------------------------+------------------+\r\n");
    fprintf(fp, "     | ");
    for (i = 0; i < 16; ++i) {
        fprintf(fp, "%02d %s", i, i == 7 ? "- " : "");
    }
    fprintf(fp, "| %16d |\r\n", len);
    fprintf(fp, "-----+---------------------------------------------------+------------------+");
    
    for (i = 0; i < len; ++i) {
        if (i % 16 == 0) {
            if (i == 0) {
                fprintf(fp, "\r\n");
            } else {
                fprintf(fp, "%s| %s |\r\n", left, right);
            }
            
            sprintf(left, "%05d| ", i);
            bzero(right, sizeof(right));
        }
        
        if (i % 8 == 0 && i % 16 != 0) {
            strcat(left, "- ");
        }
        sprintf(tmp, "%02X ", bptr[i]);
        strcat(left, tmp);
        
        bzero(tmp, sizeof(tmp));
        tmp[0] = isprint(bptr[i]) ? bptr[i] : '.';
        strcat(right, tmp);
    }
    
    fprintf(fp, "%-57s| %-16s |\r\n", left, right);
    fprintf(fp, "-----+---------------------------------------------------+------------------+\r\n");
    fflush(fp);
}

int test_gunzip(const char *input_file, const char *output_file)
{
    int fd, out_fd;
    int ulen; // 解压缩后的长度
    struct stat stat;
    
    if ((fd = open(input_file, O_RDONLY)) < 0) {
        perror("open() source:");
        return -1;
    }
    
    if ((out_fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("open() dest:");
        return -1;
    }
    
    if (fstat(fd, &stat) < 0) {
        perror("fstat");
        return -2;
    }
    
    ulen = gunzip(fd, (int) stat.st_size, out_fd);
    if (ulen < 0) {
        fprintf(stderr, "gunzip error code=%d", ulen);
    }
    close(fd);
    close(out_fd);
    
    return ulen;
}

int test_aes(void)
{
    AesCtx ctx;
    unsigned char iv[] = {
        0x53, 0x01, 0x02, 0x19, 0x50, 0x01, 0x01, 0x2f,
        0x5e, 0x2b, 0x3c, 0x7d, 0x64, 0x10, 0x37, 0x00
    };
    unsigned char key[] = "Smart your store";
    unsigned char databuf[64]; // must be in multiple of 16
    int fd;
    size_t count;
    
    // initialize context and encrypt data at one end
    if (AesCtxIni(&ctx, iv, key, KEY128, CBC) < 0) {
        printf("init error\n");
        return -1;
    }
    
    if ((fd = open("/Users/gaobo/Desktop/workspace/eslworking/tmp/src.aes", O_RDONLY)) < 0) {
        perror("open:");
        return -1;
    }
    
    memset(databuf, 0, sizeof(databuf));
    while ((count = read(fd, databuf, sizeof(databuf))) > 0) {
        if (AesDecrypt(&ctx, databuf, databuf, sizeof databuf) < 0) {
            printf("error in decryption\n");
            return -1;
        }
        
        write(1, databuf, count);
        memset(databuf, 0, sizeof(databuf));
    }
    
    printf("\n");
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }
    
    test_gunzip(argv[1], argv[2]);
    
    return 0;
}
