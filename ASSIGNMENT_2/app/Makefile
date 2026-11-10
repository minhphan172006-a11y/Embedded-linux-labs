CC = gcc

CFLAGS = -Wall -Wextra -Werror -pthread -I. -I../driver
LDFLAGS = -pthread -lm

TARGET = sms_app

SOURCES = \
	sms_app.c \
	circular_buffer.c \
	message_queue.c \
	sensor_thread.c \
	processor_thread.c \
	alert_thread.c \
	logger_thread.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
