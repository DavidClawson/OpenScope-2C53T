// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_fnirsi_2c53t_top;

    localparam int unsigned DATA_WIDTH = 8;
    localparam int unsigned CAPTURE_DEPTH = 16;
    localparam int unsigned CAPTURE_ADDR_WIDTH = $clog2(CAPTURE_DEPTH);
    localparam int unsigned FRAME_BYTES = 1026;
    localparam int unsigned SPI_COUNT_WIDTH = 16;
    localparam int unsigned TIMEBASE_WIDTH = 4;

    logic                              sample_clk;
    logic                              reset_n;
    logic [DATA_WIDTH-1:0]             adc_ch1_data;
    logic [DATA_WIDTH-1:0]             adc_ch2_data;
    logic                              raw_arm_enable;
    logic [TIMEBASE_WIDTH-1:0]         raw_interval_minus_one;
    logic                              raw_trigger_event;
    logic [7:0]                        raw_response_status0;
    logic [7:0]                        raw_response_status1;
    logic [7:0]                        raw_response_status2;
    logic                              spi_cs_n;
    logic                              spi_sclk;
    logic                              spi_mosi;
    logic                              spi_miso;
    logic                              spi_miso_oe;
    logic                              sample_tick;
    logic                              trigger_pulse;
    logic                              trigger_seen;
    logic [CAPTURE_ADDR_WIDTH-1:0]     ch1_write_ptr;
    logic [CAPTURE_ADDR_WIDTH-1:0]     ch2_write_ptr;
    logic [7:0]                        spi_opcode;
    logic                              spi_opcode_valid;
    logic                              spi_known_read_active;
    logic [7:0]                        spi_last_opcode;
    logic                              spi_last_opcode_known;
    logic [SPI_COUNT_WIDTH-1:0]        spi_last_byte_count;
    logic                              spi_last_known_frame_valid;
    logic                              spi_last_known_frame_error;
    logic                              spi_unknown_byte_valid;
    logic                              spi_rx_byte_valid;
    logic [7:0]                        spi_rx_byte;
    logic [SPI_COUNT_WIDTH-1:0]        spi_rx_byte_index;
    logic [SPI_COUNT_WIDTH-1:0]        spi_transaction_byte_count;

    logic [7:0]                        received_miso;
    int unsigned                       failures;

    always #5 sample_clk = ~sample_clk;

    fnirsi_2c53t_top #(
        .DATA_WIDTH(DATA_WIDTH),
        .CAPTURE_DEPTH(CAPTURE_DEPTH),
        .CAPTURE_ADDR_WIDTH(CAPTURE_ADDR_WIDTH),
        .FRAME_BYTES(FRAME_BYTES),
        .SPI_COUNT_WIDTH(SPI_COUNT_WIDTH),
        .TIMEBASE_WIDTH(TIMEBASE_WIDTH)
    ) dut (
        .sample_clk(sample_clk),
        .reset_n(reset_n),
        .adc_ch1_data(adc_ch1_data),
        .adc_ch2_data(adc_ch2_data),
        .raw_arm_enable(raw_arm_enable),
        .raw_interval_minus_one(raw_interval_minus_one),
        .raw_trigger_event(raw_trigger_event),
        .raw_response_status0(raw_response_status0),
        .raw_response_status1(raw_response_status1),
        .raw_response_status2(raw_response_status2),
        .spi_cs_n(spi_cs_n),
        .spi_sclk(spi_sclk),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso),
        .spi_miso_oe(spi_miso_oe),
        .sample_tick(sample_tick),
        .trigger_pulse(trigger_pulse),
        .trigger_seen(trigger_seen),
        .ch1_write_ptr(ch1_write_ptr),
        .ch2_write_ptr(ch2_write_ptr),
        .spi_opcode(spi_opcode),
        .spi_opcode_valid(spi_opcode_valid),
        .spi_known_read_active(spi_known_read_active),
        .spi_last_opcode(spi_last_opcode),
        .spi_last_opcode_known(spi_last_opcode_known),
        .spi_last_byte_count(spi_last_byte_count),
        .spi_last_known_frame_valid(spi_last_known_frame_valid),
        .spi_last_known_frame_error(spi_last_known_frame_error),
        .spi_unknown_byte_valid(spi_unknown_byte_valid),
        .spi_rx_byte_valid(spi_rx_byte_valid),
        .spi_rx_byte(spi_rx_byte),
        .spi_rx_byte_index(spi_rx_byte_index),
        .spi_transaction_byte_count(spi_transaction_byte_count)
    );

    task automatic expect_equal8(
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

    task automatic expect_bit(
        input logic  actual,
        input logic  expected,
        input string label
    );
        begin
            if (actual !== expected) begin
                $error("%s got %0b, expected %0b", label, actual, expected);
                failures++;
            end
        end
    endtask

    task automatic capture_clock(
        input logic [7:0] ch1_value,
        input logic [7:0] ch2_value,
        input logic       trigger_event
    );
        begin
            @(negedge sample_clk);
            adc_ch1_data = ch1_value;
            adc_ch2_data = ch2_value;
            raw_trigger_event = trigger_event;
            @(posedge sample_clk);
            #1;
        end
    endtask

    task automatic begin_transaction;
        begin
            spi_sclk = 1'b1;
            spi_cs_n = 1'b0;
            #2;
        end
    endtask

    task automatic transfer_byte(
        input  logic [7:0] mosi_value,
        output logic [7:0] miso_value
    );
        int bit_number;
        begin
            miso_value = '0;
            for (bit_number = 7; bit_number >= 0; bit_number--) begin
                spi_mosi = mosi_value[bit_number];
                spi_sclk = 1'b0;
                #2;
                spi_sclk = 1'b1;
                #1;
                miso_value[bit_number] = spi_miso;
                #1;
            end
        end
    endtask

    task automatic end_transaction;
        begin
            #2;
            spi_cs_n = 1'b1;
            #2;
            expect_bit(spi_miso_oe, 1'b0, "MISO output-enable after CS");
        end
    endtask

    task automatic read_channel(
        input logic [7:0] opcode,
        input logic [7:0] base_sample,
        input string      label
    );
        int unsigned byte_number;
        logic [7:0] expected_sample;
        begin
            begin_transaction();
            transfer_byte(opcode, received_miso);
            expect_equal8(received_miso, raw_response_status0, {label, " status0"});
            expect_equal8(spi_opcode, opcode, {label, " opcode"});
            expect_bit(spi_opcode_valid, 1'b1, {label, " opcode_valid"});
            expect_bit(spi_known_read_active, 1'b1, {label, " known_read_active"});

            transfer_byte(8'hff, received_miso);
            expect_equal8(received_miso, raw_response_status1, {label, " status1"});

            transfer_byte(8'hff, received_miso);
            expect_equal8(received_miso, raw_response_status2, {label, " status2"});

            for (byte_number = 0; byte_number < CAPTURE_DEPTH; byte_number++) begin
                transfer_byte(8'hff, received_miso);
                expected_sample = base_sample + 8'(byte_number);
                expect_equal8(received_miso, expected_sample, {label, " sample"});
            end

            for (byte_number = 3 + CAPTURE_DEPTH; byte_number < FRAME_BYTES; byte_number++) begin
                transfer_byte(8'hff, received_miso);
            end

            end_transaction();
            expect_bit(spi_last_opcode_known, 1'b1, {label, " last opcode known"});
            expect_equal8(spi_last_opcode, opcode, {label, " last opcode"});
            if (spi_last_byte_count !== SPI_COUNT_WIDTH'(FRAME_BYTES)) begin
                $error("%s last byte count got %0d, expected %0d",
                       label, spi_last_byte_count, FRAME_BYTES);
                failures++;
            end
            expect_bit(spi_last_known_frame_valid, 1'b1, {label, " full frame valid"});
            expect_bit(spi_last_known_frame_error, 1'b0, {label, " full frame error"});
        end
    endtask

    initial begin
        sample_clk = 1'b0;
        reset_n = 1'b0;
        adc_ch1_data = '0;
        adc_ch2_data = '0;
        raw_arm_enable = 1'b0;
        raw_interval_minus_one = '0;
        raw_trigger_event = 1'b0;
        raw_response_status0 = 8'hd3;
        raw_response_status1 = 8'h61;
        raw_response_status2 = 8'hb2;
        spi_cs_n = 1'b1;
        spi_sclk = 1'b1;
        spi_mosi = 1'b0;
        failures = 0;

        repeat (2) @(posedge sample_clk);
        reset_n = 1'b1;
        repeat (2) @(posedge sample_clk);

        // Negative control: disarmed ADC changes do not advance either buffer.
        capture_clock(8'hee, 8'hdd, 1'b0);
        if (ch1_write_ptr !== '0 || ch2_write_ptr !== '0) begin
            $error("disarmed capture advanced write pointers");
            failures++;
        end

        // One sample per armed clock.  The trigger event latches and then
        // freezes later writes; no stock post-trigger policy is invented here.
        @(negedge sample_clk);
        raw_arm_enable = 1'b1;
        raw_interval_minus_one = '0;
        for (int unsigned sample_number = 0; sample_number < CAPTURE_DEPTH; sample_number++) begin
            capture_clock(8'h10 + 8'(sample_number),
                          8'h80 + 8'(sample_number),
                          sample_number == CAPTURE_DEPTH - 1);
            if (sample_number == 0) begin
                expect_bit(sample_tick, 1'b1, "first armed sample tick");
            end
        end
        expect_bit(trigger_seen, 1'b1, "trigger latch");
        expect_bit(trigger_pulse, 1'b1, "trigger pulse");

        capture_clock(8'h99, 8'ha9, 1'b0);
        if (ch1_write_ptr !== CAPTURE_ADDR_WIDTH'(0) ||
            ch2_write_ptr !== CAPTURE_ADDR_WIDTH'(0)) begin
            $error("trigger freeze did not hold write pointers after a full buffer");
            failures++;
        end

        read_channel(8'h04, 8'h10, "CH1 read");
        read_channel(8'h05, 8'h80, "CH2 read");

        // Unknown opcodes remain raw SPI events and are not upgraded into a
        // known read response or a frame-length error.
        begin_transaction();
        transfer_byte(8'h99, received_miso);
        expect_equal8(received_miso, raw_response_status0, "unknown status0");
        expect_equal8(spi_rx_byte, 8'h99, "unknown raw byte");
        expect_bit(spi_rx_byte_valid, 1'b1, "unknown raw valid");
        if (spi_rx_byte_index !== SPI_COUNT_WIDTH'(0) ||
            spi_transaction_byte_count !== SPI_COUNT_WIDTH'(1)) begin
            $error("unknown raw index/count mismatch");
            failures++;
        end
        expect_bit(spi_unknown_byte_valid, 1'b1, "unknown opcode raw event");
        transfer_byte(8'ha5, received_miso);
        expect_equal8(received_miso, 8'h00, "unknown payload response");
        end_transaction();

        expect_bit(spi_last_opcode_known, 1'b0, "unknown last opcode known");
        expect_bit(spi_last_known_frame_valid, 1'b0, "unknown frame valid");
        expect_bit(spi_last_known_frame_error, 1'b0, "unknown frame error");
        expect_equal8(spi_last_opcode, 8'h99, "unknown last opcode");

        if (failures != 0) begin
            $fatal(1, "fnirsi_2c53t_top failed with %0d error(s)", failures);
        end

        $display("PASS: top captures both channels, latches trigger, serves 0x04/0x05 reads, and preserves unknown opcodes");
        $finish;
    end

endmodule
