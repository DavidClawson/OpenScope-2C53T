// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

// Hardware validation wrapper: the first buildable image for the real
// GW1N-UV2/QN48 part, using only the six pins whose apicula IOB locations are
// netlist-traced and whose QN48 numbers come from the audited UG171 package
// table (apicula agent/gw1n2-qn48-chipdb-20260815).
//
//   pin 30  IOB7B / GCLKC_4  dbg_clk     MCU-driven debug sample clock (PC6)
//   pin 46  IOR1B            run_enable  master run/arm line (PB11)
//   pin 34  IOB18A           spi_cs_n
//   pin 29  IOB5A            spi_sclk
//   pin 35  IOB18B           spi_mosi
//   pin 28  IOB5B            spi_so      the design's only IOBUF, as in stock
//
// The stock rPLL configuration is still undecoded, so this wrapper substitutes
// an MCU-paced debug clock on the clock-capable pin instead of claiming a PLL.
// The ADC pins are similarly unmapped, so the capture path samples synthetic
// on-chip data: CH1 a free-running ramp, CH2 a walking-one marker -- the R1
// validator pattern, produced through the real capture write path rather than
// memory initialization.  The three status positions return the fixed marker
// bytes "R1V" (0x52 0x31 0x56), deliberately distinguishable from any stock
// status semantics, which remain unknown.
//
// SRAM-only: nothing in this file or its build products authorizes writing the
// FPGA's non-volatile flash.
module debugclk_hw_top (
    input  logic dbg_clk,
    input  logic run_enable,
    input  logic spi_cs_n,
    input  logic spi_sclk,
    input  logic spi_mosi,
    inout  wire  spi_so
);

    localparam logic [7:0] STATUS_MARKER_0 = 8'h52;
    localparam logic [7:0] STATUS_MARKER_1 = 8'h31;
    localparam logic [7:0] STATUS_MARKER_2 = 8'h56;

    // Registers power up to zero after configuration, so this shifter holds
    // the core in reset for the first eight debug-clock edges the MCU sends.
    // The declaration initializer keeps simulation identical to the Gowin
    // power-up state; Yosys carries it as the flip-flop INIT value.  Both
    // waived lints flag exactly what a configuration-time power-on-reset
    // generator is: an init-value flop chain feeding asynchronous resets.
    /* verilator lint_off PROCASSINIT */
    /* verilator lint_off SYNCASYNCNET */
    logic [7:0] por_shift = '0;
    logic       reset_n;

    always_ff @(posedge dbg_clk) begin
        por_shift <= {por_shift[6:0], 1'b1};
    end
    assign reset_n = por_shift[7];
    /* verilator lint_on SYNCASYNCNET */
    /* verilator lint_on PROCASSINIT */

    // Synthetic ADC sources, free-running like a real converter.
    logic [7:0] ramp_counter;

    always_ff @(posedge dbg_clk) begin
        if (!reset_n) begin
            ramp_counter <= '0;
        end else begin
            ramp_counter <= ramp_counter + 1'b1;
        end
    end

    logic       spi_miso;
    logic       spi_miso_oe;

    assign spi_so = spi_miso_oe ? spi_miso : 1'bz;

    // Only the six physical nets exist at this level; the reconstruction
    // top's observability outputs are simulation aids with no evidenced board
    // nets, so they are left deliberately unconnected here.
    /* verilator lint_off PINCONNECTEMPTY */
    fnirsi_2c53t_top core (
        .sample_clk(dbg_clk),
        .reset_n(reset_n),
        .adc_ch1_data(ramp_counter),
        .adc_ch2_data(8'h01 << ramp_counter[2:0]),
        .raw_arm_enable(run_enable),
        .raw_interval_minus_one('0),
        .raw_response_status0(STATUS_MARKER_0),
        .raw_response_status1(STATUS_MARKER_1),
        .raw_response_status2(STATUS_MARKER_2),
        .spi_cs_n(spi_cs_n),
        .spi_sclk(spi_sclk),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso),
        .spi_miso_oe(spi_miso_oe),
        .sample_tick(),
        .trigger_pulse(),
        .trigger_seen(),
        .ch1_at_or_above_level(),
        .reg_raw_01(),
        .reg_raw_02(),
        .reg_raw_06(),
        .reg_raw_07(),
        .reg_trigger_level(),
        .reg_raw_rate_divisor(),
        .slow_path_tick(),
        .ch1_write_ptr(),
        .ch2_write_ptr(),
        .spi_opcode(),
        .spi_opcode_valid(),
        .spi_known_read_active(),
        .spi_last_opcode(),
        .spi_last_opcode_known(),
        .spi_last_byte_count(),
        .spi_last_known_frame_valid(),
        .spi_last_known_frame_error(),
        .spi_unknown_byte_valid(),
        .spi_rx_byte_valid(),
        .spi_rx_byte(),
        .spi_rx_byte_index(),
        .spi_transaction_byte_count()
    );
    /* verilator lint_on PINCONNECTEMPTY */

endmodule
