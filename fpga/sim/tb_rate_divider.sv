// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_rate_divider;

    localparam int unsigned DIVISOR_WIDTH = 8;

    logic                     clk;
    logic                     reset_n;
    logic                     enable;
    logic [DIVISOR_WIDTH-1:0] raw_divisor;
    logic                     divided_tick;

    int unsigned failures;

    always #5 clk = ~clk;

    rate_divider #(
        .DIVISOR_WIDTH(DIVISOR_WIDTH)
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
        raw_divisor = DIVISOR_WIDTH'(3);
        failures = 0;

        repeat (2) @(posedge clk);
        #1;
        reset_n = 1'b1;

        // Negative control: a disabled divider never ticks.
        clock_and_expect(1'b0, "disabled cycle 1");
        clock_and_expect(1'b0, "disabled cycle 2");

        // Divisor 3 means one tick every four enabled cycles.
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

        // Divisor 0 is the declared undivided default: a tick every cycle.
        @(negedge clk);
        enable = 1'b0;
        raw_divisor = '0;
        clock_and_expect(1'b0, "zero divisor while disabled");
        @(negedge clk);
        enable = 1'b1;
        clock_and_expect(1'b1, "zero divisor cycle 1");
        clock_and_expect(1'b1, "zero divisor cycle 2");

        if (failures != 0) begin
            $fatal(1, "rate_divider failed with %0d error(s)", failures);
        end

        $display("PASS: divided cadence, disable reset, and undivided default");
        $finish;
    end

endmodule
