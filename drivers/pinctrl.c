#include <common.h>
#include <sys/riscv.h>


int pinctrl_set(uint32_t pin, uint32_t sel) {
    uint32_t reg = PINCTRL_BASE + (pin*4);
    DEBUG("[PIN_CTRL] sel 0x%X => 0x%X\n", sel, reg);
    putreg32(sel, reg);
    DEBUG("[PIN_CTRL] 0x%X = 0x%X\n", reg, getreg32(reg));
    return 0;
}