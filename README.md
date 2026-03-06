# Bee Foraging Simulation - HPC Implementation

Multi-agent simulation of honey bee foraging behavior with parallel implementations using OpenMP and OpenMPI.

## Overview

This project simulates bee colony foraging behavior including:
- Scout bees exploring for flowers
- Waggle dance communication in the hive
- Follower bees recruited to food sources
- Competition for flower resources
- Nectar collection and regeneration

Three implementations are provided:
1. **Sequential** - Baseline single-threaded version
2. **OpenMP** - Shared-memory parallelization
3. **OpenMPI** - Distributed-memory parallelization

---

### Required Software

#### 1. C Compiler
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential

# Arch Linux
sudo pacman -S base-devel

# macOS
xcode-select --install
```

#### 2. OpenMP Support
OpenMP is typically included with GCC 4.2+. Verify:
```bash
gcc --version  # Should be 4.2 or higher
```

If needed:
```bash
# Ubuntu/Debian
sudo apt install libomp-dev

# Arch Linux
sudo pacman -S openmp

# macOS
brew install libomp
```

#### 3. OpenMPI
```bash
# Ubuntu/Debian
sudo apt install openmpi-bin openmpi-common libopenmpi-dev

# Arch Linux
sudo pacman -S openmpi

# CentOS/RHEL
sudo yum install openmpi openmpi-devel
# Then load the module:
module load mpi/openmpi-x86_64

# macOS
brew install open-mpi
```

Verify installation:
```bash
mpicc --version
mpirun --version
```

#### 4. Math Library
Usually included with system, but if needed:
```bash
# Ubuntu/Debian
sudo apt install libm-dev
```

### Python Dependencies (for Visualization)

#### Required:
```bash
# Install Python 3 if not present
sudo apt install python3 python3-pip

# Install required packages
pip3 install pandas matplotlib numpy pillow

# Or using requirements.txt:
pip3 install -r requirements.txt
```

#### Optional (for MP4 export):
```bash
# Ubuntu/Debian
sudo apt install ffmpeg

# Arch Linux
sudo pacman -S ffmpeg

# macOS
brew install ffmpeg

# Verify
ffmpeg -version
```


# run
- $ make
- $ ./seq (for sequential)
- $ ./omp
- $ mpirun -np 4 ./mpi (where 4 represents number of processes)

- for files cleanup
$ make clean
