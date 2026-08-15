// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module tb_spi_runtime_interface;

    localparam int unsigned FRAME_BYTES = 1026;
    localparam int unsigned COUNT_WIDTH = 16;

    logic                   reset_n;
    logic                   spi_cs_n;
    logic                   spi_sclk;
    logic                   spi_mosi;
    logic                   spi_miso;
    logic                   spi_miso_oe;
    logic [7:0]             first_response_byte;
    logic [7:0]             response_data;
    logic [COUNT_WIDTH-1:0] response_byte_index;
    logic [1:0]             response_channel;
    logic                   rx_byte_valid;
    logic [7:0]             rx_byte;
    logic [COUNT_WIDTH-1:0] rx_byte_index;
    logic                   unknown_byte_valid;
    logic [7:0]             opcode;
    logic                   opcode_valid;
    logic                   known_read_active;
    logic [COUNT_WIDTH-1:0] transaction_byte_count;
    logic [7:0]             last_opcode;
    logic                   last_opcode_known;
    logic [COUNT_WIDTH-1:0] last_byte_count;
    logic                   last_known_frame_valid;
    logic                   last_known_frame_error;

    logic [7:0] received_miso;
    logic [7:0] raw_bytes [0:FRAME_BYTES+3];
    logic       raw_unknown [0:FRAME_BYTES+3];
    int unsigned raw_count;
    int unsigned failures;

    spi_runtime_interface #(
        .FRAME_BYTES(FRAME_BYTES),
        .COUNT_WIDTH(COUNT_WIDTH)
    ) dut (
        .reset_n(reset_n),
        .spi_cs_n(spi_cs_n),
        .spi_sclk(spi_sclk),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso),
        .spi_miso_oe(spi_miso_oe),
        .first_response_byte(first_response_byte),
        .response_data(response_data),
        .response_byte_index(response_byte_index),
        .response_channel(response_channel),
        .rx_byte_valid(rx_byte_valid),
        .rx_byte(rx_byte),
        .rx_byte_index(rx_byte_index),
        .unknown_byte_valid(unknown_byte_valid),
        .opcode(opcode),
        .opcode_valid(opcode_valid),
        .known_read_active(known_read_active),
        .transaction_byte_count(transaction_byte_count),
        .last_opcode(last_opcode),
        .last_opcode_known(last_opcode_known),
        .last_byte_count(last_byte_count),
        .last_known_frame_valid(last_known_frame_valid),
        .last_known_frame_error(last_known_frame_error)
    );

    // Status semantics are deliberately opaque.  The distinct patterns make
    // byte positions observable without assigning meaning to their contents.
    always_comb begin
        case (response_channel)
            2'd0: response_data = 8'he0 ^ 8'(response_byte_index);
            default: begin
                case (response_byte_index)
                    16'd1: response_data = 8'h61;
                    16'd2: response_data = 8'hb2;
                    default: begin
                        case (response_channel)
                    2'd1: response_data = 8'h40 ^ 8'(response_byte_index - 3);
                    2'd2: response_data = 8'h80 ^ 8'(response_byte_index - 3);
                            default: response_data = 'x;
                        endcase
                    end
                endcase
            end
        endcase
    end

    task automatic begin_transaction;
        begin
            raw_count = 0;
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

            if (!rx_byte_valid || rx_byte !== mosi_value) begin
                $error("raw receive got valid=%0b byte=0x%02x, expected 0x%02x",
                       rx_byte_valid, rx_byte, mosi_value);
                failures++;
            end
            raw_bytes[raw_count] = rx_byte;
            raw_unknown[raw_count] = unknown_byte_valid;
            if (rx_byte_index !== COUNT_WIDTH'(raw_count)) begin
                $error("raw index got %0d, expected %0d", rx_byte_index, raw_count);
                failures++;
            end
            raw_count++;
        end
    endtask

    task automatic end_transaction;
        begin
            #2;
            spi_cs_n = 1'b1;
            #2;
            if (spi_miso_oe !== 1'b0) begin
                $error("MISO output-enable remained asserted after CS rose");
                failures++;
            end
        end
    endtask

    task automatic expect_byte(
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

    task automatic read_known_frame(input logic [7:0] requested_opcode);
        int unsigned byte_number;
        logic [7:0] expected_sample;
        begin
            begin_transaction();
            transfer_byte(requested_opcode, received_miso);
            expect_byte(received_miso, first_response_byte, "status byte 0");
            if (!opcode_valid || opcode !== requested_opcode ||
                !known_read_active || transaction_byte_count !== 16'd1) begin
                $error("opcode 0x%02x was not latched after byte zero", requested_opcode);
                failures++;
            end

            transfer_byte(8'hff, received_miso);
            expect_byte(received_miso, 8'h61, "status byte 1");
            transfer_byte(8'hff, received_miso);
            expect_byte(received_miso, 8'hb2, "status byte 2");

            for (byte_number = 3; byte_number < FRAME_BYTES; byte_number++) begin
                transfer_byte(8'hff, received_miso);
                expected_sample = ((requested_opcode == 8'h04) ? 8'h40 : 8'h80) ^
                                  8'(byte_number - 3);
                expect_byte(received_miso, expected_sample, "sample byte");
            end
            end_transaction();

            if (!last_opcode_known || last_opcode !== requested_opcode ||
                last_byte_count !== COUNT_WIDTH'(FRAME_BYTES) ||
                !last_known_frame_valid || last_known_frame_error) begin
                $error("full known frame metadata mismatch for opcode 0x%02x", requested_opcode);
                failures++;
            end
            if (raw_count != FRAME_BYTES || raw_bytes[0] !== requested_opcode ||
                raw_bytes[FRAME_BYTES-1] !== 8'hff) begin
                $error("raw stream mismatch for opcode 0x%02x", requested_opcode);
                failures++;
            end
        end
    endtask

    initial begin
        reset_n = 1'b0;
        spi_cs_n = 1'b1;
        spi_sclk = 1'b1;
        spi_mosi = 1'b0;
        first_response_byte = 8'hd3;
        raw_count = 0;
        failures = 0;

        #4;
        reset_n = 1'b1;
        #4;

        read_known_frame(8'h04);
        read_known_frame(8'h05);

        // Negative control: 0x05 is not accepted as CH2 unless CS stays low
        // for the complete observed 1026-byte runtime window.
        begin_transaction();
        transfer_byte(8'h05, received_miso);
        transfer_byte(8'hff, received_miso);
        end_transaction();
        if (!last_opcode_known || last_byte_count !== 16'd2 ||
            last_known_frame_valid || !last_known_frame_error) begin
            $error("short 0x05 transaction was not rejected");
            failures++;
        end

        // Unknown opcodes and payload bytes remain visible byte-for-byte.  They
        // are not misclassified as malformed known channel reads.
        begin_transaction();
        transfer_byte(8'h99, received_miso);
        expect_byte(received_miso, first_response_byte, "unknown response byte 0");
        transfer_byte(8'ha5, received_miso);
        expect_byte(received_miso, 8'he1, "unknown response byte 1 passthrough");
        transfer_byte(8'h5a, received_miso);
        expect_byte(received_miso, 8'he2, "unknown response byte 2 passthrough");
        end_transaction();
        if (last_opcode_known || last_known_frame_valid || last_known_frame_error ||
            last_byte_count !== 16'd3 || raw_count != 3 ||
            raw_bytes[0] !== 8'h99 || raw_bytes[1] !== 8'ha5 || raw_bytes[2] !== 8'h5a ||
            !raw_unknown[0] || !raw_unknown[1] || !raw_unknown[2]) begin
            $error("unknown command was decoded or its raw payload was lost");
            failures++;
        end

        // CS is the frame boundary: two single-byte transactions must never be
        // merged into one apparent two-byte command.
        begin_transaction();
        transfer_byte(8'h04, received_miso);
        end_transaction();
        if (last_byte_count !== 16'd1 || !last_known_frame_error) begin
            $error("first CS-delimited byte was not its own short frame");
            failures++;
        end
        begin_transaction();
        transfer_byte(8'hff, received_miso);
        end_transaction();
        if (last_byte_count !== 16'd1 || last_opcode_known ||
            last_known_frame_valid || last_known_frame_error) begin
            $error("second CS-delimited byte inherited the previous opcode");
            failures++;
        end

        if (failures != 0) begin
            $fatal(1, "spi_runtime_interface failed with %0d error(s)", failures);
        end

        $display("PASS: CS framing, opcode latch, 1026-byte counts, MISO sequencing, and unknown raw payloads");
        $finish;
    end

endmodule
