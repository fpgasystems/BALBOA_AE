# RoCE BALBOA: Service-Enhanced RDMA Offload Engine for Data Center SmartNICs
This repository contains the artifact of the OSDI '26 paper "RoCE BALBOA: Service-Enhanced RDMA Offload engine for Data Center SmartNICs". 
We introduce BALBOA, an open-source, 100G RoCE v2-compatible offload engine for SmartNICs. BALBOA allows to prototype RDMA-SmartNICs, i.e. on FPGA accelerator cards, and embed them into realistic datacenter networks with other, commercial RDMA-NICs. The open architecture of BALBOA allows to prototype and evaluate various offloads either on- or off-datapath. In the paper, we showcase this with encryption, Deep Packet Inspection and a preprocessing pipeline for Machine Learning workloads. 

### Hardware prerequisites
We have used the AMD Alveo U55C FPGA for all experiments in the paper. For that purpose, we have used the AMD-ETH Heterogeneous Accelerated Compute Cluster (HACC). Further information can be found at [here](https://github.com/fpgasystems/hacc). Any of the nodes alveo-u55c-[01-10] can be used for the NIC-only experiments. The ML-preprocessing workload requires a node that features AMD MI210 GPUs besides the mentioned FPGA. In the HACC, this is the case for hacc-box-03. The HACC is accessible for any researcher upon request / application, so that it can also serve as infrastructure for further work with BALBOA.

To get started with this artifact, please clone this repository: 
```
git clone --recurse-submodules https://github.com/fpgasystems/BALBOA_AE.git
```

To run the specific experiments, refer to the README.md files in the various subfolders as described in the next section on the structure of the repository. 


### Repo Structure 
- `Coyote/`: Coyote v2 is a FPGA-shell that functions as "Operating System" for FPGAs in the datacenter. By default, it uses BALBOA for RoCE v2 networking. For this reason, the actual BALBOA design (packet processing pipeline etc.) is included in the current open-source Coyote v2 release as a submodule and can be inspected there. This repository includes Coyote v2 as a submodule. The actual RoCE BALBOA can be found at `Coyote/hw/services/network`. This folder contains both a cmake-file for HLS-compilation and the subfolder `Coyote/hw/services/network/hls` with the HLS-files that define the pipeline. The head of this included subrepository at [here](https://github.com/fpgasystems/fpga-network-stack/tree/master) contains a detailed description of the RDMA-stack and its communication interfaces to surrounding FPGA-infrastructure. Within the HLS-subfolder, the directory `rocev2/` contains the packet processing framework and includes the sub-pipelines found in `ipv4/`, `udp/` and `ib_transport_protocol/` for the various header layers as explained in the paper. 
- `experiments/`: Includes instructions and code for all the experiments showcased in the paper. This can involve either experiments directly run within the Coyote repository or additional code-snippets that need to be integrated with Coyote. 


## Paper Citation 

If you find this repository useful, please use the following citation: 

```latex
@inproceedings{heer2026balboa,
    title = {RoCE BALBOA: Service-Enhanced RDMA Offload Engine for Data Center SmartNICs},
    author = {Heer, Maximilian Jakob and Ramhorst, Benjamin and Zhu, Yu and Liu, Luhao and Hu, Zhiyi and Dann, Jonas and Alonso, Gustavo},
    year = 2026,
    booktitle= {Proceedings of the 20th USENIX Conference on Operating Systems Design and Implementation},
    publisher = {USENIX association},
    address = {USA}, 
    location = {Seattle, WA, USA},
    series = {OSDI '26}
 }
```