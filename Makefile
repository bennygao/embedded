CC=gcc
RM=rm

#CCFLAGS=-I. -g -DDEBUG
CCFLAGS=-I. -Wall -O2
LDFLAGS=-s

OBJS=main.o gunzip.o aes.o
TARGET=yagunzip

all:$(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o$(TARGET) $(OBJS)

.c.o:
	$(CC) $(CCFLAGS) -c $<
