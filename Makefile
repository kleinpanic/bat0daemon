CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0` -I$(INC_DIR)
LDFLAGS = `pkg-config --libs gtk+-3.0`
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
OBJS = $(OBJ_DIR)/battery_monitor.o $(OBJ_DIR)/notification.o $(OBJ_DIR)/process_monitor.o
TARGET = battery_monitor

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ -c $<

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
