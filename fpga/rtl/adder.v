`timescale 1ns / 1ps

// II=1 FP32 streaming accumulator.
//
// Boundary rule:
//   - The first clear starts a new stream and produces a zero event.
//   - A later clear closes the current segment: fp32_in is still accumulated
//     into the previous segment, and the next cycle starts from zero.
//
// With LAT=3 and clear at cycles 0 and 4, the output stream is:
//   cycle 3: 0
//   cycle 4: a1
//   cycle 5: a1+a2
//   cycle 6: a1+a2+a3
//   cycle 7: a1+a2+a3+a4
//   cycle 8: a5
//
// The fp32_add function below is a compact synthesizable FP32 add model for
// bring-up and simulation. In the final FPGA implementation this add point can
// be replaced by a vendor floating-point accumulator/add IP configured as
// II=1. The clear/data timing around it should stay the same.
module adder #(
    parameter integer LAT = 3
) (
    input  wire         clk,
    input  wire         rst_n,
    input  wire         clear,
    input  wire  [31:0] fp32_in,
    output wire  [31:0] fp32_out
);

    localparam [31:0] FP32_ZERO = 32'h0000_0000;

    reg [31:0] acc_q;
    reg        active_q;

    wire [31:0] add_w = fp32_add(acc_q, fp32_in);
    wire [31:0] event_w = active_q ? add_w : FP32_ZERO;
    wire [31:0] acc_next_w = clear ? FP32_ZERO : event_w;
    wire        active_next_w = active_q | clear;

    generate
        if (LAT == 0) begin : gen_no_latency
            assign fp32_out = event_w;
        end else begin : gen_latency
            reg [31:0] pipe_q [0:LAT];
            integer i;

            always @(posedge clk or negedge rst_n) begin
                if (!rst_n) begin
                    for (i = 0; i <= LAT; i = i + 1) begin
                        pipe_q[i] <= FP32_ZERO;
                    end
                end else begin
                    pipe_q[0] <= event_w;
                    for (i = 1; i <= LAT; i = i + 1) begin
                        pipe_q[i] <= pipe_q[i-1];
                    end
                end
            end

            assign fp32_out = pipe_q[LAT];
        end
    endgenerate

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            acc_q <= FP32_ZERO;
            active_q <= 1'b0;
        end else begin
            acc_q <= acc_next_w;
            active_q <= active_next_w;
        end
    end

    function [31:0] fp32_add;
        input [31:0] a;
        input [31:0] b;

        reg sign_a;
        reg sign_b;
        reg sign_r;
        reg [7:0] exp_a;
        reg [7:0] exp_b;
        reg [7:0] exp_r;
        reg [26:0] man_a;
        reg [26:0] man_b;
        reg [27:0] man_sum;
        reg [26:0] man_r;
        reg swap;
        reg [31:0] big_v;
        reg [31:0] small_v;
        integer diff;
        integer sh;
        integer n;
    begin
        if (a[30:0] == 31'd0) begin
            fp32_add = b;
        end else if (b[30:0] == 31'd0) begin
            fp32_add = a;
        end else begin
            swap = ({b[30:23], b[22:0]} > {a[30:23], a[22:0]});
            big_v = swap ? b : a;
            small_v = swap ? a : b;

            sign_a = big_v[31];
            sign_b = small_v[31];
            exp_a = big_v[30:23];
            exp_b = small_v[30:23];
            man_a = {1'b1, big_v[22:0], 3'b000};
            man_b = {1'b1, small_v[22:0], 3'b000};

            diff = exp_a - exp_b;
            if (diff > 26) begin
                man_b = 27'd0;
            end else begin
                for (sh = 0; sh < diff; sh = sh + 1) begin
                    man_b = {1'b0, man_b[26:1]};
                end
            end

            exp_r = exp_a;
            sign_r = sign_a;

            if (sign_a == sign_b) begin
                man_sum = {1'b0, man_a} + {1'b0, man_b};
                if (man_sum[27]) begin
                    man_r = man_sum[27:1];
                    exp_r = exp_r + 8'd1;
                end else begin
                    man_r = man_sum[26:0];
                end
            end else begin
                if (man_a >= man_b) begin
                    man_r = man_a - man_b;
                    sign_r = sign_a;
                end else begin
                    man_r = man_b - man_a;
                    sign_r = sign_b;
                end

                if (man_r == 27'd0) begin
                    exp_r = 8'd0;
                    sign_r = 1'b0;
                end else begin
                    for (n = 0; n < 26; n = n + 1) begin
                        if (!man_r[26] && exp_r > 8'd0) begin
                            man_r = {man_r[25:0], 1'b0};
                            exp_r = exp_r - 8'd1;
                        end
                    end
                end
            end

            if (exp_r == 8'd0 || man_r == 27'd0) begin
                fp32_add = FP32_ZERO;
            end else begin
                fp32_add = {sign_r, exp_r, man_r[25:3]};
            end
        end
    end
    endfunction

endmodule
