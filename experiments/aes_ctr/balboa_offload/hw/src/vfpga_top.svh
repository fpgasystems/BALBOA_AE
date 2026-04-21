/**
 * This file is part of the Coyote <https://github.com/fpgasystems/Coyote>
 *
 * MIT Licence
 * Copyright (c) 2025, Systems Group, ETH Zurich
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

always_comb begin 
    /*
     * CONTROL SIGNALS
     * 
     * rq_(wr|rd) are two more Coyote interfaces, which act as inputs to the user application
     * They corresponds to network write/read requests, set from the host software and driver
     * Here, they are used to set Coyote's generic send queues, previously discussed in Example 7.
     */
    // Write
    sq_wr.valid = rq_wr.valid;
    rq_wr.ready = sq_wr.ready;
    sq_wr.data = rq_wr.data;            // Data field holds information such as remote, virtual address, buffer length etc.
    sq_wr.data.strm = STRM_HOST;        // For RDMA, by definition data is always on the host
    sq_wr.data.dest = is_opcode_rd_resp(rq_wr.data.opcode) ? 0 : 1;

    // Reads
    sq_rd.valid = rq_rd.valid;
    rq_rd.ready = sq_rd.ready;
    sq_rd.data = rq_rd.data;           // Data field holds information such as remote, virtual address, buffer length etc.
    sq_rd.data.strm = STRM_HOST;       // For RDMA, by definition data is always on the host
    sq_rd.data.dest = 1;
end

/*
 * DATA SIGNALS
 * 
 */
// Data streams for outgoing RDMA WRITEs (from local host to network stack to remote node)
// `AXISR_ASSIGN(axis_host_recv[0], axis_rreq_send[0])

// Data streams for incoming RDMA READ RESPONSEs (from remote node to network stack to local host)
`AXISR_ASSIGN(axis_rreq_recv[0], axis_host_send[0])

// Data streams for outgoing RDMA READ RESPONSEs (from local host to network stack to remote node)
`AXISR_ASSIGN(axis_host_recv[1], axis_rrsp_send[0])

// Data streams for incoming RDMA WRITEs (from remote node to network stack to local host)
// `AXISR_ASSIGN(axis_rrsp_recv[0], axis_host_send[1])


// -------------------------------
// CRYPTO CORES
// -------------------------------
logic [31:0] block_counter_outgoing_data;
logic [31:0] block_counter_incoming_data;
localparam  [95:0] nonce_constant = 96'h0000_0000_0000_0000_0000_0001; // Example nonce, should be unique per session
logic [511:0] counter_input_outgoing_data;
logic [511:0] counter_input_incoming_data;

localparam [2047:0] aes_key_constant = 2048'h000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f; // Example key, should be kept secret

// Increment the counters for outgoing and incoming data 
always_ff@(posedge aclk) begin 
    if(!aresetn) begin 
        block_counter_incoming_data <= 32'd0;
        block_counter_outgoing_data <= 32'd0;
    end else begin 
        if(axis_host_recv[0].tvalid && axis_host_recv[0].tready && axis_host_recv[0].tlast) begin 
            block_counter_outgoing_data <= block_counter_outgoing_data + 32'd4;
        end

        if(axis_rrsp_recv[0].tvalid && axis_rrsp_recv[0].tready && axis_rrsp_recv[0].tlast) begin 
            block_counter_incoming_data <= block_counter_incoming_data + 32'd4;
        end
    end 
end 

// Concatinate nonce and block counter to form the CTR input 
assign counter_input_outgoing_data = {nonce_constant, (block_counter_outgoing_data + 32'd3), 
                                       nonce_constant, (block_counter_outgoing_data + 32'd2), 
                                       nonce_constant, (block_counter_outgoing_data + 32'd1), 
                                       nonce_constant, (block_counter_outgoing_data + 32'd0)};

assign counter_input_incoming_data = {nonce_constant, (block_counter_incoming_data + 32'd3), 
                                       nonce_constant, (block_counter_incoming_data + 32'd2), 
                                       nonce_constant, (block_counter_incoming_data + 32'd1), 
                                       nonce_constant, (block_counter_incoming_data + 32'd0)};

// Instantiate an ILA to monitor encryption/decryption performance
ila_aes_perf_rdma inst_ila_aes_perf_rdma (
    .clk(aclk),
    .probe0(axis_host_recv[0].tvalid),      // 1
    .probe1(axis_host_recv[0].tready),      // 1
    .probe2(axis_host_recv[0].tlast),       // 1
    .probe3(axis_host_recv[0].tdata),       // 512
    .probe4(axis_host_recv[0].tkeep),       // 64
    .probe5(axis_rreq_send[0].tvalid),      // 1
    .probe6(axis_rreq_send[0].tready),      // 1
    .probe7(axis_rreq_send[0].tlast),       // 1
    .probe8(axis_rreq_send[0].tdata),       // 512
    .probe9(axis_rreq_send[0].tkeep),       // 64
    .probe10(block_counter_outgoing_data),  // 32
    .probe11(axis_rrsp_recv[0].tvalid),     // 1
    .probe12(axis_rrsp_recv[0].tready),     // 1
    .probe13(axis_rrsp_recv[0].tlast),      // 1
    .probe14(axis_rrsp_recv[0].tdata),      // 512
    .probe15(axis_rrsp_recv[0].tkeep),      // 64
    .probe16(axis_host_send[1].tvalid),     // 1
    .probe17(axis_host_send[1].tready),     // 1
    .probe18(axis_host_send[1].tlast),      // 1
    .probe19(axis_host_send[1].tdata),      // 512
    .probe20(axis_host_send[1].tkeep),      // 64
    .probe21(block_counter_incoming_data)   // 32
);

// Encryption for outgoing data (RDMA WRITEs)
aes_top #(
    .NPAR(AXI_DATA_BITS / 128), 
    .OPERATION(0), 
    .MODE(1)
) inst_aes_write_encryption (
    .clk(aclk), 
    .reset_n(aresetn), 
    .stall(~axis_rreq_send[0].tready),

    // Key Treatment
    .key_in(aes_key_constant), 

    // Input stream (from host)
    .data_in(axis_host_recv[0].tdata),
    .dVal_in(axis_host_recv[0].tvalid),
    .keep_in(axis_host_recv[0].tkeep),
    .last_in(axis_host_recv[0].tlast),

    // Output stream (to network)
    .data_out(axis_rreq_send[0].tdata),
    .dVal_out(axis_rreq_send[0].tvalid),
    .keep_out(axis_rreq_send[0].tkeep),
    .last_out(axis_rreq_send[0].tlast),

    // CTR input 
    .cntr_in(counter_input_outgoing_data)
); 

// Combine the ready-signals 
assign axis_host_recv[0].tready = axis_rreq_send[0].tready;

// Decryption for incoming data (RDMA WRITEs)
aes_top #(
    .NPAR(AXI_DATA_BITS / 128), 
    .OPERATION(0), 
    .MODE(1)
) inst_aes_write_decryption (
    .clk(aclk), 
    .reset_n(aresetn), 
    .stall(~axis_host_send[1].tready),

    // Key Treatment
    .key_in(aes_key_constant), 

    // Input stream (from network)
    .data_in(axis_rrsp_recv[0].tdata),
    .dVal_in(axis_rrsp_recv[0].tvalid),
    .keep_in(axis_rrsp_recv[0].tkeep),
    .last_in(axis_rrsp_recv[0].tlast),

    // Output stream (to host)
    .data_out(axis_host_send[1].tdata),
    .dVal_out(axis_host_send[1].tvalid),
    .keep_out(axis_host_send[1].tkeep),
    .last_out(axis_host_send[1].tlast),

    // CTR input 
    .cntr_in(counter_input_incoming_data)
); 

// Combine the ready-signals
assign axis_rrsp_recv[0].tready = axis_host_send[1].tready;

// Tie off unused interfaces
always_comb axi_ctrl.tie_off_s();
always_comb notify.tie_off_m();
always_comb cq_rd.tie_off_s();
always_comb cq_wr.tie_off_s();

// ILA for debugging
ila_perf_rdma inst_ila_perf_rdma (
    .clk(aclk),
    .probe0(axis_host_recv[0].tvalid),      // 1
    .probe1(axis_host_recv[0].tready),      // 1
    .probe2(axis_host_recv[0].tlast),       // 1

    .probe3(axis_host_recv[1].tvalid),      // 1
    .probe4(axis_host_recv[1].tready),      // 1
    .probe5(axis_host_recv[1].tlast),       // 1

    .probe6(axis_host_send[0].tvalid),      // 1
    .probe7(axis_host_send[0].tready),      // 1
    .probe8(axis_host_send[0].tlast),       // 1

    .probe9(axis_host_send[1].tvalid),      // 1
    .probe10(axis_host_send[1].tready),     // 1
    .probe11(axis_host_send[1].tlast),      // 1

    .probe12(sq_wr.valid),                  // 1
    .probe13(sq_wr.ready),                  // 1
    .probe14(sq_wr.data),                   // 128
    .probe15(sq_rd.valid),                  // 1
    .probe16(sq_rd.ready),                  // 1
    .probe17(sq_rd.data)                    // 128
);
