// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_capture_channel;

    localparam int unsigned DATA_WIDTH = 8;
    localparam int unsigned DEPTH = 5;
    localparam int unsigned ADDR_WIDTH = $clog2(DEPTH);

    logic                  write_clk;
    logic                  read_clk;
    logic                  reset_n;
    logic                  capture_enable;
    logic                  freeze;
    logic [DATA_WIDTH-1:0] sample_data;
    logic [ADDR_WIDTH-1:0] read_addr;
    logic [DATA_WIDTH-1:0] read_data;
    logic [ADDR_WIDTH-1:0] write_ptr;

    logic [DATA_WIDTH-1:0] expected [0:DEPTH-1];
    int unsigned failures;

    always #5 write_clk = ~write_clk;
    always #7 read_clk = ~read_clk;

    capture_channel #(
        .DATA_WIDTH(DATA_WIDTH),
        .DEPTH(DEPTH),
        .ADDR_WIDTH(ADDR_WIDTH)
    ) dut (
        .write_clk(write_clk),
        .reset_n(reset_n),
        .capture_enable(capture_enable),
        .freeze(freeze),
        .sample_data(sample_data),
        .read_clk(read_clk),
        .read_addr(read_addr),
        .read_data(read_data),
        .write_ptr(write_ptr)
    );

    task automatic capture_sample(input logic [DATA_WIDTH-1:0] value);
        begin
            @(negedge write_clk);
            sample_data = value;
            capture_enable = 1'b1;
            freeze = 1'b0;
            @(posedge write_clk);
            #1;
            capture_enable = 1'b0;
        end
    endtask

    task automatic attempt_write(
        input logic [DATA_WIDTH-1:0] value,
        input logic                  enable,
        input logic                  frozen
    );
        begin
            @(negedge write_clk);
            sample_data = value;
            capture_enable = enable;
            freeze = frozen;
            @(posedge write_clk);
            #1;
            capture_enable = 1'b0;
            freeze = 1'b0;
        end
    endtask

    task automatic expect_memory(
        input logic [ADDR_WIDTH-1:0] address,
        input logic [DATA_WIDTH-1:0] value,
        input string                 label
    );
        begin
            @(negedge read_clk);
            read_addr = address;
            @(posedge read_clk);
            #1;

            if (read_data !== value) begin
                $error("%s: address %0d got 0x%02x, expected 0x%02x",
                       label, address, read_data, value);
                failures++;
            end
        end
    endtask

    task automatic expect_snapshot(input string label);
        int unsigned address;
        begin
            for (address = 0; address < DEPTH; address++) begin
                expect_memory(ADDR_WIDTH'(address), expected[address], label);
            end
        end
    endtask

    initial begin
        write_clk = 1'b0;
        read_clk = 1'b0;
        reset_n = 1'b0;
        capture_enable = 1'b0;
        freeze = 1'b0;
        sample_data = '0;
        read_addr = '0;
        failures = 0;

        repeat (2) @(posedge write_clk);
        reset_n = 1'b1;

        // Seven writes into a five-entry memory prove explicit circular wrap.
        capture_sample(8'h10);
        capture_sample(8'h11);
        capture_sample(8'h12);
        capture_sample(8'h13);
        capture_sample(8'h14);
        capture_sample(8'h15);
        capture_sample(8'h16);

        expected[0] = 8'h15;
        expected[1] = 8'h16;
        expected[2] = 8'h12;
        expected[3] = 8'h13;
        expected[4] = 8'h14;

        if (write_ptr !== ADDR_WIDTH'(2)) begin
            $error("wrap: write_ptr got %0d, expected 2", write_ptr);
            failures++;
        end
        expect_snapshot("wrapped readback");

        // Freeze is a write gate, not a memory clear or read-port gate.
        attempt_write(8'haa, 1'b1, 1'b1);
        if (write_ptr !== ADDR_WIDTH'(2)) begin
            $error("freeze: write_ptr changed to %0d", write_ptr);
            failures++;
        end
        expect_snapshot("frozen readback");

        // Negative control: changing data with capture disabled must not write.
        attempt_write(8'hcc, 1'b0, 1'b0);
        if (write_ptr !== ADDR_WIDTH'(2)) begin
            $error("disabled capture: write_ptr changed to %0d", write_ptr);
            failures++;
        end
        expect_snapshot("disabled capture");

        if (failures != 0) begin
            $fatal(1, "capture_channel failed with %0d error(s)", failures);
        end

        $display("PASS: wrap, freeze, readback, and disabled-capture stability");
        $finish;
    end

endmodule
