CC = gcc
CFLAGS = -O3
SOURCES = main.c
TARGET = gbooru
LIBS = -lcurl -lpthread

all:
	$(CC) $(CFLAGS) $(SOURCES) $(LIBS) -o $(TARGET)