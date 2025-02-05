#include "Vcrosspoint.h"
#include "verilated.h"
#include "sim.h"
#include <stdlib.h>
#include <assert.h>

struct sim {
  VerilatedContext *contextp;
  Vcrosspoint *top;
};

extern "C" struct sim *sim_create(void) {
  struct sim *sim = (struct sim *) malloc(sizeof(struct sim));
  assert(sim);
  sim->contextp = new VerilatedContext;
  sim->top = new Vcrosspoint{sim->contextp};
  return sim;
}
extern "C" void sim_destroy(struct sim *sim) {
  delete sim->top;
  delete sim->contextp;
  free(sim);
}

extern "C" void sim_eval(struct sim *sim) {
  sim->top->eval();
}
extern "C" void sim_con_pin_set(struct sim *sim, enum sim_ConPin pin, bool value) {
  switch (pin) {
    case sim_ConPinClk:
      sim->top->clk_ = value;
      break;
    case sim_ConPinDat:
      sim->top->dat = value;
      break;
    case sim_ConPinClear:
      sim->top->clear = value;
      break;
    default:
      assert(0);
  }
}
extern "C" void sim_input_pin_set(struct sim *sim, unsigned input_number, bool value) {
  assert(input_number < 48);
  if(value) {
    sim->top->inp |= 1lu << input_number;
  } else {
    sim->top->inp &= ~(1lu << input_number);
  }
}
extern "C" bool sim_output_pin_check(struct sim *sim, unsigned output_number) {
  assert(output_number < 48);
  return sim->top->outp & (1lu << output_number);
}
