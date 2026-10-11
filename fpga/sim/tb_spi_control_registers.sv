// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_spi_control_registers;

    localparam int unsigned COUNT_WIDTH = 16;

    logic                   reset_n;
    logic                   spi_cs_n;
    logic [7:0]             opcode;
    logic                   opcode_valid;
    logic                   known_read_active;
    logic [COUNT_WIDTH-1:0] transaction_byte_count;
    logic [7:0]             rx_byte;
    logic [7:0]             raw_reg_01;
    logic [7:0]             raw_reg_02;
    logic [7:0]             raw_reg_06;
    logic [7:0]             raw_reg_07;
    logic [7:0]             trigger_level;
    logic [7:0]             raw_rate_divisor;

    int unsigned failures;

    spi_control_registers #(
        .COUNT_WIDTH(COUNT_WIDTH)
    ) dut (
        .reset_n(reset_n),
        .spi_cs_n(spi_cs_n),
        .opcode_select(opcode[4:0]),
        .opcode_valid(opcode_valid),
        .known_read_active(known_read_active),
        .transaction_byte_count(transaction_byte_count),
        .rx_byte(rx_byte),
        .raw_reg_01(raw_reg_01),
        .raw_reg_02(raw_reg_02),
        .raw_reg_06(raw_reg_06),
        .raw_reg_07(raw_reg_07),
        .trigger_level(trigger_level),
        .raw_rate_divisor(raw_rate_divisor)
    );

    task automatic expect_reg(
        input logic [7:0] actual,
        input logic [7:0] expected,
        input string      label
    );
        begin
            if (actual !== expected) begin
                $error("%s got 0x%02x, expected 0x%02x", label, actual, expected);
                failures++;
            end
        end
    endtask

    // Models the state the SPI frontend presents at a transaction boundary.
    task automatic close_frame(
        input logic [7:0]             frame_opcode,
        input logic                   frame_known_read,
        input logic [COUNT_WIDTH-1:0] frame_byte_count,
        input logic [7:0]             frame_last_byte
    );
        begin
            spi_cs_n = 1'b0;
            #2;
            opcode = frame_opcode;
            opcode_valid = 1'b1;
            known_read_active = frame_known_read;
            transaction_byte_count = frame_byte_count;
            rx_byte = frame_last_byte;
            #2;
            spi_cs_n = 1'b1;
            #2;
        end
    endtask

    initial begin
        reset_n = 1'b0;
        spi_cs_n = 1'b1;
        opcode = '0;
        opcode_valid = 1'b0;
        known_read_active = 1'b0;
        transaction_byte_count = '0;
        rx_byte = '0;
        failures = 0;

        #4;
        reset_n = 1'b1;
        #4;

        // Reset defaults are the stock arm-sequence values.
        expect_reg(raw_reg_01, 8'h08, "reset raw_reg_01");
        expect_reg(raw_reg_02, 8'h03, "reset raw_reg_02");
        expect_reg(raw_reg_06, 8'h00, "reset raw_reg_06");
        expect_reg(raw_reg_07, 8'h00, "reset raw_reg_07");
        expect_reg(trigger_level, 8'had, "reset trigger_level");
        expect_reg(raw_rate_divisor, 8'h08, "raw rate alias follows raw_reg_01");

        // A two-byte unknown frame commits its register.
        close_frame(8'h08, 1'b0, COUNT_WIDTH'(2), 8'h80);
        expect_reg(trigger_level, 8'h80, "trigger level write");

        // The low-five-bit select hypothesis: 0x22 commits register 0x02.
        close_frame(8'h22, 1'b0, COUNT_WIDTH'(2), 8'h55);
        expect_reg(raw_reg_02, 8'h55, "aliased raw_reg_02 write");

        // Negative controls: wrong-length frames and known channel reads must
        // not commit anything.
        close_frame(8'h08, 1'b0, COUNT_WIDTH'(3), 8'hee);
        expect_reg(trigger_level, 8'h80, "three-byte frame ignored");
        close_frame(8'h08, 1'b0, COUNT_WIDTH'(1), 8'hee);
        expect_reg(trigger_level, 8'h80, "one-byte frame ignored");
        close_frame(8'h04, 1'b1, COUNT_WIDTH'(2), 8'hee);
        expect_reg(raw_reg_01, 8'h08, "known read left raw_reg_01");
        expect_reg(trigger_level, 8'h80, "known read left trigger_level");

        // The remaining stock arm registers store raw values.
        close_frame(8'h01, 1'b0, COUNT_WIDTH'(2), 8'ha1);
        close_frame(8'h06, 1'b0, COUNT_WIDTH'(2), 8'ha6);
        close_frame(8'h07, 1'b0, COUNT_WIDTH'(2), 8'ha7);
        expect_reg(raw_reg_01, 8'ha1, "raw_reg_01 write");
        expect_reg(raw_rate_divisor, 8'ha1, "raw rate alias after raw_reg_01 write");
        expect_reg(raw_reg_06, 8'ha6, "raw_reg_06 write");
        expect_reg(raw_reg_07, 8'ha7, "raw_reg_07 write");

        if (failures != 0) begin
            $fatal(1, "spi_control_registers failed with %0d error(s)", failures);
        end

        $display("PASS: reset defaults, two-byte commits, aliased select, and length/known-read negative controls");
        $finish;
    end

endmodule
