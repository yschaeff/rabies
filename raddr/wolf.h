#ifndef WOLF_H
#define WOLF_H

#include <stdbool.h>

#define T0L 85
#define T0H 40
#define T1L 45
#define T1H 80

#define K 8

#define GROWL 1

#define BARK 0
#define HOWL 1

enum states {
    S_REST,       //do nothing
    S_ALERT,      //listen for howl
    S_HOWL,       //transmit frame
    S_BARK        //copy frame
};

extern void gpio_set(int);
extern int gpio_get(void);
extern void sleep_ns(int);
extern bool SOME_INPUT[K];
extern void update_input(void);

static inline void bark(int bit)
{
    gpio_set(1);
    sleep_ns(bit?T1H:T0H);
    gpio_set(0);
}

static inline void bark_full(int bit)
{
    bark(bit);
    sleep_ns(bit?T1L:T0L); //wait low time of bit
}

#endif
