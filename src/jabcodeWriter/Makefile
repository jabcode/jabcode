PREFIX 	=
CC 	= $(PREFIX)gcc
CFLAGS	 = -O2 -std=c11

TARGET = bin/jabcodeWriter

OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

$(TARGET): $(OBJECTS)
	$(CC) $^ -L../jabcode/build -ljabcode -L../jabcode/lib -lpng -lz -lm $(CFLAGS) -o $@

$(OBJECTS): %.o: %.c
	$(CC) -c -I. -I../jabcode -I../jabcode/include $(CFLAGS) $< -o $@

install:
	install $(TARGET) /usr/bin

clean:
	rm -f $(TARGET) $(OBJECTS)
