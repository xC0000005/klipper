// Commands for sending messages on an I2C bus
//
// Copyright (C) 2018  Florian Heilmann <Florian.Heilmann@gmx.net>
// Copyright (C) 2018  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/misc.h" //timer_from_us
#include "command.h" // shutdown
#include "gpio.h" // i2c_transfer
#include "sam4e.h" // Twi
#include "sched.h" // sched_shutdown

void
i2c_init(Twi *p_twi)
{
    uint32_t twi_id = (p_twi == TWI0) ? ID_TWI0 : ID_TWI1;
    if ((PMC->PMC_PCSR0 & (1u << twi_id)) == 0) {
        PMC->PMC_PCER0 = 1 << twi_id;
    }
    if (p_twi == TWI0) {
        gpio_set_peripheral('A', PIO_PA4A_TWCK0, 'A', 0);
        gpio_set_peripheral('A', PIO_PA3A_TWD0, 'A', 0);
    } else {
        gpio_set_peripheral('B', PIO_PB5A_TWCK1, 'A', 0);
        gpio_set_peripheral('B', PIO_PB4A_TWD1, 'A', 0);
    }
    p_twi->TWI_IDR = 0xFFFFFFFF;
    (void)p_twi->TWI_SR;
    p_twi->TWI_CR = TWI_CR_SWRST;
    (void)p_twi->TWI_RHR;
    p_twi->TWI_CR = TWI_CR_MSDIS;
    p_twi->TWI_CR = TWI_CR_SVDIS;
    p_twi->TWI_CR = TWI_CR_MSEN;
     // cldiv = 74, chdiv = 68, ckdiv = 0
    p_twi->TWI_CWGR = TWI_CWGR_CLDIV(74) | TWI_CWGR_CHDIV(68) | TWI_CWGR_CKDIV(0);
}

struct i2c_config
i2c_setup(uint32_t bus, uint32_t rate, uint8_t addr)
{
    if ((bus > 1) | (rate < 400000))
        shutdown("Invalid i2c_setup parameters!");
    Twi *p_twi = (bus == 0) ? TWI0 : TWI1;
    i2c_init(p_twi);
    return (struct i2c_config){ .twi=p_twi, .addr=addr};
}

static void
i2c_wait(Twi* p_twi, uint32_t bit, uint32_t timeout)
{
    for (;;) {
        uint32_t flags = p_twi->TWI_SR;
        if (flags & bit)
            break;
        if (!timer_is_before(timer_read_time(), timeout))
            shutdown("i2c timeout");
    }
}

void
i2c_transfer(struct i2c_config config, uint8_t write_len, uint8_t *write
             , uint8_t read_len, uint8_t *read)
{
    Twi *p_twi = config.twi;

    // Send write_len bytes
    if (write_len) {
        p_twi->TWI_MMR = TWI_MMR_DADR(config.addr);
        for (;;) {
            uint32_t status = p_twi->TWI_SR;
            if (status & TWI_SR_NACK)
                shutdown("I2C NACK error encountered!");
            if (!(status & TWI_SR_TXRDY))
                continue;
            if (! write_len)
                break;
            p_twi->TWI_THR = *write++;
            write_len--;
        }
    }

    // Read read_len bytes
    if (read_len) {
        uint32_t cr = TWI_CR_START;
        p_twi->TWI_MMR = TWI_MMR_MREAD | TWI_MMR_DADR(config.addr);
        for (;;) {
            if (read_len == 1)
                cr |= TWI_CR_STOP;
            if (cr) {
                p_twi->TWI_CR = cr;
                cr = 0;
            }
            uint32_t status = p_twi->TWI_SR;
            if (status & TWI_SR_NACK) {
                shutdown("I2C NACK error encountered!");
            }
            i2c_wait(p_twi, TWI_SR_RXRDY, timer_read_time() + timer_from_us(5000));
            *read++ = p_twi->TWI_RHR;
            read_len--;
            if (! read_len)
                break;
        }
    } else {
        // Send STOP signal
        p_twi->TWI_CR = TWI_CR_STOP;
    }

    while (!(p_twi->TWI_SR & TWI_SR_TXCOMP)) {
    }
}
