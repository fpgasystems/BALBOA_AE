# ML Preprocessing Pipeline

This artifact reproduces Section 8 (Figures 9 and 10) of the paper, demonstrating on-datapath DLRM preprocessing offloaded to the CREED network datapath with direct-to-GPU data delivery. Two FPGA-equipped nodes and at least one AMD GPU are required.

The paper compares three configurations (Figure 8):

1. **Vanilla** — RDMA delivers data to CPU memory; preprocessing runs in software on the CPU (NumPy); the result is copied to GPU memory. Reproduced from `client_py/` — the standard RDMA-capable CREED / Coyote bitstream is sufficient for this application.
2. **FPGA preprocessing + CPU-to-GPU copy** (`main.cpp`) — the `ml_preprocessing` HLS kernel sits inline on the RDMA-RX datapath; the preprocessed payload lands in CPU memory; the client calls `hipMemcpy` after each completion. A new bitstream needs to be built that combines CREED with the ML-preprocessing circuits. 
3. **FPGA preprocessing + GPU P2P** (`main_rdma_gpu.cpp`) — same HLS kernel, but the RDMA buffer is registered directly as GPU memory, bypassing the CPU entirely. Uses the same bitstream as example 2. 

## Folder Layout

```
hw/
  CMakeLists.txt                 FPGA project (EN_RDMA=1, 1 vFPGA, 2 host streams)
  src/vfpga_top.svh              vFPGA wiring; instantiates ml_preprocessing on the RDMA-RX path
  src/hls/ml_preprocessing/     HLS preprocessing kernel (Neg2Zero → Log pipeline)
sw/
  src/client/main.cpp            Setup ②: CPU-staged preprocessing + hipMemcpy per completion
  src/client/main_rdma_gpu.cpp   Setup ③: GPU-direct RDMA (rename over main.cpp to use)
  src/server/main.cpp            Passive server
  src/client/extract_output.txt  Reference output from a prior run
client_py/
  src/rdma_module.cpp            pybind11 C++ extension wrapping Coyote + HIP for Python
  example_CPU.py                 Setup ①: vanilla CPU baseline (no FPGA, no RDMA required)
  example.py                     RDMA + Python preprocessing benchmark (requires server)
  include/constants.hpp          Shared constants (vFPGA ID, GPU ID)
```

## Vanilla CPU Baseline (Setup ①, Figure 9)

This setup requires only the client node with an AMD GPU. No FPGA bitstream and no server are needed.

### Build

Install the Python dependencies, then build the pybind11 C++ extension:

```bash
pip install pybind11 threadpoolctl numpy

cd client_py/
mkdir build && cd build
cmake .. [-DAMD_GPU=<arch>] && make
cd ..
```

The compiled module (`rdma_module*.so`) is placed in `build/lib/` and loaded automatically by the Python scripts.

### Run

```bash
python3 example_CPU.py [--threads N] [--n-runs R] [--n-transfers T] [--buffer-size B]
```

- `--threads`: number of CPU threads for the NumPy preprocessing loop (default 1); sweep 1–N to reproduce the thread-scaling curve in Figure 9.
- `--n-runs` / `--n-transfers`: averaging repetitions / transfers per data point (defaults 10 / 64).
- `--buffer-size`: pinned CPU buffer in bytes (default 128 MB).

The preprocessing pipeline matches the FPGA kernel: `max(x, 0)` + `log1p` on the first 16 of 48 int32 columns, modulo 8192 on the remaining 32.

## Hardware

Build the FPGA bitstream from `hw/`:

```bash
cd hw/
mkdir build && cd build
cmake .. && make
```

`BUILD_OPT=1` is set in the CMakeLists because timing closure with the RDMA stack is tight; expect long synthesis times.

## Software

Server and client are built separately. All paths in the CMake scripts have been updated to work from this location in the repository.

```bash
cd sw/

mkdir build_server && cd build_server
cmake .. -DINSTANCE=server && make

cd ..
mkdir build_client && cd build_client
cmake .. -DINSTANCE=client && make
```

To reproduce setup ③ (GPU P2P, Figure 10), copy `main_rdma_gpu.cpp` over `main.cpp` before building the client:

```bash
cp src/client/main_rdma_gpu.cpp src/client/main.cpp
```

The `AMD_GPU` CMake variable defaults to `gfx90a` (MI210); override with `-DAMD_GPU=<arch>` if your GPU differs.

## Running

**Always start the server first.** The server is the passive party in the QP bootstrap exchange.

```bash
# Server node
./build_server/test [-o 0|1] [-r N] [-x MIN] [-X MAX]

# Client node
./build_client/test -i <server-CPU-IP> [-o 0|1] [-r N] [-x MIN] [-X MAX]
```

- `-i`: server CPU IP on the management network (not the FPGA data-network address).
- `-o 0`: RDMA READ (default); `-o 1`: RDMA WRITE.
- `-r`: repetitions per data point (default 10).
- `-x` / `-X`: min/max buffer size in bytes (defaults 64 / 1 048 576).

Reference output from a prior run is in `sw/src/client/extract_output.txt`. Expected results match Figures 9 and 10 of the paper: the FPGA-offloaded path reaches ~8500 MB/s (PCIe-switch limited between FPGA and GPU), while vanilla CPU preprocessing tops out at ~1190 MB/s regardless of thread count; GPU-direct delivery (setup ③) saves 20–135 µs of latency compared to the CPU-staged path (setup ②).

## Status and Known Issues

- **Pipeline stage placeholder.** `ml_preprocessing.cpp` calls `Dense_Log` twice instead of routing through `Sparse_HexToIntMod`. The sparse-feature stage is defined in `ml_preprocessing.hpp` but currently commented out at the call site.
- **Float / int handling in dense stages.** Inside `Dense_NegsToZero` and the input side of `Dense_Log`, the 32-bit lane is loaded as `int` rather than reinterpreted as `float` via the `conv` union. This gives wrong results for negative-signed-bit float inputs.
- **Variants not built by default.** `*_rdma_gpu.cpp` must be manually renamed over `main.cpp` before invoking CMake.
