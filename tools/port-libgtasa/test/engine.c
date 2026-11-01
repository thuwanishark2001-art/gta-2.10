/* Stand-in for libGTASA. Built twice: OLD as is, NEW with -DNEW, which
 * moves everything (padding first) and changes the body of changed(). */
#include <stddef.h>

#ifdef NEW
__attribute__((used)) int padding_fn(int x) { return x * 3 + 1; }
__attribute__((used)) char padding_data[0x400] = {1};
#endif

int counter = 1;
int table[8] = {1, 2, 3, 4, 5, 6, 7, 8};
extern int imported;

__attribute__((noinline)) int same(int a, int b)
{
    int r = a;
    for (int i = 0; i < b; i++) r = r * 7 + i;
    return r + counter;
}

__attribute__((noinline)) int changed(int a)
{
#ifdef NEW
    return a * 5 + counter + 2;
#else
    return a * 3 + counter;
#endif
}

int uses_import(void) { return imported + counter; }

const char *format(void) { return "engine/%s/%d.dat"; }
