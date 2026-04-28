# RoCE CREED: Service-Enhanced RDMA Offload Engine for Data Center SmartNICs
This repository contains the artifact of the OSDI '26 paper "RoCE CREED: Service-Enhanced RDMA Offload engine for Data Center SmartNICs". 
We introduce CREED, an open-source, 100G RoCE v2-compatible offload engine for SmartNICs. CREED allows to prototype RDMA-SmartNICs, i.e. on FPGA accelerator cards, and embed them into realistic datacenter networks with other, commercial RDMA-NICs. The open architecture of CREED allows to prototype and evaluate various offloads either on- or off-datapath. In the paper, we showcase this with encryption, Deep Packet Inspection and a preprocessing pipeline for Machine Learning workloads. 

### Hardware prerequisites
We have used the AMD Alveo U55C FPGA for all experiments in the paper. For that purpose, we have used the AMD-ETH Heterogeneous Accelerated Compute Cluster (HACC). Further information can be found at [here](https://github.com/fpgasystems/hacc). Any of the nodes alveo-u55c-[01-10] can be used for the NIC-only experiments. The ML-preprocessing workload requires a node that features AMD MI210 GPUs besides the mentioned FPGA. In the HACC, this is the case for hacc-box-03. Other experiments (mainly throughput evaluation and software baseline for AES CTR) also require machines with commercial RNICs (Mellanox CX, Bluefield etc.). The HACC is accessible for any researcher upon request / application, so that it can also serve as infrastructure for further work with CREED.

To get started with this artifact, please clone this repository: 
```
git clone --recurse-submodules https://github.com/fpgasystems/CREED_AE.git
```

To run the specific experiments, refer to the README.md files in the various subfolders as described in the next section on the structure of the repository. 


### Repo Structure 
- `Coyote/`: Coyote v2 is a FPGA-shell that functions as "Operating System" for FPGAs in the datacenter. By default, it uses CREED for RoCE v2 networking. For this reason, the actual CREED design (packet processing pipeline etc.) is included in the current open-source Coyote v2 release as a submodule and can be inspected there. This repository includes Coyote v2 as a submodule, which again pulls CREED as a submodule for RDMA-connectivity. One part of the actual RoCE CREED can be found at `Coyote/hw/services/network`, which contains both a cmake-file for HLS-compilation and the subfolder `Coyote/hw/services/network/hls` with the HLS-files that define the pipeline (section 4.3 of the paper). Within the HLS-subfolder, the directory `rocev2/` contains the packet processing framework and includes the sub-pipelines found in `ipv4/`, `udp/` and `ib_transport_protocol/` for the various header layers as explained in the paper. Besides this, `Coyote/hw/hdl/network/rdma` contains the other hardware modules described in the paper: `icrc.sv` contains the checksum pipeline (section 4.6), `rdma_flow.sv` the ACK-clocked flow control (section 4.5) and `rdma_mux_retrans.sv` the retransmission logic (section 4.4) The full integration of all modules together with the packet processing pipeline as described in section 4.2 and displayed in Figure 2 is realized within `roce_stack.sv`. 
- `experiments/`: Includes instructions and code for all the experiments showcased in the paper. This can involve either experiments directly run within the Coyote repository or additional code-snippets that need to be integrated with Coyote. The experiments map to the paper sections as following: 
    - `experiments/throughput_evaluation` maps to the microbenchmarks for RDMA-performance (both throughput and latency) demonstrated in section 6.1 of the paper. 
    - `experiments/bandwidth_distribution` contains the code for the experiment shown in section 6.2 of the paper, where throughput distribution over multiple parallel QPs is plotted. 
    - `experiments/aes_ctr` has the code for the offloaded AES-encryption and the CPU-baseline shown in section 6.3.1 of the paper. 
    - `experiments/ml_based_dpi` has the instructions how to recreate the Deep Packet Inspection experiment shown in section 6.3.2 of the paper. 
    - `experiments/ml_preprocessing` has the code and instructions on how to recreate the offloaded ML-preprocessing tasks (and diverse baselines) as originally shown in section 8 of the paper. 
- `balboa_dpi`: The CREED experiment for ML-based Deep Packet Inspection has been described in-depth in its own artifact repository, which is included here as subrepo. It contains all relevant descriptions to build the required FPGA-hardware and execute the experiments used for the paper. 


### Paper Citation 

If you find this repository useful, please use the following citation: 

```latex
@inproceedings{heer2026CREED,
    title = {RoCE CREED: Service-Enhanced RDMA Offload Engine for Data Center SmartNICs},
    author = {Heer, Maximilian Jakob and Ramhorst, Benjamin and Zhu, Yu and Liu, Luhao and Hu, Zhiyi and Dann, Jonas and Alonso, Gustavo},
    year = 2026,
    booktitle= {Proceedings of the 20th USENIX Conference on Operating Systems Design and Implementation},
    publisher = {USENIX association},
    address = {USA}, 
    location = {Seattle, WA, USA},
    series = {OSDI '26}
 }
```

### Additional remark
We are in contact with our shepherd and the conference chairs to officially rename the paper to "RoCE BALBOA: Service-Enhanced RDMA Offload Engine for Data Center SmartNICs", thus naming the presented system BALBOA rather than CREED. We originally switched the system name for reasons of double-blindness. As we have no final confirmation for being allowed to switch back to BALBOA, this artefact sticks to "CREED" as system name, but reflects the original "BALBOA" naming convention in some file- and path names. 