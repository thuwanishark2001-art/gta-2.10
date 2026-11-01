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

/* Hooked by (mangled) name in the test's sources. */
int kept(int a) __asm__("_Z4kepti");
int kept(int a) { return a + 2; }
#ifndef NEW
int renamed_in_new(int a) __asm__("_Z14renamed_in_newi");
int renamed_in_new(int a) { return a + 1; }
#endif

int uses_import(void) { return imported + counter; }

const char *format(void) { return "engine/%s/%d.dat"; }

/* For `structs`: 16 bytes on arm64, 8 on armv7. */
struct Gadget { void *texture; int flags; };
struct Gadget gadget;

/* Renamed between builds the way 2.11 renamed GTA's text functions:
 * unsigned short* became short*. */
#ifdef NEW
int draw_text(const short *s) __asm__("_Z9draw_textPs");
#else
int draw_text(const unsigned short *s) __asm__("_Z9draw_textPt");
#endif
#ifdef NEW
int draw_text(const short *s) { return s != 0; }
#else
int draw_text(const unsigned short *s) { return s != 0; }
#endif

/* A static only the code that uses it can find once NEW is stripped. */
static int hidden_state = 3;
__attribute__((noinline)) int read_hidden(int a) { return hidden_state + a; }
__attribute__((noinline)) void write_hidden(int a) { hidden_state = a * 2; }
