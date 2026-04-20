# Throughput Evaluation

The basic BALBOA-BALBOA throughput evaluation (FPGAs with BALBOA on both ends of the network connection) can be conducted as explained in `Coyote/examples/09_perf_rdma`, by designating one of the endpoints as server, the other as client of this connection. For this purpose, one needs to follow the instructions in the Coyote-repository to first build a Coyote-bitstream that includes BALBOA as RDMA-stack and a RDMA-dedicated vFPGA. This same bitstream can then also be used for throughput evaluation with other NICs as demonstrated in the paper. However, different software has to be used for that purpose. In this directory, we provide both Coyote/BALBOA-side software for an "inactive communication partner" that connects with the executing NIC and responds to incoming RDMA messages, as well as the IB-verbs based software that executes on the commercial NIC on the other side. The following instructions help to execute these experiments: 

### BALBOA side: 


### RNIC side: 
The subdirectory `/commercial_rnic_sw` contains the code `RNIC_Comp_Poll_Tester.cpp` required to run a RDMA performance benchmark from a commercial RNIC to a FPGA hosting BALBOA. BALBOA in this case behaves passively - receiving incoming RDMA messages, processing them and generating ACKs, which are then used for completion polling in the commercial RNIC. `RNIC_Comp_Poll_Tester.cpp` can be compiled by calling the bash-script `compilation_script.sh` and will generate the executable `RNIC_Comp_Poll_Tester`. By default, this program will use the first ib_device exposed in ibv_get_devices. To change this behaviour, one can adopt line 220 of the source code. The RNIC-side functions as server in the QP-exchange and thus has to be started first. For this purpose, the following arguments have to be used when calling the executable: 
- `t` - IP-address of the RDMA-interface as integer, used when exchanging meta-information with the BALBOA-instance on the other side. 
- `o` - RDMA-operation to be tested, either READ (0) or WRITE (1). 
- `n` - Minimum message size to test. 
- `x` - Maximum message size to test. 
- `r` - Number of throughput repetitions used for the test run. 
- `l` - Number of latency repetitions used for the test run. 
- `v` - Select to activate verbosity of debug printouts. 
- `a` - Overall number of transactions / repetitions for the throughput / latency experiment. 


### BALBOA side: 
For running the RDMA-throughput experiments with a commercial RNIC on the remote side, the same bitstreams as for the BALBOA-BALBOA experiments can be used. However, one needs to compile the software contained in this experiment folder for an `inactive client`, which essentially connects to the RNIC and then spins in a busy loop responding to the RDMA-transactions of the RNIC. After the remote side has finished (i.e. printing of the test results), the BALBOA-side software can be terminated manually. 
For the compilation of the `inactive client`, the same steps apply as for the general Coyote software examples. All paths in the cmake have been adopted to be compilable right at this location of the repository. For running the client, the IP-address of the remote node (the RNIC) has to be given as argument `i`. The client has to be started secondly, while the server is already busy spinning, waiting for an incoming connection. 