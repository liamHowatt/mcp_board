
module base
(
  input clk_,
  input dat,
  input clear,
  input inp0,
  input inp1,
  input inp2,
  input inp3,
  input inp4,
  input inp5,
  input inp6,
  input inp7,
  input inp8,
  input inp9,
  input inp10,
  input inp11,
  input inp12,
  input inp13,
  input inp14,
  input inp15,
  input inp16,
  input inp17,
  input inp18,
  input inp19,
  input inp20,
  input inp21,
  input inp22,
  input inp23,
  input inp24,
  input inp25,
  input inp26,
  input inp27,
  input inp28,
  input inp29,
  input inp30,
  input inp31,
  input inp32,
  input inp33,
  input inp34,
  input inp35,
  input inp36,
  input inp37,
  input inp38,
  input inp39,
  input inp40,
  input inp41,
  input inp42,
  input inp43,
  input inp44,
  input inp45,
  input inp46,
  input inp47,
  output out0,
  output out1,
  output out2,
  output out3,
  output out4,
  output out5,
  output out6,
  output out7,
  output out8,
  output out9,
  output out10,
  output out11,
  output out12,
  output out13,
  output out14,
  output out15,
  output out16,
  output out17,
  output out18,
  output out19,
  output out20,
  output out21,
  output out22,
  output out23,
  output out24,
  output out25,
  output out26,
  output out27,
  output out28,
  output out29,
  output out30,
  output out31,
  output out32,
  output out33,
  output out34,
  output out35,
  output out36,
  output out37,
  output out38,
  output out39,
  output out40,
  output out41,
  output out42,
  output out43,
  output out44,
  output out45,
  output out46,
  output out47
);
  reg [48*48-1:0]big_mem = 0;
  reg [48-1:0]const_ones = 0;
  reg [4-1:0]bit_count = 0;
  reg [7+7-1:0]buff;

  always @(posedge clk_) begin
    if (clear) begin
      bit_count <= 0;
    end else if (bit_count == (7+7)) begin
      if (buff[7+7-1:7] == 48) begin
        const_ones <= const_ones | (48'b1 << buff[7-1:0]);
      end else if (buff[7+7-1:7] == (48 + 1)) begin
        const_ones <= const_ones & (~(48'b1 << buff[7-1:0]));
      end else begin
        big_mem[48*(buff[7+7-1:7]) +: 48] <= (buff[7-1:0] != 7'b0) ? (48'b1 << (buff[7-1:0] - 7'b1)) : 48'b0;
      end
    end else begin
      buff <= {buff[7+7-2:0], dat};
      bit_count <= bit_count + 1;
    end
  end

  assign {
    out47,
    out46,
    out45,
    out44,
    out43,
    out42,
    out41,
    out40,
    out39,
    out38,
    out37,
    out36,
    out35,
    out34,
    out33,
    out32,
    out31,
    out30,
    out29,
    out28,
    out27,
    out26,
    out25,
    out24,
    out23,
    out22,
    out21,
    out20,
    out19,
    out18,
    out17,
    out16,
    out15,
    out14,
    out13,
    out12,
    out11,
    out10,
    out9,
    out8,
    out7,
    out6,
    out5,
    out4,
    out3,
    out2,
    out1,
    out0
  } =
    ({48{1'b1}} & const_ones) |
    ({48{inp0}} & big_mem[47:0]) |
    ({48{inp1}} & big_mem[95:48]) |
    ({48{inp2}} & big_mem[143:96]) |
    ({48{inp3}} & big_mem[191:144]) |
    ({48{inp4}} & big_mem[239:192]) |
    ({48{inp5}} & big_mem[287:240]) |
    ({48{inp6}} & big_mem[335:288]) |
    ({48{inp7}} & big_mem[383:336]) |
    ({48{inp8}} & big_mem[431:384]) |
    ({48{inp9}} & big_mem[479:432]) |
    ({48{inp10}} & big_mem[527:480]) |
    ({48{inp11}} & big_mem[575:528]) |
    ({48{inp12}} & big_mem[623:576]) |
    ({48{inp13}} & big_mem[671:624]) |
    ({48{inp14}} & big_mem[719:672]) |
    ({48{inp15}} & big_mem[767:720]) |
    ({48{inp16}} & big_mem[815:768]) |
    ({48{inp17}} & big_mem[863:816]) |
    ({48{inp18}} & big_mem[911:864]) |
    ({48{inp19}} & big_mem[959:912]) |
    ({48{inp20}} & big_mem[1007:960]) |
    ({48{inp21}} & big_mem[1055:1008]) |
    ({48{inp22}} & big_mem[1103:1056]) |
    ({48{inp23}} & big_mem[1151:1104]) |
    ({48{inp24}} & big_mem[1199:1152]) |
    ({48{inp25}} & big_mem[1247:1200]) |
    ({48{inp26}} & big_mem[1295:1248]) |
    ({48{inp27}} & big_mem[1343:1296]) |
    ({48{inp28}} & big_mem[1391:1344]) |
    ({48{inp29}} & big_mem[1439:1392]) |
    ({48{inp30}} & big_mem[1487:1440]) |
    ({48{inp31}} & big_mem[1535:1488]) |
    ({48{inp32}} & big_mem[1583:1536]) |
    ({48{inp33}} & big_mem[1631:1584]) |
    ({48{inp34}} & big_mem[1679:1632]) |
    ({48{inp35}} & big_mem[1727:1680]) |
    ({48{inp36}} & big_mem[1775:1728]) |
    ({48{inp37}} & big_mem[1823:1776]) |
    ({48{inp38}} & big_mem[1871:1824]) |
    ({48{inp39}} & big_mem[1919:1872]) |
    ({48{inp40}} & big_mem[1967:1920]) |
    ({48{inp41}} & big_mem[2015:1968]) |
    ({48{inp42}} & big_mem[2063:2016]) |
    ({48{inp43}} & big_mem[2111:2064]) |
    ({48{inp44}} & big_mem[2159:2112]) |
    ({48{inp45}} & big_mem[2207:2160]) |
    ({48{inp46}} & big_mem[2255:2208]) |
    ({48{inp47}} & big_mem[2303:2256])
  ;
endmodule
