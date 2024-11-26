CC = gcc
CFLAGS = -Wall -O2

TARGET = httpd
SOURCES = httpd.c 

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

run: $(TARGET)
	./$(TARGET) 3334
