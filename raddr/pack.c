/**
 * Reverse Addressable Binary Input
 * Slave implementation: "The Pack"
 **/
#include <stdio.h>

#include "wolf.h"

#define DBG 0

void statemachine(int bit, uint32_t elapsed)
{
    static int state = S_REST;
    static int bark_i = K;

    if (elapsed > 4*STFU) {
        state = S_REST;
    }

    switch (state) {
        case S_REST:
            if (DBG) printf("REST\r\n");
            if (bit == GROWL) { //growl
                if (DBG) printf("goto ALERT\r\n");
                state = S_ALERT;
                bark(GROWL); //wake up next with growl
            } else {
                /* a BARK makes no sense here. Therefor just absorb it */
            }
            break; //Wait for next bit
        case S_ALERT:
            if (DBG) printf("ALERT\r\n");
            if (bit != HOWL) {
                bark(BARK); //Yelp, so next will copy next frame
                if (DBG) printf("goto BARK\r\n");
                state = S_BARK; //
                break; //Wait for next bit
            }
            bark_full(BARK); //Yelp, so next will copy next frame
            update_input();
            for (int k=0; k<K; k++) {
                bark_full(SOME_INPUT[k]);
            }
            if (DBG) printf("goto HOWL\r\n");
            state = S_HOWL; // not really needed
            /* FALL-THROUGH */
        /* Since we do not have to wait for a bit we do a fall through here.
         * It *is* an actual state in the finite automata sense. */
        case S_HOWL:
            if (DBG) printf("HOWL\r\n");
            //Maybe include parity bit?
            bark_full(GROWL); //wake up next with growl
            bark(HOWL); //Howl, so next will also go to S_HOWL
            if (DBG) printf("goto REST\r\n");
            state = S_REST; //we are the last. Get some rest.
            break; //Wait for next bit
        case S_BARK:
            if (DBG) printf("BARK\r\n");
            bark(bit); //Copy input to output
            if (!--bark_i) {
                if (DBG) printf("goto ALERT\r\n");
                state = S_ALERT;
                bark_i = K;
                //maybe check parity? Go to S_REST on parity fail?
            }
            break; //Wait for next bit
    }
}
