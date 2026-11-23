// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_rate_divider;

    localparam int unsigned DIVISOR_WIDTH = 16;

    logic                     clk;
    logic                     reset_n;
    logic                     enable;
    logic [7:0]               raw_divisor;
    logic                     divided_tick;

    int unsigned failures;

    always #5 clk = ~clk;

    rate_divider #(
        .SELECT_WIDTH(8),
        .DIVISOR_WIDTH(DIVISOR_WIDTH),
        .BASE_DIVISOR(16'd99),
        .LEGACY_SLOW_DEFAULT_DIVISOR(16'd3)
    ) dut (
        .clk(clk),
        .reset_n(reset_n),
        .enable(enable),
        .raw_divisor(raw_divisor),
        .divided_tick(divided_tick)
    );

    task automatic clock_and_expect(
        input logic  expected_tick,
        input string label
    );
        begin
            @(posedge clk);
            #1;
            if (divided_tick !== expected_tick) begin
                $error("%s: divided_tick got %b, expected %b",
                       label, divided_tick, expected_tick);
                failures++;
            end
        end
    endtask

    initial begin
        clk = 1'b0;
        reset_n = 1'b0;
        enable = 1'b0;
        raw_divisor = 8'h08;
        failures = 0;

        repeat (2) @(posedge clk);
        #1;
        reset_n = 1'b1;

        // Negative control: a disabled divider never ticks.
        clock_and_expect(1'b0, "disabled cycle 1");
        clock_and_expect(1'b0, "disabled cycle 2");

        // 0x08 retains the old slow-default interval (divisor 3 => 4 cycles).
        @(negedge clk);
        enable = 1'b1;
        clock_and_expect(1'b0, "phase 0");
        clock_and_expect(1'b0, "phase 1");
        clock_and_expect(1'b0, "phase 2");
        clock_and_expect(1'b1, "phase 3 tick");
        clock_and_expect(1'b0, "next phase 0");
        clock_and_expect(1'b0, "next phase 1");
        clock_and_expect(1'b0, "next phase 2");
        clock_and_expect(1'b1, "next phase 3 tick");

        // Disable clears the phase; re-enable restarts a full interval.
        @(negedge clk);
        enable = 1'b0;
        clock_and_expect(1'b0, "disable mid-interval");
        @(negedge clk);
        enable = 1'b1;
        clock_and_expect(1'b0, "restarted phase 0");
        clock_and_expect(1'b0, "restarted phase 1");
        clock_and_expect(1'b0, "restarted phase 2");
        clock_and_expect(1'b1, "restarted phase 3 tick");

        // 0x0f is the measured 30 kS/s base interval; with BASE_DIVISOR=99
        // this is 100 cycles including the terminal count.
        @(negedge clk);
        enable = 1'b0;
        raw_divisor = 8'h0f;
        clock_and_expect(1'b0, "zero divisor while disabled");
        @(negedge clk);
        enable = 1'b1;
        repeat (99) clock_and_expect(1'b0, "0f base interval");
        clock_and_expect(1'b1, "0f base tick");

        // 0x0e is twice the measured rate, hence half the divider interval.
        @(negedge clk);
        enable = 1'b0;
        raw_divisor = 8'h0e;
        clock_and_expect(1'b0, "0e disabled");
        @(negedge clk);
        enable = 1'b1;
        repeat (49) clock_and_expect(1'b0, "0e interval");
        clock_and_expect(1'b1, "0e tick");

        // 0x10 is the measured 15 kS/s further-slow step (2x the 0f divisor).
        @(negedge clk);
        enable = 1'b0;
        raw_divisor = 8'h10;
        clock_and_expect(1'b0, "10 disabled");
        @(negedge clk);
        enable = 1'b1;
        repeat (198) clock_and_expect(1'b0, "10 interval");
        clock_and_expect(1'b1, "10 tick");

        if (failures != 0) begin
            $fatal(1, "rate_divider failed with %0d error(s)", failures);
        end

        $display("PASS: legacy mode, measured ladder decode, disable reset, and 15 kS/s step");
        $finish;
    end

endmodule
