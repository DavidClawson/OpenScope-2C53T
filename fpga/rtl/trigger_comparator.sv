// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module trigger_comparator #(
    parameter int unsigned DATA_WIDTH = 8
) (
    input  logic                  clk,
    input  logic                  reset_n,
    input  logic                  arm_enable,
    input  logic                  sample_valid,
    input  logic [DATA_WIDTH-1:0] sample_data,
    input  logic [DATA_WIDTH-1:0] trigger_level,

    output logic                  at_or_above_level,
    output logic                  crossing_event
);

    logic level_history_valid;

    // Evidence: SPI register 0x08 is a digital post-ADC trigger level in ADC
    // codes, bench-proven in both directions (2026-08-14): quiet noise below
    // stock's armed level 0xAD produces no events, moving the level into the
    // noise band produces events, and restoring the level stops them.  The
    // comparator therefore belongs in the fabric, after the ADC, comparing
    // codes rather than analog voltages.
    //
    // Hypothesis boundary: a noise band straddling the level fires on rising
    // and falling readings alike, so the bench cannot separate edge polarity,
    // and stock's edge selection, hysteresis, holdoff, and trigger source
    // encoding remain unrecovered.  This block declares one local contract: a
    // one-cycle event on a rising crossing of trigger_level, evaluated only on
    // qualified samples.
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            at_or_above_level   <= 1'b0;
            level_history_valid <= 1'b0;
            crossing_event      <= 1'b0;
        end else if (!arm_enable) begin
            at_or_above_level   <= 1'b0;
            level_history_valid <= 1'b0;
            crossing_event      <= 1'b0;
        end else begin
            crossing_event <= 1'b0;
            if (sample_valid) begin
                at_or_above_level   <= (sample_data >= trigger_level);
                level_history_valid <= 1'b1;
                // The first qualified sample after arming only seeds the
                // comparison history; a crossing needs a preceding qualified
                // sample below the level.
                crossing_event <= level_history_valid && !at_or_above_level &&
                                  (sample_data >= trigger_level);
            end
        end
    end

endmodule
