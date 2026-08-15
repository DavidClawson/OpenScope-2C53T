// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module spi_control_registers #(
    parameter int unsigned COUNT_WIDTH = 16,
    // PLACEHOLDER SELECT, NOT EVIDENCE: netlist tracing (gw1n2-apicula M12)
    // shows the gated BSRAM_1/2 write clock behind a counter whose load path
    // reaches the SPI receiver bank, but the divisor register address has not
    // been identified.  The byte-exact stock arm replay (2026-08-14) stayed at
    // the slow default rate, which argues the five observed arm registers are
    // not that divisor, so the placeholder deliberately sits outside them at
    // an unobserved address.  Replace it the moment the bench or netlist pins
    // the real select.
    parameter logic [4:0] RAW_RATE_DIVISOR_SELECT = 5'h0b
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
    // unknown; only register 0x08 has decoded semantics (the digital post-ADC
    // trigger level, bench-proven 2026-08-14).  Registers 0x01/0x02/0x06/0x07
    // are stored and re-exposed raw.
    localparam logic [7:0] RESET_REG_01        = 8'h08;
    localparam logic [7:0] RESET_REG_02        = 8'h03;
    localparam logic [7:0] RESET_REG_06        = 8'h00;
    localparam logic [7:0] RESET_REG_07        = 8'h00;
    localparam logic [7:0] RESET_TRIGGER_LEVEL = 8'had;
    localparam logic [7:0] RESET_RATE_DIVISOR  = 8'h00;

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
            raw_rate_divisor <= RESET_RATE_DIVISOR;
        end else if (opcode_valid && !known_read_active &&
                     (transaction_byte_count == COUNT_WIDTH'(2))) begin
            case (opcode_select)
                5'h01:   raw_reg_01    <= rx_byte;
                5'h02:   raw_reg_02    <= rx_byte;
                5'h06:   raw_reg_06    <= rx_byte;
                5'h07:   raw_reg_07    <= rx_byte;
                5'h08:   trigger_level <= rx_byte;
                default: begin
                    if (opcode_select == RAW_RATE_DIVISOR_SELECT) begin
                        raw_rate_divisor <= rx_byte;
                    end
                end
            endcase
        end
    end

endmodule
