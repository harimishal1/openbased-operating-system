
#include "rand.h"

#define RAND_NUM_RANGE 1000
uint64_t random_seed_current = 0xdeadbeef;

uint64_t random_uint_bounded(uint64_t upper_bound) {
	random_seed_current = ((random_seed_current + 491) * 167) % upper_bound;
	return random_seed_current;
}

uint64_t random_uint() {
	random_seed_current = ((random_seed_current + 491) * 167);
	return random_seed_current;
}

/*
 * A very primitive probability distribution simulated by generating random
 * numbers. If the random number is in the success range (<= (success_prob *
 * RAND_NUM_RANGE)) then the function returns true else false.
*/
bool random_decide(double success_prob) {
	uint64_t rand = random_uint_bounded(RAND_NUM_RANGE);
	return rand <= (success_prob * RAND_NUM_RANGE);
}

