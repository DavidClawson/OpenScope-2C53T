// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module fnirsi_2c53t_top #(
    parameter int unsigned DATA_WIDTH = 8,
    parameter int unsigned CAPTURE_DEPTH = 1024,
    parameter int unsigned CAPTURE_ADDR_WIDTH = (CAPTURE_DEPTH <= 1) ? 1 : $clog2(CAPTURE_DEPTH),
    parameter int unsigned FRAME_BYTES = 1026,
    parameter int unsigned SPI_COUNT_WIDTH = 16,
    parameter int unsigned TIMEBASE_WIDTH = 16,
    parameter int unsigned SLOW_PATH_DIVISOR_WIDTH = 16,
    parameter int unsigned SLOW_PATH_BASE_DIVISOR = 16'd1999,
    parameter int unsigned SLOW_PATH_LEGACY_DIVISOR = 16'd8
) (
    input  logic                              sample_clk,
    input  logic                              reset_n,

    input  logic [DATA_WIDTH-1:0]             adc_ch1_data,
    input  logic [DATA_WIDTH-1:0]             adc_ch2_data,

    // Raw local controls.  The complete stock register map, arming lifecycle,
    // and timebase encoding are still unknown; the trigger event is no longer
    // an input because the comparator is bench-evidenced as digital and
    // post-ADC, driven by SPI register 0x08 (see trigger_comparator).
    input  logic                              raw_arm_enable,
    input  logic [TIMEBASE_WIDTH-1:0]         raw_interval_minus_one,

    // Raw response bytes for the three non-sample positions used by the
    // observed 1026-byte CS-low runtime reads.  Their meanings are not decoded
    // here; firmware or a later evidenced block owns that interpretation.
    input  logic [7:0]                        raw_response_status0,
    input  logic [7:0]                        raw_response_status1,
    input  logic [7:0]                        raw_response_status2,

    input  logic                              spi_cs_n,
    input  logic                              spi_sclk,
    input  logic                              spi_mosi,
    output logic                              spi_miso,
    output logic                              spi_miso_oe,

    output logic                              sample_tick,
    output logic                              trigger_pulse,
    output logic                              trigger_seen,
    output logic                              ch1_at_or_above_level,

    // Committed SPI control registers.  Register 0x08 is the bench-proven
    // trigger level; register 0x01 is now also consumed locally as the raw
    // slow-path rate select byte per the PR #24 / 2026-08-15 reconstruction.
    // The other stock-written registers remain raw exports for a later
    // evidenced owner.
    output logic [7:0]                        reg_raw_01,
    output logic [7:0]                        reg_raw_02,
    output logic [7:0]                        reg_raw_06,
    output logic [7:0]                        reg_raw_07,
    output logic [7:0]                        reg_trigger_level,
    output logic [7:0]                        reg_raw_rate_divisor,

    // Reconstruction slot for the gated BSRAM_1/2 write cadence: the divided
    // tick is exported instead of instantiating the slow store pair because
    // that record's function (decimation, min/max, roll) is still a
    // hypothesis.  The port name keeps historical compatibility even though
    // the source byte is now raw register 0x01, not a separate placeholder
    // divisor register.
    output logic                              slow_path_tick,

    output logic [CAPTURE_ADDR_WIDTH-1:0]     ch1_write_ptr,
    output logic [CAPTURE_ADDR_WIDTH-1:0]     ch2_write_ptr,

    output logic [7:0]                        spi_opcode,
    output logic                              spi_opcode_valid,
    output logic                              spi_known_read_active,
    output logic [7:0]                        spi_last_opcode,
    output logic                              spi_last_opcode_known,
    output logic [SPI_COUNT_WIDTH-1:0]        spi_last_byte_count,
    output logic                              spi_last_known_frame_valid,
    output logic                              spi_last_known_frame_error,
    output logic                              spi_unknown_byte_valid,
    output logic                              spi_rx_byte_valid,
    output logic [7:0]                        spi_rx_byte,
    output logic [SPI_COUNT_WIDTH-1:0]        spi_rx_byte_index,
    output logic [SPI_COUNT_WIDTH-1:0]        spi_transaction_byte_count
);

    logic [SPI_COUNT_WIDTH-1:0]    response_byte_index;
    logic [1:0]                    response_channel;
    logic [7:0]                    response_data;

    logic [CAPTURE_ADDR_WIDTH-1:0] read_addr;
    logic [DATA_WIDTH-1:0]         ch1_read_data;
    logic [DATA_WIDTH-1:0]         ch2_read_data;
    logic                          capture_write_enable;
    logic                          capture_freeze;
    logic                          sample_response_byte;
    logic                          ch1_crossing_event;

    assign capture_write_enable = raw_arm_enable && sample_tick;
    assign capture_freeze = trigger_seen;
    assign sample_response_byte = (response_byte_index >= SPI_COUNT_WIDTH'(3)) &&
                                  (response_byte_index < SPI_COUNT_WIDTH'(FRAME_BYTES));

    // Assumption boundary: this address feeds the RAM read port clocked by
    // spi_sclk while the writer is clocked by sample_clk.  That isolates the
    // current editable model to a dual-port-memory crossing; it is not a claim
    // that the stock bitstream used this CDC shape or that timing is closed.
    always_comb begin
        if (sample_response_byte) begin
            read_addr = CAPTURE_ADDR_WIDTH'(response_byte_index - SPI_COUNT_WIDTH'(3));
        end else begin
            read_addr = '0;
        end
    end

    always_comb begin
        case (response_byte_index)
            SPI_COUNT_WIDTH'(1): begin
                response_data = (response_channel != 2'd0) ? raw_response_status1 : 8'h00;
            end
            SPI_COUNT_WIDTH'(2): begin
                response_data = (response_channel != 2'd0) ? raw_response_status2 : 8'h00;
            end
            default: begin
                if (sample_response_byte && response_channel == 2'd1) begin
                    response_data = ch1_read_data;
                end else if (sample_response_byte && response_channel == 2'd2) begin
                    response_data = ch2_read_data;
                end else begin
                    response_data = 8'h00;
                end
            end
        endcase
    end

    // Commit boundary is CS rising, matching the observed stock write frames.
    // The committed values cross into the sample_clk domain without a
    // recovered CDC protocol, the same declared limitation as the capture
    // memories' dual-clock read path.
    spi_control_registers #(
        .COUNT_WIDTH(SPI_COUNT_WIDTH)
    ) control_registers (
        .reset_n(reset_n),
        .spi_cs_n(spi_cs_n),
        .opcode_select(spi_opcode[4:0]),
        .opcode_valid(spi_opcode_valid),
        .known_read_active(spi_known_read_active),
        .transaction_byte_count(spi_transaction_byte_count),
        .rx_byte(spi_rx_byte),
        .raw_reg_01(reg_raw_01),
        .raw_reg_02(reg_raw_02),
        .raw_reg_06(reg_raw_06),
        .raw_reg_07(reg_raw_07),
        .trigger_level(reg_trigger_level),
        .raw_rate_divisor(reg_raw_rate_divisor)
    );

    // CH1 is the trigger source here because every bench trigger observation
    // is CH1-side and the b2 freshness flag appears on CH1 reads only; stock
    // source and edge selection remain unknown.
    trigger_comparator #(
        .DATA_WIDTH(DATA_WIDTH)
    ) ch1_trigger (
        .clk(sample_clk),
        .reset_n(reset_n),
        .arm_enable(raw_arm_enable),
        .sample_valid(sample_tick),
        .sample_data(adc_ch1_data),
        .trigger_level(reg_trigger_level),
        .at_or_above_level(ch1_at_or_above_level),
        .crossing_event(ch1_crossing_event)
    );

    rate_divider #(
        .SELECT_WIDTH(8),
        .DIVISOR_WIDTH(SLOW_PATH_DIVISOR_WIDTH),
        .BASE_DIVISOR(SLOW_PATH_BASE_DIVISOR),
        .LEGACY_SLOW_DEFAULT_DIVISOR(SLOW_PATH_LEGACY_DIVISOR)
    ) slow_path_rate (
        .clk(sample_clk),
        .reset_n(reset_n),
        .enable(raw_arm_enable),
        .raw_divisor(reg_raw_01),
        .divided_tick(slow_path_tick)
    );

    trigger_timebase #(
        .COUNTER_WIDTH(TIMEBASE_WIDTH)
    ) timebase (
        .clk(sample_clk),
        .reset_n(reset_n),
        .raw_arm_enable(raw_arm_enable),
        .raw_interval_minus_one(raw_interval_minus_one),
        .raw_trigger_event(ch1_crossing_event),
        .sample_tick(sample_tick),
        .trigger_pulse(trigger_pulse),
        .trigger_seen(trigger_seen)
    );

    capture_channel #(
        .DATA_WIDTH(DATA_WIDTH),
        .DEPTH(CAPTURE_DEPTH),
        .ADDR_WIDTH(CAPTURE_ADDR_WIDTH)
    ) ch1_capture (
        .write_clk(sample_clk),
        .reset_n(reset_n),
        .capture_enable(capture_write_enable),
        .freeze(capture_freeze),
        .sample_data(adc_ch1_data),
        .read_clk(spi_sclk),
        .read_addr(read_addr),
        .read_data(ch1_read_data),
        .write_ptr(ch1_write_ptr)
    );

    capture_channel #(
        .DATA_WIDTH(DATA_WIDTH),
        .DEPTH(CAPTURE_DEPTH),
        .ADDR_WIDTH(CAPTURE_ADDR_WIDTH)
    ) ch2_capture (
        .write_clk(sample_clk),
        .reset_n(reset_n),
        .capture_enable(capture_write_enable),
        .freeze(capture_freeze),
        .sample_data(adc_ch2_data),
        .read_clk(spi_sclk),
        .read_addr(read_addr),
        .read_data(ch2_read_data),
        .write_ptr(ch2_write_ptr)
    );

    spi_runtime_interface #(
        .FRAME_BYTES(FRAME_BYTES),
        .COUNT_WIDTH(SPI_COUNT_WIDTH)
    ) spi_runtime (
        .reset_n(reset_n),
        .spi_cs_n(spi_cs_n),
        .spi_sclk(spi_sclk),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso),
        .spi_miso_oe(spi_miso_oe),
        .first_response_byte(raw_response_status0),
        .response_data(response_data),
        .response_byte_index(response_byte_index),
        .response_channel(response_channel),
        .rx_byte_valid(spi_rx_byte_valid),
        .rx_byte(spi_rx_byte),
        .rx_byte_index(spi_rx_byte_index),
        .unknown_byte_valid(spi_unknown_byte_valid),
        .opcode(spi_opcode),
        .opcode_valid(spi_opcode_valid),
        .known_read_active(spi_known_read_active),
        .transaction_byte_count(spi_transaction_byte_count),
        .last_opcode(spi_last_opcode),
        .last_opcode_known(spi_last_opcode_known),
        .last_byte_count(spi_last_byte_count),
        .last_known_frame_valid(spi_last_known_frame_valid),
        .last_known_frame_error(spi_last_known_frame_error)
    );

endmodule
