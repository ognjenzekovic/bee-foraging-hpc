CC = gcc
CFLAGS = -Wall -O2 -lm
TARGET = bee_sim_seq

all: $(TARGET)

$(TARGET): simulation.c
	$(CC) simulation.c -o $(TARGET) $(CFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) results_*.txt

.PHONY: all run clean