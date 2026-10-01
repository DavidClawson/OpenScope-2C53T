// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module trigger_timebase #(
    parameter int unsigned COUNTER_WIDTH = 16
) (
    input  logic                     clk,
    input  logic                     reset_n,
    // Raw active-high local arm gate. Stock source, polarity, and re-arm
    // sequencing remain unknown.
    input  logic                     raw_arm_enable,

    // Raw reconstruction control, not a recovered stock register encoding.
    // This local contract emits one sample_tick every
    // (raw_interval_minus_one + 1) active clock cycles.
    input  logic [COUNTER_WIDTH-1:0] raw_interval_minus_one,

    // Raw event from an external comparator/decoder. Threshold, source, edge
    // polarity, holdoff, and stock command encoding are intentionally outside
    // this primitive because none of those semantics has been recovered.
    input  logic                     raw_trigger_event,

    output logic                     sample_tick,
    output logic                     trigger_pulse,
    output logic                     trigger_seen
);

    logic [COUNTER_WIDTH-1:0] interval_counter;

    // Hypothesis boundary: a programmable periodic enable is useful for an
    // editable timebase, but the stock raw capture memories are evidenced on
    // an undivided PLL clock and the slower computed path uses an unresolved
    // gated fabric clock. This counter therefore proves only the declared
    // local behavior; it is not claimed to match either stock clock path.
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            interval_counter <= '0;
            sample_tick      <= 1'b0;
            trigger_pulse    <= 1'b0;
            trigger_seen     <= 1'b0;
        end else if (!raw_arm_enable) begin
            interval_counter <= '0;
            sample_tick      <= 1'b0;
            trigger_pulse    <= 1'b0;
            trigger_seen     <= 1'b0;
        end else begin
            sample_tick   <= 1'b0;
            trigger_pulse <= 1'b0;

            if (interval_counter == raw_interval_minus_one) begin
                interval_counter <= '0;
                sample_tick      <= 1'b1;
            end else begin
                interval_counter <= interval_counter + 1'b1;
            end

            // Latch only the first event in an armed interval. What capture
            // does after this pulse (freeze, post-trigger count, or re-arm) is
            // deliberately left to an integration layer.
            if (raw_trigger_event && !trigger_seen) begin
                trigger_pulse <= 1'b1;
                trigger_seen  <= 1'b1;
            end
        end
    end

endmodule
