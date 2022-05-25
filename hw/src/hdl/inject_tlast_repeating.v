`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 05/19/2022 01:35:37 PM
// Design Name: 
// Module Name: inject_tlast_repeating
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


module inject_tlast_repeating (
    input [31:0] data_cmp_value,
    input [31:0] data_cmp_mask,
    input [31:0] s_tdata,
    input s_tvalid,
    output s_tready,
    output [31:0] m_tdata,
    output m_tvalid,
    input m_tready,
    output m_tlast
    );
    assign m_tlast = (data_cmp_value == (s_tdata & data_cmp_mask)) ? 1'b1 : 1'b0;
    assign s_tready = m_tready;
    assign m_tvalid = s_tvalid;
    assign m_tdata = s_tdata;
endmodule
