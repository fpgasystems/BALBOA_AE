# Throughput Evaluation - Section 6.1

The basic CREED-CREED throughput evaluation (FPGAs with CREED on both ends of the network connection) can be conducted as explained in `Coyote/examples/09_perf_rdma`, by designating one of the endpoints as server, the other as client of this connection. For this purpose, one needs to follow the instructions in the Coyote-repository to first build a Coyote-bitstream that includes CREED as RDMA-stack and a RDMA-dedicated vFPGA. Besides the full-length description in the included subrepository, we attach a brief summary of the required building steps: 

### Building CREED within Coyote and conducting experiments within the Coyote-framework:

**1. Hardware synthesis (bitstream)**

The hardware design lives in `Coyote/examples/09_perf_rdma/hw/`. Building requires Vivado >= 2022.1 (our numbers are obtained with Vivado 2024.2).

```bash
cd Coyote/examples/09_perf_rdma/hw
mkdir build_hw && cd build_hw
cmake ../ -DFDEV_NAME=<target_dev>   # e.g. u55c, u280, u250
make project && make bitgen
```

The finished bitstream is placed at `Coyote/examples/09_perf_rdma/hw/build_hw/bitstreams/cyt_top.bit`. Hardware builds can take several hours; run inside `screen` or `tmux` on a remote node. If synthesis fails because the network submodule is missing, run `git submodule update --init --recursive` from the repository root first.

**2. Driver compilation**

```bash
cd Coyote/driver
make TARGET_PLATFORM=ultrascale_plus 
```

This produces `Coyote/driver/build/coyote_driver.ko`.

**3. Programming the FPGA and inserting the driver**

On the ETHZ HACC cluster use the provided helper script (from the repository root):

```bash
bash Coyote/util/program_hacc_local.sh \
    Coyote/examples/09_perf_rdma/hw/build_hw/bitstreams/cyt_top.bit \
    Coyote/driver/build/coyote_driver.ko
```

The script programs the FPGA via `hdev` and inserts the driver. A successful load ends with `probe returning 0` in `sudo dmesg`. For networking, the script additionally passes the QSFP IP and MAC address of the node to the driver — check `Coyote/util/program_hacc_local.sh` to confirm these are set correctly for your machine.

On an independent (non-HACC) setup, program the bitstream through Vivado Hardware Manager, rescan PCIe (see `Coyote/util/hot_reset.sh`), then insert the driver manually:

```bash
sudo insmod Coyote/driver/build/coyote_driver.ko ip_addr=<qsfp_ip> mac_addr=<qsfp_mac>
```

**4. Software compilation (CREED–CREED experiments)**

Because client and server require different binaries, two separate CMake builds are needed:

```bash
cd Coyote/examples/09_perf_rdma/sw

mkdir build_server && cd build_server
cmake ../ -DINSTANCE=server && make
cd ..

mkdir build_client && cd build_client
cmake ../ -DINSTANCE=client && make
```

**5. Running CREED–CREED experiments**

Start the **server** binary first, then the **client**. Pass the server CPU's IP address (reachable via the management network, not the FPGA's QSFP IP) to the client with `-i`. Key parameters are:
- `-o` — operation: `0` = READ, `1` = WRITE
- `-x` / `-X` — minimum / maximum transfer size (bytes)
- `-r` — number of test runs

```bash
# On the server node
./build_server/test

# On the client node
./build_client/test -i <server_cpu_ip> -o 1 -x 64 -X 1048576 -r 100
```

The same bitstream is used on both nodes. Network statistics (packet drops, retransmissions) can be inspected with:

```bash
cat /sys/kernel/coyote_sysfs_0/cyt_attr_nstats
```


## Conducting experiments between CREED and commercial RNICs: 
This same bitstream can then also be used for throughput evaluation with other NICs as demonstrated in the paper. However, different software has to be used for that purpose. In this directory, we provide both Coyote/CREED-side software for an "inactive communication partner" that connects with the executing NIC and responds to incoming RDMA messages, as well as the IB-verbs based software that executes on the commercial NIC on the other side. The following instructions help to execute these experiments: 

### RNIC side: 
The subdirectory `/commercial_rnic_sw` contains the code `RNIC_Comp_Poll_Tester.cpp` required to run a RDMA performance benchmark from a commercial RNIC to a FPGA hosting CREED. CREED in this case behaves passively - receiving incoming RDMA messages, processing them and generating ACKs, which are then used for completion polling in the commercial RNIC. `RNIC_Comp_Poll_Tester.cpp` can be compiled by calling the bash-script `compilation_script.sh` and will generate the executable `RNIC_Comp_Poll_Tester`. By default, this program will use the first ib_device exposed in ibv_get_devices. To change this behaviour, one can adopt line 220 of the source code. The RNIC-side functions as server in the QP-exchange and thus has to be started first. For this purpose, the following arguments have to be used when calling the executable: 
- `t` - IP-address of the RDMA-interface as integer, used when exchanging meta-information with the CREED-instance on the other side. 
- `o` - RDMA-operation to be tested, either READ (0) or WRITE (1). 
- `n` - Minimum message size to test. 
- `x` - Maximum message size to test. 
- `r` - Number of throughput repetitions used for the test run. 
- `l` - Number of latency repetitions used for the test run. 
- `v` - Select to activate verbosity of debug printouts. 
- `a` - Overall number of transactions / repetitions for the throughput / latency experiment. 


### CREED side: 
For running the RDMA-throughput experiments with a commercial RNIC on the remote side, the same bitstreams (and drivers) as for the CREED-CREED experiments can be used. However, one needs to compile the software contained in this experiment folder for an `inactive client`, which essentially connects to the RNIC and then spins in a busy loop responding to the RDMA-transactions of the RNIC. After the remote side has finished (i.e. printing of the test results), the CREED-side software can be terminated manually. 
For the compilation of the `inactive client`, the same steps apply as for the general Coyote software examples. All paths in the cmake have been adopted to be compilable right at this location of the repository:

```bash
cd experiments/throughput_evaluation/coyote_extension
mkdir build && cd build
cmake ../
make
```

For running the client, the IP-address of the remote node (the RNIC) has to be given as argument `i`. The client has to be started secondly, while the server is already busy spinning, waiting for an incoming connection. 

### Baseline: 
The baseline experiments for RDMA-connections between commercial RNICs as shown in Figure 4 have been obtained by using the standard linux perftest utility (https://github.com/linux-rdma/perftest). Specifically, we used the `ib_write_bw`, `ib_read_bw`, `ib_write_lat` and `ib_read_lat` test commands.  