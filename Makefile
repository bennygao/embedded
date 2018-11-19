CC=gcc
RM=rm

CCFLAGS=-I. -g -DDEBUG
#CCFLAGS=-I. -O2
LDFLAGS=-lz

OBJS=main.o gunzip.o aes.o code128.o
TARGET=yagunzip

all:$(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CC) -o$(TARGET) $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CCFLAGS) -c $<
