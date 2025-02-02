module crosspoint
(
  input clk_,
  input dat,
  input clear,
  input [47:0] inp,
  output [47:0] outp
);
  reg [48*49-1:0] matrix = 0;
  wire [48:0] inp_one = {inp, 1'b1};
  reg [3:0] bit_count = 0;
  reg [11:0] addr;

  always @(negedge clk_) begin
    if (clear) begin
      bit_count <= 0;
    end else if (bit_count == 12) begin
      matrix[addr] <= dat;
    end else begin
      addr <= {dat, addr[11:1]};
      bit_count <= bit_count + 1;
    end
  end

  generate
    genvar i;
    for (i = 0; i < 48; i = i + 1) begin
      assign outp[i] = |(matrix[i*9 +: 9] & inp_one);
    end
  endgenerate
endmodule

