#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <zlib.h>

#include "gunzip.h"
#include "aes.h"

#define TRACE fprintf(stderr, "%s %d\n", __FILE__, __LINE__);

int test_gunzip(const char *input_file, const char *output_file) {
	int fd, out_fd;
	int ulen; // 解压缩后的长度
	struct stat stat;
	byte ubuf[SEGMENT_LEN];

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

	ulen = gunzip(fd, (int) stat.st_size, ubuf);
	if (ulen < 0) {
		fprintf(stderr, "gunzip error code=%d\n", ulen);
	} else {
		if (write(out_fd, ubuf, ulen) != ulen) {
			fprintf(stderr, "write file error\n");
		}
	}
	close(fd);
	close(out_fd);

	return ulen;
}

int test_aes(const char *input_file, const char *output_file) {
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

		if (write(ofd, plain, count) != count) {
			fprintf(stderr, "write file error\n");
		}
	}

	close(ifd);
	close(ofd);
	return 0;
}

int gzcompress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata) {
	z_stream c_stream;
	int err = 0;

	c_stream.zalloc = NULL;
	c_stream.zfree = NULL;
	c_stream.opaque = NULL;
	//只有设置为MAX_WBITS + 16才能在在压缩文本中带header和trailer
	if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
		return -1;
	}

	c_stream.next_in = data;
	c_stream.avail_in = ndata;
	c_stream.next_out = zdata;
	c_stream.avail_out = *nzdata;
	while (c_stream.avail_in != 0 && c_stream.total_out < *nzdata) {
		if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
			return -1;
	}
	if (c_stream.avail_in != 0)
		return c_stream.avail_in;
	for (;;) {
		if ((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END)
			break;
		if (err != Z_OK)
			return -1;
	}
	if (deflateEnd(&c_stream) != Z_OK)
		return -1;
	*nzdata = c_stream.total_out;
	return 0;
}

int compress_file(const char *filename) {
	int ifd; // input文件描述符
	int ofd;
	int count, sequence = 0, total = 0;
	char output_path[PATH_MAX];
	byte buffer[SEGMENT_LEN], zipped[SEGMENT_LEN];
	uLong zlen;
	struct stat stat;
	double rate;

	if ((ifd = open(filename, O_RDONLY)) < 0) {
		perror("open() source:");
		return -1;
	}

	if (fstat(ifd, &stat) < 0) {
		perror("fstat");
		return -2;
	}

	while ((count = read(ifd, buffer, SEGMENT_LEN)) > 0) {
		zlen = SEGMENT_LEN;
		if (gzcompress(buffer, count, zipped, &zlen) < 0) {
			fprintf(stderr, "gzcompress failed\n");
			return -1;
		} else {
			sprintf(output_path, "%s.%04d.gz", filename, sequence++);
			if ((ofd = open(output_path, O_RDWR | O_CREAT | O_TRUNC, 0644))
					< 0) {
				perror("open() output file:");
				return -1;
			}

			if (write(ofd, zipped, zlen) != zlen) {
				fprintf(stderr, "write file error\n");
			}
			total += zlen;
			close(ofd);
		}
	}

	close(ifd);

	rate = (double) total * 100 / stat.st_size;
	printf("compress rate: %.2lf%%\n", rate);
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [-c|-d] <input file> <output file>\n",
				argv[0]);
		fprintf(stderr, "-c compress\n");
		fprintf(stderr, "-d decompress\n");
		return 1;
	}

	if (strcmp(argv[1], "-c") == 0) {
		compress_file(argv[2]);
	} else {
		test_gunzip(argv[2], argv[3]);
	}
	// test_aes(argv[1], argv[2]);

	return 0;
}
