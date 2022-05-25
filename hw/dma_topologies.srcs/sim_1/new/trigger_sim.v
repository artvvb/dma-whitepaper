`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: Digilent Inc
// Engineer: Arthur Brown
// 
// Create Date: 05/19/2022 02:49:07 PM
// Design Name: 
// Module Name: trigger_sim
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module trigger_sim;
    reg clk = 0;
    initial begin
        #1 clk = 1;
        forever #0.5 clk = ~clk;
    end
    reg resetn = 0;
    initial begin
        #1.5 resetn = 1;
    end
    
    reg    [31:0] trigger = 0;
    reg    [31:0] trigger_enable = 32'h0000ffff;
    reg    [31:0] trigger_to_last_delay = 110;
    wire          idle;
    reg           start = 0;
    wire   [31:0] trigger_detected;
    reg    [31:0] prebuffer_size = 750;
    
    wire   [31:0] s_tdata;
    reg           s_tvalid = 1;
    wire          s_tready;
        
    wire   [31:0] m_tdata;
    wire          m_tvalid;
    reg           m_tready = 1;
    wire          m_tlast;
    
    counter #(
        .HIGH         (32'hffffffff)
    ) counter_inst (
        .clock        (clk),
        .clock_enable (1),
        .sync_reset   (~resetn),
        .enable       (1),
        .count        (s_tdata),
        .tc           ()
    );

    always begin
        #400 trigger = 32'h00000002;
        #1 trigger = 0;
        #99;
    end

    initial begin
        #100 start = 1;
        #1 start = 0;
        #1899;
    end

    inject_tlast_on_trigger dut (
        .clk(clk),
        .resetn(resetn),
        
        .trigger(trigger),
        .trigger_enable(trigger_enable),
        .trigger_to_last_delay(trigger_to_last_delay),
        .idle(idle),
        .start(start),
        .trigger_detected(trigger_detected),
        .prebuffer_size(prebuffer_size),
        
        .s_tdata(s_tdata),
        .s_tvalid(s_tvalid),
        .s_tready(s_tready),
        
        .m_tdata(m_tdata),
        .m_tvalid(m_tvalid),
        .m_tready(m_tready),
        .m_tlast(m_tlast)
    );
endmodule
