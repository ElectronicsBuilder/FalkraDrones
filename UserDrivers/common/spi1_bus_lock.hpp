#ifndef SPI1_BUS_LOCK_HPP
#define SPI1_BUS_LOCK_HPP

void spi1_bus_lock_init();

class Spi1BusGuard {
public:
    Spi1BusGuard();
    ~Spi1BusGuard();

    Spi1BusGuard(const Spi1BusGuard&) = delete;
    Spi1BusGuard& operator=(const Spi1BusGuard&) = delete;

private:
    bool locked_ = false;
};

#endif // SPI1_BUS_LOCK_HPP
