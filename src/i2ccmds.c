// Commands for sending messages on an I2C bus
//
// Copyright (C) 2018  Florian Heilmann <Florian.Heilmann@gmx.net>
// Copyright (C) 2018  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "basecmd.h" // oid_alloc
#include "board/gpio.h" // i2c_transfer
#include "command.h"  // sendf

struct i2cdev_s {
    struct i2c_config i2c_config;
};

void
command_config_i2c(uint32_t *args)
{
    struct i2cdev_s *i2c = oid_alloc(args[0], command_config_i2c, sizeof(*i2c));
    i2c->i2c_config = i2c_setup(args[1], args[2], args[3]);
}
DECL_COMMAND(command_config_i2c, "config_i2c oid=%c bus=%u rate=%u addr=%u");

void
command_i2c_transfer(uint32_t *args)
{
    uint8_t oid = args[0];
    struct i2cdev_s *i2c = oid_lookup(oid, command_config_i2c);
    uint8_t write_len = args[1];
    uint8_t *write = (void*)(size_t)args[2];
    uint8_t read_len = args[3];
    uint8_t read[read_len];
    i2c_transfer(i2c->i2c_config, write_len, write, read_len, read);
    sendf("i2c_transfer_response oid=%c response=%*s", oid, read_len, read);
}
DECL_COMMAND(command_i2c_transfer, "i2c_transfer oid=%c write=%*s read=%c");
