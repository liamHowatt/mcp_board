#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sim;

enum sim_ConPin {
    sim_ConPinClk,
    sim_ConPinDat,
    sim_ConPinClear
};

struct sim *sim_create(void);
void sim_destroy(struct sim *sim);

void sim_eval(struct sim *sim);
void sim_con_pin_set(struct sim *sim, enum sim_ConPin pin, bool value);
void sim_input_pin_set(struct sim *sim, unsigned input_number, bool value);
bool sim_output_pin_check(struct sim *sim, unsigned output_number);


#ifdef __cplusplus
}
#endif
