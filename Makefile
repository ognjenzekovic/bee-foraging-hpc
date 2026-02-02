CC = gcc
MPICC = mpicc
CFLAGS = -Wall -O3
LDFLAGS = -lm

TARGET_SEQ = seq
SRC_SEQ = simulation.c

TARGET_OMP = omp
SRC_OMP = simulation_omp.c

TARGET_MPI = mpi
SRC_MPI = simulation_mpi.c

all: $(TARGET_SEQ) $(TARGET_OMP) $(TARGET_MPI)

$(TARGET_SEQ): $(SRC_SEQ) types.h config.h
	$(CC) $(SRC_SEQ) -o $(TARGET_SEQ) $(CFLAGS) $(LDFLAGS)

$(TARGET_OMP): $(SRC_OMP) types.h config.h
	$(CC) $(SRC_OMP) -o $(TARGET_OMP) $(CFLAGS) -fopenmp $(LDFLAGS)

$(TARGET_MPI): $(SRC_MPI) types.h config.h
	$(MPICC) $(SRC_MPI) -o $(TARGET_MPI) $(CFLAGS) $(LDFLAGS)

run_seq: $(TARGET_SEQ)
	./$(TARGET_SEQ)

run_omp: $(TARGET_OMP)
	./$(TARGET_OMP) 4

run_mpi: $(TARGET_MPI)
	mpirun -np 4 ./$(TARGET_MPI)

clean:
	rm -f $(TARGET_SEQ) $(TARGET_OMP) $(TARGET_MPI) results_*.txt positions.csv bee_simulation.gif

.PHONY: all run_seq run_omp run_mpi clean