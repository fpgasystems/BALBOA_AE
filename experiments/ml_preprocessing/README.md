# ML Preprocessing Pipeline (Section 8)

This artifact reproduces Section 8 (Figures 9 and 10) of the paper, demonstrating on-datapath DLRM preprocessing offloaded to the CREED network datapath with direct-to-GPU data delivery. Two FPGA-equipped nodes and at least one AMD GPU are required.

The paper compares three configurations (Figure 8):

1. **Vanilla** — RDMA delivers data to CPU memory; preprocessing runs in software on the CPU (NumPy); the result is copied to GPU memory. Reproduced from `client_py/` — the standard RDMA-capable CREED / Coyote bitstream is sufficient for this application.
2. **FPGA preprocessing + CPU-to-GPU copy** - the `ml_preprocessing` HLS kernel sits inline on the RDMA-RX datapath; the preprocessed payload lands in CPU memory; the client calls `hipMemcpy` after each completion. A new bitstream needs to be built that combines CREED with the ML-preprocessing circuits. 
3. **FPGA preprocessing + GPU P2P** — same HLS kernel, but the RDMA buffer is registered directly as GPU memory, bypassing the CPU entirely. Uses the same bitstream as example 2. 

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

On the side of the server, we can use the standard server software provided with `sw/src/server/main.cpp`. For the client, we use a python runtime for the usual software to have access to the SW-implementations of the preprocessing operators. 

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

The server has to be started first with the usual steps for bitstream loading, driver insertion and kick-off of the software. The same arguments as later for the client (buffer size, number of reps etc.) need to be applied.

Also on the client side, driver and bitstream need to be loaded first. Afterwards, we run `client_py/example.py`: 

```bash
python3 example.py --server-ip <server-IP> [--threads N] [--n-runs R] [--n-transfers T] [--buffer-size B]
```

- `--server-ip`: IP address of the server node (required).
- `--threads`: number of CPU threads for the NumPy preprocessing loop (default 1); sweep 1–N to reproduce the thread-scaling curve in Figure 9.
- `--n-runs` / `--n-transfers`: averaging repetitions / transfers per data point (defaults 10 / 64).
- `--buffer-size`: pinned CPU buffer in bytes (default 128 MB).


## CREED-offloaded ML-preprocessing
As shown in the paper, we have two setups with offloaded ML-preprocessing: Either using explicit memory copies from CPU memory to the GPUs, or leveraging the GPU-direct feature. In both cases, the used hardware is the same on both nodes, while the software for the client differs (server remains the same): 

### Hardware

The bitstream on the server side can remain the basic one from the very beginning, also used in the experiment before. On the client, we now need a bitstream with offloaded preprocessing. Build the FPGA bitstream for the client from `hw/`:

```bash
cd hw/
mkdir build && cd build
cmake .. && make
```

`BUILD_OPT=1` is set in the CMakeLists because timing closure with the RDMA stack is tight; expect long synthesis times.

### Software

Server and client are built separately. All paths in the CMake scripts have been updated to work from this location in the repository.

```bash
cd sw/

mkdir build_server && cd build_server
cmake .. -DINSTANCE=server && make

cd ..
mkdir build_client && cd build_client
cmake .. -DINSTANCE=client && make
```

In the original state of the repo, `sw/src/client/main.cpp` reflects the setup 2, with explicit CPU-GPU copies. 
To reproduce setup ③ (GPU P2P, Figure 10), copy `main_rdma_gpu.cpp` over `main.cpp` before building the client:

```bash
cp src/client/main_rdma_gpu.cpp src/client/main.cpp
```

After that, recompile and rerun. We provide both setups (2 + 3, CPU-GPU copies and direct to GPU) in the two distincly named files `main_rdma_cpu_gpu.cpp` and `main_rdma_gpu.cpp`, so that this overwriting can be reverted any time later. 

The `AMD_GPU` CMake variable defaults to `gfx90a` (MI210); override with `-DAMD_GPU=<arch>` if your GPU differs.

