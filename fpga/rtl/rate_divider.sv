// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module rate_divider #(
    parameter int unsigned DIVISOR_WIDTH = 8
) (
    input  logic                     clk,
    input  logic                     reset_n,
    input  logic                     enable,

    // Raw byte-shaped divisor with the same local contract as the timebase
    // interval: one divided_tick every (raw_divisor + 1) enabled clock cycles.
    input  logic [DIVISOR_WIDTH-1:0] raw_divisor,

    output logic                     divided_tick
);

    logic [DIVISOR_WIDTH-1:0] phase_counter;

    // Structural target (gw1n2-apicula M12): BSRAM_1/2 write on a gated
    // fabric clock whose gate cone is a self-feeding counter with an SPI-side
    // load path -- the structural definition of a programmable sample-rate
    // divider, not a bare enable.  This block reconstructs that shape on the
    // editable side: counter state feeding a periodic gate.  The stock divisor
    // value, its register address, and the mapping between the measured slow
    // default rate and stock's ~23x faster regime all remain unproven, so
    // this counter claims only the declared local behavior.
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            phase_counter <= '0;
            divided_tick  <= 1'b0;
        end else if (!enable) begin
            phase_counter <= '0;
            divided_tick  <= 1'b0;
        end else begin
            divided_tick <= 1'b0;
            if (phase_counter == raw_divisor) begin
                phase_counter <= '0;
                divided_tick  <= 1'b1;
            end else begin
                phase_counter <= phase_counter + 1'b1;
            end
        end
    end

endmodule
