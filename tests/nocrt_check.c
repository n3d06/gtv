/* Link lõi Telex với /NODEFAULTLIB để chắc chắn lõi không cần CRT. */
#include "../src/telex.h"

int entry(void)
{
    Telex t;
    TelexEdit e;
    telex_init(&t);
    telex_key(&t, L'a', &e);
    return telex_key(&t, L's', &e) && e.len == 1 && e.text[0] == 0x00E1 ? 0 : 1;
}
