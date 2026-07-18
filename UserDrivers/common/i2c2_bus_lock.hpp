#ifndef I2C2_BUS_LOCK_HPP
#define I2C2_BUS_LOCK_HPP

// Serializes access to the shared I2C2 bus (SHT4x, BQ27441, BMP581), which is
// polled from multiple tasks (status task, battery monitor task, console
// commands). Without this, concurrent transactions collide on the HAL handle
// and all sensors on the bus intermittently report failures.
// Same pattern as Spi1BusGuard.

void i2c2_bus_lock_init();

class I2c2BusGuard {
public:
    I2c2BusGuard();
    ~I2c2BusGuard();

    I2c2BusGuard(const I2c2BusGuard&) = delete;
    I2c2BusGuard& operator=(const I2c2BusGuard&) = delete;

private:
    bool locked_ = false;
};

#endif // I2C2_BUS_LOCK_HPP
