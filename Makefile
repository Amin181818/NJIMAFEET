CC = gcc
CFLAGS = -Wall -Wextra -pthread $(shell sdl2-config --cflags)
LDFLAGS = -pthread -lm $(shell sdl2-config --libs) -lSDL2_ttf

TARGET = safefeet

SRCS = main.c \
       sensor_simulation_task.c \
       mapping_task.c \
       fall_detection_task.c \
       stabilization_alert_task.c \
       display_ui_task.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c shared_data.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
