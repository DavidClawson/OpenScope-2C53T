// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_trigger_comparator;

    localparam int unsigned DATA_WIDTH = 8;

    logic                  clk;
    logic                  reset_n;
    logic                  arm_enable;
    logic                  sample_valid;
    logic [DATA_WIDTH-1:0] sample_data;
    logic [DATA_WIDTH-1:0] trigger_level;
    logic                  at_or_above_level;
    logic                  crossing_event;

    int unsigned failures;

    always #5 clk = ~clk;

    trigger_comparator #(
        .DATA_WIDTH(DATA_WIDTH)
    ) dut (
        .clk(clk),
        .reset_n(reset_n),
        .arm_enable(arm_enable),
        .sample_valid(sample_valid),
        .sample_data(sample_data),
        .trigger_level(trigger_level),
        .at_or_above_level(at_or_above_level),
        .crossing_event(crossing_event)
    );

    task automatic sample_and_expect(
        input logic [DATA_WIDTH-1:0] value,
        input logic                  valid,
        input logic                  expected_above,
        input logic                  expected_crossing,
        input string                 label
    );
        begin
            @(negedge clk);
            sample_data = value;
            sample_valid = valid;
            @(posedge clk);
            #1;
            if (at_or_above_level !== expected_above) begin
                $error("%s: at_or_above_level got %b, expected %b",
                       label, at_or_above_level, expected_above);
                failures++;
            end
            if (crossing_event !== expected_crossing) begin
                $error("%s: crossing_event got %b, expected %b",
                       label, crossing_event, expected_crossing);
                failures++;
            end
        end
    endtask

    initial begin
        clk = 1'b0;
        reset_n = 1'b0;
        arm_enable = 1'b0;
        sample_valid = 1'b0;
        sample_data = '0;
        // The bench-anchored case: stock's armed level is 0xAD (173).
        trigger_level = 8'had;
        failures = 0;

        repeat (2) @(posedge clk);
        #1;
        reset_n = 1'b1;

        // Negative control: disarmed samples above the level do nothing.
        sample_and_expect(8'hf0, 1'b1, 1'b0, 1'b0, "disarmed above level");

        // The first armed sample only seeds history, even when it is already
        // at or above the level; no crossing may fire without a prior
        // below-level sample.
        @(negedge clk);
        arm_enable = 1'b1;
        sample_and_expect(8'hf0, 1'b1, 1'b1, 1'b0, "seed sample above level");
        sample_and_expect(8'hf0, 1'b1, 1'b1, 1'b0, "held above level");

        // Quiet-noise regime: samples below 0xAD never produce events, the
        // bench observation behind stock's zero edge rate at level 0xAD.
        sample_and_expect(8'h52, 1'b1, 1'b0, 1'b0, "noise floor low");
        sample_and_expect(8'h65, 1'b1, 1'b0, 1'b0, "noise floor high");

        // A rising crossing fires exactly one event and then holds.
        sample_and_expect(8'hc0, 1'b1, 1'b1, 1'b1, "rising crossing");
        sample_and_expect(8'hc4, 1'b1, 1'b1, 1'b0, "stays above");

        // The declared contract is rising-only: the falling crossing is
        // silent, and the next rising crossing fires again.
        sample_and_expect(8'h40, 1'b1, 1'b0, 1'b0, "falling crossing silent");
        sample_and_expect(8'hb0, 1'b1, 1'b1, 1'b1, "second rising crossing");

        // Unqualified cycles change nothing even with crossing-shaped data.
        sample_and_expect(8'h10, 1'b0, 1'b1, 1'b0, "invalid low ignored");
        sample_and_expect(8'hf0, 1'b0, 1'b1, 1'b0, "invalid high ignored");

        // Lowering the level into the band reproduces the level-0x37 bench
        // experiment: previously silent readings start crossing.
        @(negedge clk);
        trigger_level = 8'h37;
        sample_and_expect(8'h20, 1'b1, 1'b0, 1'b0, "below moved level");
        sample_and_expect(8'h52, 1'b1, 1'b1, 1'b1, "noise crosses moved level");

        // Disarm clears comparison history; re-arm needs a fresh seed.
        @(negedge clk);
        arm_enable = 1'b0;
        sample_and_expect(8'hf0, 1'b1, 1'b0, 1'b0, "disarm clears state");
        @(negedge clk);
        arm_enable = 1'b1;
        sample_and_expect(8'hf0, 1'b1, 1'b1, 1'b0, "re-armed seed sample");

        if (failures != 0) begin
            $fatal(1, "trigger_comparator failed with %0d error(s)", failures);
        end

        $display("PASS: seed suppression, rising-only events, level moves, qualification, and disarm reset");
        $finish;
    end

endmodule
