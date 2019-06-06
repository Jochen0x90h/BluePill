#include <stddef.h>
#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>


int main(void) {
	//rcc_clock_setup_in_hsi_out_24mhz();

	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	int i = 0;
	while (1) {
		gpio_set(GPIOC, GPIO13);
		gpio_clear(GPIOC, GPIO13);
		++i;
	}

	return 0;
}
