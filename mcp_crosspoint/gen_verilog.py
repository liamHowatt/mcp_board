import sys

def gen_verilog(N: int, out_file: str):
  template = """
module base
(
  input clk_,
  input dat,
  input clear,
  {input_params},
  {output_params}
);
  reg [{N}*{N}-1:0]big_mem = 0;
  reg [{N}-1:0]const_ones = 0;
  reg [{bit_count_l}-1:0]bit_count = 0;
  reg [{inpi_l}+{outi_l}-1:0]buff;

  always @(posedge clk_) begin
    if (clear) begin
      bit_count <= 0;
    end else if (bit_count == ({inpi_l}+{outi_l})) begin
      if (buff[{outi_l}+{inpi_l}-1:{outi_l}] == {N}) begin
        const_ones <= const_ones | ({N}'b1 << buff[{outi_l}-1:0]);
      end else if (buff[{outi_l}+{inpi_l}-1:{outi_l}] == ({N} + 1)) begin
        const_ones <= const_ones & (~({N}'b1 << buff[{outi_l}-1:0]));
      end else begin
        big_mem[{N}*(buff[{outi_l}+{inpi_l}-1:{outi_l}]) +: {N}] <= (buff[{outi_l}-1:0] != {outi_l}'b0) ? ({N}'b1 << (buff[{outi_l}-1:0] - {outi_l}'b1)) : {N}'b0;
      end
    end else begin
      buff <= {{buff[{inpi_l}+{outi_l}-2:0], dat}};
      bit_count <= bit_count + 1;
    end
  end

  assign {{
    {output_drives}
  }} =
    ({{{N}{{1'b1}}}} & const_ones) |
    {output_expr}
  ;
endmodule
"""

  def log2(x):
    ret = 0
    while x:
      x >>= 1
      ret += 1
    return ret

  inpi_l = 7
  assert log2(N-1 + 2) <= inpi_l
  outi_l = 7
  assert log2(N-1 + 1) <= outi_l
  bit_count_l = log2(inpi_l + outi_l)

  verilog = template.format(
    N=N,
    inpi_l=inpi_l,
    outi_l=outi_l,
    bit_count_l=bit_count_l,
    input_params=",\n  ".join(f"input inp{i}" for i in range(N)),
    output_params=",\n  ".join(f"output out{i}" for i in range(N)),
    output_drives=",\n    ".join(f"out{i}" for i in reversed(range(N))),
    output_expr=" |\n    ".join(f"({{{N}{{inp{i}}}}} & big_mem[{i*N+N-1}:{i*N}])" for i in range(N)),
  )

  with open(out_file, "w") as f:
    f.write(verilog)

def main():
  N = int(sys.argv[1])
  out_file = sys.argv[2]
  gen_verilog(N, out_file)

if __name__ == "__main__":
  main()
