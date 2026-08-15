// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module spi_runtime_interface #(
    parameter int unsigned FRAME_BYTES = 1026,
    parameter int unsigned COUNT_WIDTH = 16
) (
    input  logic                   reset_n,
    input  logic                   spi_cs_n,
    input  logic                   spi_sclk,
    input  logic                   spi_mosi,
    output logic                   spi_miso,
    output logic                   spi_miso_oe,

    // Byte zero is already on MISO when the transaction starts.  For every
    // later byte, response_byte_index identifies the byte that the caller
    // must present on response_data before its first bit is shifted out.
    input  logic [7:0]             first_response_byte,
    input  logic [7:0]             response_data,
    output logic [COUNT_WIDTH-1:0] response_byte_index,
    output logic [1:0]             response_channel,

    // Every received byte is exposed.  Unknown commands deliberately use the
    // same raw event path; their payload is not decoded or discarded here.
    output logic                   rx_byte_valid,
    output logic [7:0]             rx_byte,
    output logic [COUNT_WIDTH-1:0] rx_byte_index,
    output logic                   unknown_byte_valid,

    output logic [7:0]             opcode,
    output logic                   opcode_valid,
    output logic                   known_read_active,
    output logic [COUNT_WIDTH-1:0] transaction_byte_count,

    // These values latch when CS rises and remain stable until the next end.
    output logic [7:0]             last_opcode,
    output logic                   last_opcode_known,
    output logic [COUNT_WIDTH-1:0] last_byte_count,
    output logic                   last_known_frame_valid,
    output logic                   last_known_frame_error
);

    localparam logic [7:0] OPCODE_CH1 = 8'h04;
    localparam logic [7:0] OPCODE_CH2 = 8'h05;

    logic [2:0] rx_bit_index;
    logic [6:0] rx_shift;
    logic [2:0] tx_bit_index;
    logic [7:0] tx_shift;
    logic       tx_first_byte;
    logic       tx_first_edge;

    logic [7:0] completed_rx_byte;
    logic       completed_opcode_known;
    logic       spi_inactive;

    assign completed_rx_byte = {rx_shift, spi_mosi};
    assign completed_opcode_known = (completed_rx_byte == OPCODE_CH1) ||
                                    (completed_rx_byte == OPCODE_CH2);
    assign spi_inactive = !reset_n || spi_cs_n;
    assign spi_miso = tx_first_byte ? first_response_byte[7-tx_bit_index] : tx_shift[7];
    assign spi_miso_oe = !spi_cs_n;

    always_comb begin
        if (opcode_valid && (opcode == OPCODE_CH1)) begin
            response_channel = 2'd1;
        end else if (opcode_valid && (opcode == OPCODE_CH2)) begin
            response_channel = 2'd2;
        end else begin
            response_channel = 2'd0;
        end
    end

    // Evidence: the stock MCU uses SPI mode 3, so MOSI is sampled on rising
    // SCLK edges.  The only interpreted runtime opcodes are the observed CH1
    // and CH2 reads.  Their status bytes, sample ordering, and every other
    // command remain outside this block because those semantics are unproven.
    always_ff @(posedge spi_sclk or posedge spi_inactive) begin
        if (spi_inactive) begin
            rx_bit_index             <= '0;
            rx_shift                 <= '0;
            rx_byte_valid            <= 1'b0;
            rx_byte                  <= '0;
            rx_byte_index            <= '0;
            unknown_byte_valid       <= 1'b0;
            opcode                   <= '0;
            opcode_valid             <= 1'b0;
            known_read_active        <= 1'b0;
            transaction_byte_count   <= '0;
        end else begin
            rx_byte_valid        <= 1'b0;
            unknown_byte_valid   <= 1'b0;
            rx_shift             <= completed_rx_byte[6:0];

            if (rx_bit_index == 3'd7) begin
                rx_bit_index           <= '0;
                rx_byte                <= completed_rx_byte;
                rx_byte_index          <= transaction_byte_count;
                rx_byte_valid          <= 1'b1;
                transaction_byte_count <= transaction_byte_count + 1'b1;

                if (!opcode_valid) begin
                    opcode             <= completed_rx_byte;
                    opcode_valid       <= 1'b1;
                    known_read_active  <= completed_opcode_known;
                    unknown_byte_valid <= !completed_opcode_known;
                end else begin
                    unknown_byte_valid <= !known_read_active;
                end
            end else begin
                rx_bit_index <= rx_bit_index + 1'b1;
            end
        end
    end

    // CS rising is the observed transaction boundary.  Keeping completed-frame
    // metadata in this small CS-clocked bank avoids pretending that another,
    // currently unknown system clock owns the SPI protocol state.
    always_ff @(posedge spi_cs_n or negedge reset_n) begin
        if (!reset_n) begin
            last_opcode            <= '0;
            last_opcode_known      <= 1'b0;
            last_byte_count        <= '0;
            last_known_frame_valid <= 1'b0;
            last_known_frame_error <= 1'b0;
        end else begin
            last_opcode            <= opcode;
            last_opcode_known      <= opcode_valid && known_read_active;
            last_byte_count        <= transaction_byte_count;
            last_known_frame_valid <= opcode_valid && known_read_active &&
                                      (transaction_byte_count == COUNT_WIDTH'(FRAME_BYTES));
            last_known_frame_error <= opcode_valid && known_read_active &&
                                      (transaction_byte_count != COUNT_WIDTH'(FRAME_BYTES));
        end
    end

    // MISO changes on falling SCLK edges and is stable at the rising sampling
    // edge.  response_data is intentionally a transparent byte source: for
    // known reads the integration layer may provide status/sample bytes; for
    // unknown commands it may expose any separately recovered raw behavior.
    always_ff @(negedge spi_sclk or posedge spi_inactive) begin
        if (spi_inactive) begin
            tx_bit_index        <= '0;
            tx_shift            <= '0;
            tx_first_byte       <= 1'b1;
            tx_first_edge       <= 1'b1;
            response_byte_index <= COUNT_WIDTH'(1);
        end else if (tx_first_edge) begin
            // Mode 3 begins with a falling edge.  Byte zero comes directly
            // from first_response_byte, so this edge must not advance past its
            // most-significant bit.
            tx_first_edge <= 1'b0;
        end else if (tx_bit_index == 3'd7) begin
            tx_bit_index        <= '0;
            tx_shift            <= response_data;
            tx_first_byte       <= 1'b0;
            response_byte_index <= response_byte_index + 1'b1;
        end else begin
            tx_bit_index <= tx_bit_index + 1'b1;
            tx_shift     <= {tx_shift[6:0], 1'b0};
        end
    end

endmodule
