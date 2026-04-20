/**
  * Copyright (c) 2021-2024, Systems Group, ETH Zurich
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *
  * 1. Redistributions of source code must retain the above copyright notice,
  * this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  * this list of conditions and the following disclaimer in the documentation
  * and/or other materials provided with the distribution.
  * 3. Neither the name of the copyright holder nor the names of its contributors
  * may be used to endorse or promote products derived from this software
  * without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

  #include <any>
  #include <iostream>
  #include <cstdlib>
  
  // External library for easier parsing of CLI arguments by the executable
  #include <boost/program_options.hpp>
  
  // Coyote-specific includes
  #include "cThread.hpp"
  #include "constants.hpp"
  
  constexpr bool const IS_CLIENT = false;
  
  int main(int argc, char *argv[])  {
      // CLI arguments
      bool operation;
      unsigned int min_size, max_size, n_runs, n_parallel_qps;
  
      boost::program_options::options_description runtime_options("Coyote Perf RDMA Options");
      runtime_options.add_options()
          ("operation,o", boost::program_options::value<bool>(&operation)->default_value(false), "Benchmark operation: READ(0) or WRITE(1)")
          ("runs,r", boost::program_options::value<unsigned int>(&n_runs)->default_value(10), "Number of times to repeat the test")
          ("min_size,x", boost::program_options::value<unsigned int>(&min_size)->default_value(64), "Starting (minimum) transfer size")
          ("max_size,X", boost::program_options::value<unsigned int>(&max_size)->default_value(1 * 1024 * 1024), "Ending (maximum) transfer size")
          ("parallel_QPs,p", boost::program_options::value<unsigned int>(&n_parallel_qps)->default_value(1), "Number of parallel QPs to use");
      boost::program_options::variables_map command_line_arguments;
      boost::program_options::store(boost::program_options::parse_command_line(argc, argv, runtime_options), command_line_arguments);
      boost::program_options::notify(command_line_arguments);
  
      PR_HEADER("CLI PARAMETERS:");
      std::cout << "Benchmark operation: " << (operation ? "WRITE" : "READ") << std::endl;
      std::cout << "Number of test runs: " << n_runs << std::endl;
      std::cout << "Starting transfer size: " << min_size << std::endl;
      std::cout << "Ending transfer size: " << max_size << std::endl << std::endl;
      std::cout << "Number of parallel QPs: " << n_parallel_qps << std::endl;
  
      // Allocate Coyothe thread and set-up RDMA connections, buffer etc.
      // initRDMA is explained in more detail in client/main.cpp
      std::vector<std::unique_ptr<coyote::cThread<std::any>>> qp_threads;
      std::vector<bool> transfer_done; 
      std::vector<int *> h_mems;
      std::vector<coyote::sgEntry> sg_list;

      // Prep: Do the QP-exchange for all the requires QPs 
      for(unsigned int i = 0; i < n_parallel_qps; i++) {

          // Create a new cThread for each QP
          qp_threads.emplace_back(new coyote::cThread<std::any>(DEFAULT_VFPGA_ID, getpid(), 0));


          // Initialize the RDMA connection
          int *mem = (int *) qp_threads[i]->initRDMA(max_size, coyote::defPort+i);
          h_mems.emplace_back(mem);

          // None of the transfers is done, so set everything to false
          transfer_done.push_back(false);

          coyote::sgEntry sg;
          sg.rdma = { .len = max_size };
          sg_list.emplace_back(sg);


          printf("Creating a QP for %d\n", i);
      }

      // Answer all the transfers 
      for(unsigned int i = 0; i < n_runs; i++) {
            printf("Starting run %d\n", i);
            // Clear the threads and reset the done-signals 
                for(unsigned int j = 0; j < n_parallel_qps; j++) {
                    printf("Clearing QP %d\n", j);

                    // qp_threads[j]->clearCompleted();
                    transfer_done[j] = false;

                    // Busy loop
                    while(true) { } 
                }
      } 

        // Final Sync and exit
        for(unsigned int j = 0; j < n_parallel_qps; j++) {
            printf("Sync for QP %d\n", j);
            qp_threads[j]->connSync(IS_CLIENT);
        }
    
  }