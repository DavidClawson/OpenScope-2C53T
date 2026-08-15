// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module spi_control_registers #(
    parameter int unsigned COUNT_WIDTH = 16
) (
    input  logic                   reset_n,
    input  logic                   spi_cs_n,

    // The select port is deliberately five bits wide: applying the read
    // path's bench-proven low-five-bit decode to write selects is this
    // block's declared hypothesis.
    input  logic [4:0]             opcode_select,
    input  logic                   opcode_valid,
    input  logic                   known_read_active,
    input  logic [COUNT_WIDTH-1:0] transaction_byte_count,
    input  logic [7:0]             rx_byte,

    output logic [7:0]             raw_reg_01,
    output logic [7:0]             raw_reg_02,
    output logic [7:0]             raw_reg_06,
    output logic [7:0]             raw_reg_07,
    output logic [7:0]             trigger_level,
    output logic [7:0]             raw_rate_divisor
);

    // Reset defaults are the stock arm-sequence values (01 08 / 02 03 / 06 00
    // / 07 00 / 08 AD) so an unwritten reconstruction matches the only fully
    // observed register state.  The true pre-write power-on contents are
    // unknown; register 0x08 is the digital post-ADC trigger level (bench
    // proven 2026-08-14), and register 0x01 now selects the measured rate
    // ladder. Registers 0x01/0x02/0x06/0x07 remain exposed raw.
    localparam logic [7:0] RESET_REG_01        = 8'h08;
    localparam logic [7:0] RESET_REG_02        = 8'h03;
    localparam logic [7:0] RESET_REG_06        = 8'h00;
    localparam logic [7:0] RESET_REG_07        = 8'h00;
    localparam logic [7:0] RESET_TRIGGER_LEVEL = 8'had;

    // Compatibility alias only: PR #24 (2026-08-15) moved the slow-path rate
    // select onto observed register 0x01, so there is no longer a separate
    // placeholder write path.  Keep exposing the old port name so existing
    // top-level/sim wiring still sees the raw byte, but the single source of
    // truth is raw_reg_01.
    assign raw_rate_divisor = raw_reg_01;

    // Every observed stock write is a two-byte CS-low frame: a select byte
    // followed by one value byte.  Commit therefore happens at the observed
    // transaction boundary (CS rising), only for exactly-two-byte frames whose
    // first byte is not a known channel read.  Longer or shorter unknown
    // frames stay raw events upstream.
    always_ff @(posedge spi_cs_n or negedge reset_n) begin
        if (!reset_n) begin
            raw_reg_01       <= RESET_REG_01;
            raw_reg_02       <= RESET_REG_02;
            raw_reg_06       <= RESET_REG_06;
            raw_reg_07       <= RESET_REG_07;
            trigger_level    <= RESET_TRIGGER_LEVEL;
        end else if (opcode_valid && !known_read_active &&
                     (transaction_byte_count == COUNT_WIDTH'(2))) begin
            case (opcode_select)
                5'h01:   raw_reg_01    <= rx_byte;
                5'h02:   raw_reg_02    <= rx_byte;
                5'h06:   raw_reg_06    <= rx_byte;
                5'h07:   raw_reg_07    <= rx_byte;
                5'h08:   trigger_level <= rx_byte;
                default: ;
            endcase
        end
    end

endmodule
