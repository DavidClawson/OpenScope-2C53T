// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

// Validates the hardware wrapper exclusively through its six physical nets,
// the way the real bench will: no internal observability is assumed.
module tb_debugclk_hw_top;

    localparam int unsigned FRAME_BYTES = 1026;
    localparam int unsigned SAMPLE_BYTES = FRAME_BYTES - 3;

    logic dbg_clk;
    logic run_enable;
    logic spi_cs_n;
    logic spi_sclk;
    logic spi_mosi;
    wire  spi_so;

    logic [7:0] captured_frame [0:FRAME_BYTES-1];
    logic [7:0] frame_a [0:FRAME_BYTES-1];
    logic [7:0] frame_b [0:FRAME_BYTES-1];
    int unsigned failures;

    always #5 dbg_clk = ~dbg_clk;

    debugclk_hw_top dut (
        .dbg_clk(dbg_clk),
        .run_enable(run_enable),
        .spi_cs_n(spi_cs_n),
        .spi_sclk(spi_sclk),
        .spi_mosi(spi_mosi),
        .spi_so(spi_so)
    );

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
                miso_value[bit_number] = spi_so;
                #1;
            end
        end
    endtask

    // Icarus does not support unpacked-array task ports and silently drops
    // copy-out to variable-indexed array-element actuals, so bytes land in a
    // scalar first and are then stored into the shared captured_frame buffer.
    task automatic read_frame(input logic [7:0] opcode);
        int unsigned byte_number;
        logic [7:0] byte_value;
        begin
            spi_sclk = 1'b1;
            spi_cs_n = 1'b0;
            #2;
            transfer_byte(opcode, byte_value);
            captured_frame[0] = byte_value;
            for (byte_number = 1; byte_number < FRAME_BYTES; byte_number++) begin
                transfer_byte(8'hff, byte_value);
                captured_frame[byte_number] = byte_value;
            end
            #2;
            spi_cs_n = 1'b1;
            #2;
            if (spi_so !== 1'bz) begin
                $error("SO kept driving after CS rose");
                failures++;
            end
        end
    endtask

    function automatic bit is_onehot_or_zero(input logic [7:0] value);
        return (value == 8'h00) || ((value & (value - 8'h01)) == 8'h00);
    endfunction

    initial begin
        dbg_clk = 1'b0;
        run_enable = 1'b0;
        spi_cs_n = 1'b1;
        spi_sclk = 1'b1;
        spi_mosi = 1'b0;
        failures = 0;

        // Power-on-reset shifter needs eight debug clocks; give it margin.
        repeat (16) @(posedge dbg_clk);

        // Stock's observed arm sequence opens with an empty CS pulse; issue
        // the same pulse here.  On hardware every flop is defined after
        // configuration; in simulation this pulse also clears the X-state
        // pessimism in the SPI shifters before the first real frame.
        spi_cs_n = 1'b0;
        #4;
        spi_cs_n = 1'b1;
        #4;

        // Run the engine long enough for the CH1 ramp to cross the default
        // 0xAD trigger level and freeze the buffers with deterministic
        // content: an incrementing CH1 prefix, a walking-one CH2 prefix, and
        // untouched zeroed BSRAM words past the freeze point.
        @(negedge dbg_clk);
        run_enable = 1'b1;
        repeat (2000) @(posedge dbg_clk);

        read_frame(8'h04);
        for (int unsigned index = 0; index < FRAME_BYTES; index++) begin
            frame_a[index] = captured_frame[index];
        end

        if (frame_a[0] !== 8'h52 || frame_a[1] !== 8'h31 || frame_a[2] !== 8'h56) begin
            $error("CH1 status markers got %02x %02x %02x, expected 52 31 56",
                   frame_a[0], frame_a[1], frame_a[2]);
            failures++;
        end

        // Structural checks avoid phase math: the sample region must contain a
        // long strictly-incrementing run and must not be constant.  The
        // byte-by-byte temporaries also dodge an Icarus miscompile of
        // $isunknown over loop-indexed array selects inside a conjunction.
        begin
            int unsigned longest_run;
            int unsigned current_run;
            logic [7:0] this_byte;
            logic [7:0] next_byte;
            longest_run = 0;
            current_run = 0;
            for (int unsigned index = 3; index < FRAME_BYTES - 1; index++) begin
                this_byte = frame_a[index];
                next_byte = frame_a[index + 1];
                if (!$isunknown(next_byte) && next_byte === (this_byte + 8'h01)) begin
                    current_run++;
                    if (current_run > longest_run) begin
                        longest_run = current_run;
                    end
                end else begin
                    current_run = 0;
                end
            end
            if (longest_run < 64) begin
                $error("CH1 ramp run too short: %0d consecutive increments", longest_run);
                failures++;
            end
        end

        // Frozen buffer: a second read must be byte-identical while the debug
        // clock keeps running.
        repeat (50) @(posedge dbg_clk);
        read_frame(8'h04);
        for (int unsigned index = 0; index < FRAME_BYTES; index++) begin
            frame_b[index] = captured_frame[index];
        end
        for (int unsigned index = 0; index < FRAME_BYTES; index++) begin
            if (frame_a[index] !== frame_b[index]) begin
                $error("frozen CH1 frame changed at byte %0d: %02x -> %02x",
                       index, frame_a[index], frame_b[index]);
                failures++;
            end
        end

        // CH2 carries the walking-one marker; every sample byte is one-hot or
        // an untouched zero word.
        read_frame(8'h05);
        for (int unsigned index = 0; index < FRAME_BYTES; index++) begin
            frame_b[index] = captured_frame[index];
        end
        if (frame_b[0] !== 8'h52 || frame_b[1] !== 8'h31 || frame_b[2] !== 8'h56) begin
            $error("CH2 status markers got %02x %02x %02x, expected 52 31 56",
                   frame_b[0], frame_b[1], frame_b[2]);
            failures++;
        end
        // Words past the freeze point were never written: configured hardware
        // reads them as zeroed BSRAM, while simulation reads X because the
        // reconstruction deliberately models no memory initialization.  Both
        // count as untouched here.
        begin
            int unsigned onehot_count;
            logic [7:0] sample_byte;
            onehot_count = 0;
            for (int unsigned index = 3; index < FRAME_BYTES; index++) begin
                sample_byte = frame_b[index];
                if (!$isunknown(sample_byte) && !is_onehot_or_zero(sample_byte)) begin
                    $error("CH2 byte %0d is not one-hot or zero: %02x",
                           index, sample_byte);
                    failures++;
                end
                if (!$isunknown(sample_byte) && sample_byte != 8'h00) begin
                    onehot_count++;
                end
            end
            if (onehot_count < 64) begin
                $error("CH2 marker prefix too short: %0d non-zero bytes", onehot_count);
                failures++;
            end
        end

        // Re-arm through the run line: dropping and raising run_enable clears
        // the trigger latch, and a later read shows refreshed content.
        @(negedge dbg_clk);
        run_enable = 1'b0;
        repeat (4) @(posedge dbg_clk);
        @(negedge dbg_clk);
        run_enable = 1'b1;
        repeat (300) @(posedge dbg_clk);
        read_frame(8'h04);
        for (int unsigned index = 0; index < FRAME_BYTES; index++) begin
            frame_b[index] = captured_frame[index];
        end
        begin
            int unsigned changed_bytes;
            changed_bytes = 0;
            for (int unsigned index = 3; index < FRAME_BYTES; index++) begin
                if (frame_a[index] !== frame_b[index]) begin
                    changed_bytes++;
                end
            end
            if (changed_bytes == 0) begin
                $error("re-armed capture never refreshed the buffer");
                failures++;
            end
        end

        if (failures != 0) begin
            $fatal(1, "debugclk_hw_top failed with %0d error(s)", failures);
        end

        $display("PASS: POR, status markers, ramp capture, trigger freeze stability, walking marker, and run-line re-arm");
        $finish;
    end

endmodule
