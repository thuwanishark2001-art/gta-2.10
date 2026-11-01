# port-libgtasa

The client hooks and calls GTA: San Andreas (`libGTASA.so`) at fixed
addresses written for game version **2.10**. Another game build (the Play
Store now ships 2.11) puts the same code elsewhere, so those addresses must be
moved before the client can allow that version. `port.py` does the moving.

It finds every engine address in `app/src/main/cpp/samp` (both halves of each
`VER_x32 ? armv7 : arm64` and the bare ones inside `#if VER_x32`), and names
each one in terms that survive a rebuild:

| The address is…                                 | Named as                       | Moved when                                      |
|-------------------------------------------------|--------------------------------|-------------------------------------------------|
| a function or variable                          | symbol                         | the symbol exists in the new build              |
| inside a function (a patch site)                | symbol + offset                | the function is the same code (see below)       |
| inside a table or object (a vtable entry, say)  | symbol + offset                | the object is still big enough and the entry holds the same symbol |
| a GOT or pointer slot                           | relocation and its symbol      | exactly one matching slot exists                |
| a string in `.rodata`                           | its text                       | the text appears exactly once                   |

"The same code" means the same size and the same instructions once
PC-relative fields are masked out: branch targets, ADRP pages and their low
12 bits, Thumb literal loads and the literal pools they read. Those change
whenever anything around the function moves. Registers, struct offsets and
constants must still match, so a function that was edited is reported
rather than patched at a guessed offset.

It also checks that every symbol the client hooks by name
(`CHook::InlineHook("_Z...")` and the like, about 300) still exists in the
new build.

Anything the tool can't name or match is left as it is and printed for a
human to check. Sizes and field offsets written the same way
(`VALIDATE_SIZE(T, VER_x32 ? 0x20 : 0x28)`) aren't addresses and are skipped.

## Use

Pull `libGTASA.so` for each architecture out of the game (`lib/arm64-v8a/`
and `lib/armeabi-v7a/` in the APK or its split APKs). These are Rockstar's
files: keep them outside the repository and never commit them.

```sh
tools/port-libgtasa/port.py scan  old64.so old32.so                  # how many addresses can be named
tools/port-libgtasa/port.py check old64.so old32.so                  # armv7 and arm64 halves agree
tools/port-libgtasa/port.py port  old64.so old32.so new64.so new32.so          # report only
tools/port-libgtasa/port.py port  old64.so old32.so new64.so new32.so --write  # rewrite the sources
```

Pass `-` for a new library you don't have, to port only one architecture.
`port` exits 1 while any address still needs a human.

After `--write`, the launcher still accepts only the old version. In
`GameFiles.java`, set `REQUIRED_VERSION_ARM64` or `REQUIRED_VERSION_ARMV7`
for the architecture you ported, and replace its `KNOWN_GOOD` hashes;
`port` prints the new `libGTASA.so` hash. Leave the other architecture
alone: its addresses are still the old version's.

## Not covered

Struct layouts. Field offsets and sizes written as `VER_x32 ? a : b` aren't
addresses, so they are left alone. `VALIDATE_SIZE` only compares our own
struct definitions with our own numbers, so it can't notice a class Rockstar
changed. Only in-game testing catches that. (The 2.10 libraries carry DWARF
debug info. If the new build does too, its struct layouts could be compared
the same way; that isn't written yet.)

`check` needs only the 2.10 libraries. It found seven addresses where the two
halves of a ternary pointed at different functions, now fixed (for example,
arm64 `Font_SetRightJustifyWrap` was calling into `Interior_c::Lounge_AddHifi`).

## Test

`test/test.sh` builds two small stand-in libraries with the NDK. In the
second build every address has moved and one function body differs. The
test checks that each kind of address is moved, kept or reported correctly,
and that porting a build to itself changes nothing. CI runs it in the
client job.
