// SPDX-License-Identifier: GPL-3.0-or-later

`timescale 1ns/1ps

module rate_divider #(
    parameter int unsigned SELECT_WIDTH = 8,
    parameter int unsigned DIVISOR_WIDTH = 16,
    parameter int unsigned BASE_DIVISOR = 16'd1999,
    parameter int unsigned LEGACY_SLOW_DEFAULT_DIVISOR = 16'd8
) (
    input  logic                     clk,
    input  logic                     reset_n,
    input  logic                     enable,

    // Raw register 0x01 byte.  The 2026-08-15 bench sweep (upstream PR #24)
    // identified its low nibble as a 1-2-5 sample-rate ladder, measured as
    // 0x0F ~30 kS/s, 0x0E ~60k, 0x0D ~150k, 0x0C ~300k, 0x0B ~500-600k,
    // 0x0A ~1 MS/s, with 0x10/0x1F one further slow step at ~15 kS/s.  The
    // decode below scales the DIVISOR (the interval), so 0x0F carries the
    // largest ladder divisor and each faster step divides it (/2, /5, /10,
    // /20, /33) while 0x10/0x1F doubles it.  The absolute base clock is
    // unknown: BASE_DIVISOR is a parameter and only the measured ratios are
    // claimed.  0x08 (the stock arm default) is not on the ladder -- bench
    // shows tone-independent zero-crossing counts, i.e. not a uniform sample
    // rate -- so it keeps the old local slow-default divider as a distinct
    // unproven mode.
    input  logic [SELECT_WIDTH-1:0] raw_divisor,

    output logic                     divided_tick
);

    logic [DIVISOR_WIDTH-1:0] phase_counter;
    logic [DIVISOR_WIDTH-1:0] effective_divisor;

    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_SLOW_DEFAULT = 8'h08;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE         = 8'h0f;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE_DIV2    = 8'h0e;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE_DIV5    = 8'h0d;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE_DIV10   = 8'h0c;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE_DIV20   = 8'h0b;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_BASE_DIV33   = 8'h0a;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_15K_A        = 8'h10;
    localparam logic [SELECT_WIDTH-1:0] RATE_SEL_15K_B        = 8'h1f;

    always_comb begin
        effective_divisor = DIVISOR_WIDTH'(LEGACY_SLOW_DEFAULT_DIVISOR);
        unique case (raw_divisor)
            RATE_SEL_BASE: begin
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR);
            end
            RATE_SEL_BASE_DIV2: begin
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR / 2);
            end
            RATE_SEL_BASE_DIV5: begin
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR / 5);
            end
            RATE_SEL_BASE_DIV10: begin
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR / 10);
            end
            RATE_SEL_BASE_DIV20: begin
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR / 20);
            end
            RATE_SEL_BASE_DIV33: begin
                // Integer approximation of the measured ~1 MS/s point:
                // the interval is about 33.3x faster than the 0F interval.
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR / 33);
            end
            RATE_SEL_15K_A,
            RATE_SEL_15K_B: begin
                // 15 kS/s is one further slow step: twice the 0F interval.
                effective_divisor = DIVISOR_WIDTH'(BASE_DIVISOR * 2);
            end
            RATE_SEL_SLOW_DEFAULT: begin
                effective_divisor = DIVISOR_WIDTH'(LEGACY_SLOW_DEFAULT_DIVISOR);
            end
            default: begin
                effective_divisor = DIVISOR_WIDTH'(LEGACY_SLOW_DEFAULT_DIVISOR);
            end
        endcase
    end

    // Structural target (gw1n2-apicula M12): BSRAM_1/2 write on a gated
    // fabric clock whose gate cone is a self-feeding counter with an SPI-side
    // load path -- the structural definition of a programmable sample-rate
    // divider, not a bare enable.  This block reconstructs that shape on the
    // editable side: counter state feeding a periodic gate.  The exact stock
    // clock, divider preload convention, and semantics of 0x08 versus the
    // ladder modes are still not fully bench-closed, so this block claims only
    // the declared local decode and interval contract.
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            phase_counter <= '0;
            divided_tick  <= 1'b0;
        end else if (!enable) begin
            phase_counter <= '0;
            divided_tick  <= 1'b0;
        end else begin
            divided_tick <= 1'b0;
            if (phase_counter == effective_divisor) begin
                phase_counter <= '0;
                divided_tick  <= 1'b1;
            end else begin
                phase_counter <= phase_counter + 1'b1;
            end
        end
    end

endmodule
