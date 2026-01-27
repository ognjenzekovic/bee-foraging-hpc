CC = gcc
CFLAGS = -Wall -O2 -lm
TARGET = seq

all: $(TARGET)

$(TARGET): simulation.c
	$(CC) simulation.c -o $(TARGET) $(CFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) results_*.txt positions.csv bee_simulation.gif

.PHONY: all run clean