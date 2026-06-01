`timescale 1ns / 1ps

module adder_tb;
    localparam integer LAT = 3;

    reg         clk;
    reg         rst_n;
    reg         clear;
    reg  [31:0] fp32_in;
    wire [31:0] fp32_out;

    integer cycle;
    reg [31:0] in_vec [0:11];
    reg        clear_vec [0:11];
    reg [31:0] exp_vec [0:11];

    adder #(
        .LAT(LAT)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .clear(clear),
        .fp32_in(fp32_in),
        .fp32_out(fp32_out)
    );

    initial begin
        clk = 1'b0;
        forever #5 clk = ~clk;
    end

    initial begin
        in_vec[0]  = 32'h3f800000; // a0 = 1.0, ignored by initial clear
        in_vec[1]  = 32'h3f800000; // a1 = 1.0
        in_vec[2]  = 32'h40000000; // a2 = 2.0
        in_vec[3]  = 32'h40400000; // a3 = 3.0
        in_vec[4]  = 32'h40800000; // a4 = 4.0, closes previous segment
        in_vec[5]  = 32'h40a00000; // a5 = 5.0
        in_vec[6]  = 32'h40c00000; // a6 = 6.0
        in_vec[7]  = 32'h40e00000; // a7 = 7.0
        in_vec[8]  = 32'h41000000; // a8 = 8.0
        in_vec[9]  = 32'h41100000; // a9 = 9.0
        in_vec[10] = 32'h41200000; // a10 = 10.0
        in_vec[11] = 32'h41300000; // a11 = 11.0

        clear_vec[0]  = 1'b1;
        clear_vec[1]  = 1'b0;
        clear_vec[2]  = 1'b0;
        clear_vec[3]  = 1'b0;
        clear_vec[4]  = 1'b1;
        clear_vec[5]  = 1'b0;
        clear_vec[6]  = 1'b0;
        clear_vec[7]  = 1'b0;
        clear_vec[8]  = 1'b0;
        clear_vec[9]  = 1'b0;
        clear_vec[10] = 1'b0;
        clear_vec[11] = 1'b0;

        exp_vec[0]  = 32'h00000000; // pipeline warm-up
        exp_vec[1]  = 32'h00000000; // pipeline warm-up
        exp_vec[2]  = 32'h00000000; // pipeline warm-up
        exp_vec[3]  = 32'h00000000; // clear at cycle 0
        exp_vec[4]  = 32'h3f800000; // a1
        exp_vec[5]  = 32'h40400000; // a1 + a2 = 3
        exp_vec[6]  = 32'h40c00000; // a1 + a2 + a3 = 6
        exp_vec[7]  = 32'h41200000; // a1 + a2 + a3 + a4 = 10
        exp_vec[8]  = 32'h40a00000; // a5
        exp_vec[9]  = 32'h41300000; // a5 + a6 = 11
        exp_vec[10] = 32'h41900000; // a5 + a6 + a7 = 18
        exp_vec[11] = 32'h41d00000; // a5 + a6 + a7 + a8 = 26
    end

    initial begin
        rst_n = 1'b0;
        clear = 1'b0;
        fp32_in = 32'h00000000;
        cycle = 0;

        repeat (3) @(posedge clk);
        rst_n = 1'b1;

        $display("");
        $display("adder timeline, LAT=%0d, II=1", LAT);
        $display("resource model: one FP32 accumulator datapath ~= ALU 30, FF 30, DSP 1");
        $display("cycle | clear | fp32_in    | fp32_out   | expected   | status | meaning");
        $display("------+-------+------------+------------+------------+--------+-------------------------------");

        for (cycle = 0; cycle < 12; cycle = cycle + 1) begin
            @(negedge clk);
            clear = clear_vec[cycle];
            fp32_in = in_vec[cycle];
            @(posedge clk);
            #1;
            $display(
                "%5d | %5b | 0x%08h | 0x%08h | 0x%08h | %s | %s",
                cycle,
                clear,
                fp32_in,
                fp32_out,
                exp_vec[cycle],
                (fp32_out === exp_vec[cycle]) ? "PASS" : "FAIL",
                meaning(cycle)
            );
        end

        $display("------+-------+------------+------------+------------+--------+-------------------------------");
        $display("Expected key point: cycle 7 is a1+a2+a3+a4, while cycle 8 is a5.");
        $display("");
        $finish;
    end

    function [8*31-1:0] meaning;
        input integer idx;
    begin
        case (idx)
        0:  meaning = "warm-up";
        1:  meaning = "warm-up";
        2:  meaning = "warm-up";
        3:  meaning = "clear0 -> 0";
        4:  meaning = "a1";
        5:  meaning = "a1+a2";
        6:  meaning = "a1+a2+a3";
        7:  meaning = "a1+a2+a3+a4";
        8:  meaning = "a5";
        9:  meaning = "a5+a6";
        10: meaning = "a5+a6+a7";
        11: meaning = "a5+a6+a7+a8";
        default: meaning = "";
        endcase
    end
    endfunction

endmodule
