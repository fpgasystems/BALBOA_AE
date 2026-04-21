# AES CTR mode offload

Testing The AES CTR offload in BALBOA requires a two-fold approach to get both the hardware-accelerated numbers and the software baseline shown in the paper. The process for obtaining these numbers is described below: 

#### BALBOA-offloaded AES CTR hardware: 
For this purpose, new bitstreams need to be built, following the standard Coyote approach applied to the directory `aes_ctr/balboa_offload`. The paths in the Cmake-file have been edited to work from the current location of the repository. After obtaining the bitstream, one can use the "plain software" from `Coyote/examples/09_perf_rdma/sw` to run both latency and throughput numbers. 

#### SW-Baseline
