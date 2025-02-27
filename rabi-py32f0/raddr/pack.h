#ifndef PACK_H
#define PACK_H
enum CryCommand  {
    CRY_OKAY,
    CRY_RESET,
};

void join_cry(int bit, enum CryCommand cmd);
void rally_pack();

#endif

