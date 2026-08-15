// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module capture_channel #(
    parameter int unsigned DATA_WIDTH = 8,
    parameter int unsigned DEPTH = 1024,
    parameter int unsigned ADDR_WIDTH = (DEPTH <= 1) ? 1 : $clog2(DEPTH)
) (
    input  logic                  write_clk,
    input  logic                  reset_n,
    input  logic                  capture_enable,
    input  logic                  freeze,
    input  logic [DATA_WIDTH-1:0] sample_data,

    input  logic                  read_clk,
    input  logic [ADDR_WIDTH-1:0] read_addr,
    output logic [DATA_WIDTH-1:0] read_data,

    output logic [ADDR_WIDTH-1:0] write_ptr
);

    logic [DATA_WIDTH-1:0] sample_memory [0:DEPTH-1];

    // Keep RAM writes out of the asynchronous-reset process below. With the
    // write and registered read in their own clocked processes, Yosys 0.66
    // preserves this as a dual-port memory and synth_gowin maps the default
    // 1024 x 8 channels to DPX9B BSRAM primitives. The separate clocks still
    // do not establish the unrecovered CDC protocol or stock read ordering.
    always_ff @(posedge read_clk) begin
        read_data <= sample_memory[read_addr];
    end

    always_ff @(posedge write_clk) begin
        if (capture_enable && !freeze) begin
            sample_memory[write_ptr] <= sample_data;
        end
    end

    always_ff @(posedge write_clk or negedge reset_n) begin
        if (!reset_n) begin
            write_ptr <= '0;
        end else if (capture_enable && !freeze) begin
            // An explicit terminal comparison supports non-power-of-two DEPTH.
            if (write_ptr == ADDR_WIDTH'(DEPTH - 1)) begin
                write_ptr <= '0;
            end else begin
                write_ptr <= write_ptr + 1'b1;
            end
        end
    end

    // The read address is physical. After freezing, write_ptr identifies the
    // oldest entry once the buffer has wrapped; ordering policy belongs above
    // this primitive rather than being hidden in the storage block.

endmodule
