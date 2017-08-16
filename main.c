#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include "gunzip.h"
#include "aes.h"

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
        fprintf(stderr, "gunzip error code=%d\n", ulen);
    }
    close(fd);
    close(out_fd);
    
    return ulen;
}

int test_aes(const char *input_file, const char *output_file)
{
    unsigned char key[] = "Smart your store"; // 秘钥，16bytes
    unsigned char cipher[16]; // 密文
    unsigned char plain[16]; // 解密后的明文
    
    int ifd, ofd;
    AesCtx ctx;
    size_t count;
    
    if ((ifd = open(input_file, O_RDONLY)) < 0) {
        perror("open() source:");
        return -1;
    }
    
    if ((ofd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
        perror("open() dest:");
        return -1;
    }
    
    // initialize context and encrypt data at one end
    if (AesCtxIni(&ctx, NULL, key, KEY128, EBC) < 0) {
        printf("init error\n");
        return -1;
    }
    
    while ((count = read(ifd, cipher, sizeof(cipher))) > 0) {
        if (AesDecrypt(&ctx, cipher, plain, sizeof(cipher)) < 0) {
            printf("error in decryption\n");
            return -1;
        }
        
        write(ofd, plain, count);
    }
    
    close(ifd);
    close(ofd);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }
    
    test_gunzip(argv[1], argv[2]);
    // test_aes(argv[1], argv[2]);
    
    return 0;
}
