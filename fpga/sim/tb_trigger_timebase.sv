// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_trigger_timebase;

    localparam int unsigned COUNTER_WIDTH = 4;

    logic                     clk;
    logic                     reset_n;
    logic                     raw_arm_enable;
    logic [COUNTER_WIDTH-1:0] raw_interval_minus_one;
    logic                     raw_trigger_event;
    logic                     sample_tick;
    logic                     trigger_pulse;
    logic                     trigger_seen;

    int unsigned failures;

    always #5 clk = ~clk;

    trigger_timebase #(
        .COUNTER_WIDTH(COUNTER_WIDTH)
    ) dut (
        .clk(clk),
        .reset_n(reset_n),
        .raw_arm_enable(raw_arm_enable),
        .raw_interval_minus_one(raw_interval_minus_one),
        .raw_trigger_event(raw_trigger_event),
        .sample_tick(sample_tick),
        .trigger_pulse(trigger_pulse),
        .trigger_seen(trigger_seen)
    );

    task automatic clock_and_expect(
        input logic expected_tick,
        input logic expected_pulse,
        input logic expected_seen,
        input string label
    );
        begin
            @(posedge clk);
            #1;

            if (sample_tick !== expected_tick) begin
                $error("%s: sample_tick got %b, expected %b",
                       label, sample_tick, expected_tick);
                failures++;
            end
            if (trigger_pulse !== expected_pulse) begin
                $error("%s: trigger_pulse got %b, expected %b",
                       label, trigger_pulse, expected_pulse);
                failures++;
            end
            if (trigger_seen !== expected_seen) begin
                $error("%s: trigger_seen got %b, expected %b",
                       label, trigger_seen, expected_seen);
                failures++;
            end
        end
    endtask

    initial begin
        clk = 1'b0;
        reset_n = 1'b0;
        raw_arm_enable = 1'b0;
        raw_interval_minus_one = COUNTER_WIDTH'(2);
        raw_trigger_event = 1'b0;
        failures = 0;

        repeat (2) @(posedge clk);
        #1;
        reset_n = 1'b1;

        // Negative control: neither timebase nor trigger progresses disarmed.
        raw_trigger_event = 1'b1;
        clock_and_expect(1'b0, 1'b0, 1'b0, "disarmed raw event");
        raw_trigger_event = 1'b0;
        clock_and_expect(1'b0, 1'b0, 1'b0, "disarmed idle");

        // raw_interval_minus_one=2 means one tick every three armed clocks.
        @(negedge clk);
        raw_arm_enable = 1'b1;
        clock_and_expect(1'b0, 1'b0, 1'b0, "interval cycle 1");
        clock_and_expect(1'b0, 1'b0, 1'b0, "interval cycle 2");
        clock_and_expect(1'b1, 1'b0, 1'b0, "interval cycle 3");
        clock_and_expect(1'b0, 1'b0, 1'b0, "next interval cycle 1");

        // The first event pulses and latches; later events remain ignored.
        @(negedge clk);
        raw_trigger_event = 1'b1;
        clock_and_expect(1'b0, 1'b1, 1'b1, "first trigger event");
        clock_and_expect(1'b1, 1'b0, 1'b1, "held trigger event");
        @(negedge clk);
        raw_trigger_event = 1'b0;
        clock_and_expect(1'b0, 1'b0, 1'b1, "trigger latch persists");

        // Disarm clears the phase and trigger latch. Re-arm restarts from the
        // first interval cycle rather than leaking the old counter state.
        @(negedge clk);
        raw_arm_enable = 1'b0;
        clock_and_expect(1'b0, 1'b0, 1'b0, "disarm clears state");
        @(negedge clk);
        raw_arm_enable = 1'b1;
        clock_and_expect(1'b0, 1'b0, 1'b0, "re-arm cycle 1");
        clock_and_expect(1'b0, 1'b0, 1'b0, "re-arm cycle 2");
        clock_and_expect(1'b1, 1'b0, 1'b0, "re-arm cycle 3");

        // Zero is explicitly defined as one tick per active clock.
        @(negedge clk);
        raw_arm_enable = 1'b0;
        raw_interval_minus_one = '0;
        clock_and_expect(1'b0, 1'b0, 1'b0, "zero interval while disarmed");
        @(negedge clk);
        raw_arm_enable = 1'b1;
        clock_and_expect(1'b1, 1'b0, 1'b0, "zero interval cycle 1");
        clock_and_expect(1'b1, 1'b0, 1'b0, "zero interval cycle 2");

        if (failures != 0) begin
            $fatal(1, "trigger_timebase failed with %0d error(s)", failures);
        end

        $display("PASS: periodic tick, first trigger latch, disarm reset, and disabled negative control");
        $finish;
    end

endmodule
