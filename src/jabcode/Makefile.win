PREFIX 	=
CC 	= $(PREFIX)gcc
CFLAGS	= -O2 -std=c11

TARGET = build/libjabcode.dll

OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

$(TARGET): $(OBJECTS)
	$(CC) $^ -L./lib/win64 -ltiff -lpng16 -lz -lm -shared $(CFLAGS) -o $@

$(OBJECTS): %.o: %.c
	$(CC) -c -I. -I./include $(CFLAGS) $< -o $@	

clean:
	rm -f $(TARGET) $(OBJECTS)
