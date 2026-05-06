# AES CTR mode offload - Section 6.3.1

Testing the AES CTR offload in CREED requires a two-fold approach to get both the hardware-accelerated numbers and the software baseline shown in the paper. The process for obtaining these numbers is described below: 

#### CREED-offloaded AES CTR hardware:
For this purpose, new bitstreams need to be built, following the standard Coyote approach applied to the directory `aes_ctr/balboa_offload/hw/`. The paths in the CMakeLists have been edited to work from the current location of the repository:

```bash
cd experiments/aes_ctr/balboa_offload/hw
mkdir build_hw && cd build_hw
cmake ../ -DFDEV_NAME=u55c
make project && make bitgen
```

The bitstream is produced at `experiments/aes_ctr/balboa_offload/hw/build_hw/bitstreams/cyt_top.bit`. Flash it and load the driver exactly as described in `experiments/throughput_evaluation/README.md` (steps 2–3), substituting this bitstream path. After that, use the standard server/client binaries from `Coyote/examples/09_perf_rdma/sw` (built as described in step 4 of that README) to run both latency and throughput numbers with the same parameters (IP-address for out-of-band QP-exchange, number of repetitions, message size scaling, etc.). 

#### SW-Baseline

The software baseline is a standalone C application in `software_baseline/` that benchmarks AES-128 encryption/decryption over RDMA using a client-server model. It measures round-trip latency and bandwidth for a range of message sizes (64 B to 1 GB).

**Configuration**
Some other parameters (e.g., max size and thread count) can be configured in `common.h`.  
The default values in this file were used for the paper.

**Dependencies**

The following libraries must be installed:
- `libibverbs` and `librdmacm` (RDMA Verbs and Connection Manager)
- OpenSSL (`libssl`, `libcrypto`)
- pthreads

Both machines must have commercial RNICs that support libverbs for network connectivity (i.e. Mellanox ConnectX-5). 

**Build**

```bash
cd software_baseline
make server
make client
```

This produces `bin/server` and `bin/client`, respectively.  

**Run**

Start the server on one machine, passing the AES mode (0 = ECB, 1 = CTR):

```bash
./bin/server 1
```

Then start the client on the other machine, providing the server's hostname or IP and the same AES mode:

```bash
./bin/client <server_ip> 1
```

The client prints per-trial latency and bandwidth for each message size, followed by AVG/MIN/MAX statistics. The server does not produce output beyond connection diagnostics.
