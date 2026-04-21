# AES CTR mode offload

Testing The AES CTR offload in BALBOA requires a two-fold approach to get both the hardware-accelerated numbers and the software baseline shown in the paper. The process for obtaining these numbers is described below: 

#### BALBOA-offloaded AES CTR hardware: 
For this purpose, new bitstreams need to be built, following the standard Coyote approach applied to the directory `aes_ctr/balboa_offload`. The paths in the Cmake-file have been edited to work from the current location of the repository. After obtaining the bitstream, one can use the "plain software" from `Coyote/examples/09_perf_rdma/sw` to run both latency and throughput numbers. 

#### SW-Baseline

The software baseline is a standalone C application in `software_baseline/` that benchmarks AES-128 encryption/decryption over RDMA using a client-server model. It measures round-trip latency and bandwidth for a range of message sizes (64 B to 1 GB).

**Dependencies**

The following libraries must be installed:
- `libibverbs` and `librdmacm` (RDMA Verbs and Connection Manager)
- OpenSSL (`libssl`, `libcrypto`)
- pthreads

Both machines must have commercial RNICs that support libverbs for network connectivity.

**Build**

```bash
cd software_baseline
make
```

This produces `bin/server` and `bin/client`.

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
