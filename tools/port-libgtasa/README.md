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

## Struct layouts

Field offsets and sizes written as `VER_x32 ? a : b` aren't addresses, so
`port` leaves them alone. `structs` checks them a different way: the 2.10
libraries carry Rockstar's DWARF debug info, and it compares every
`VALIDATE_SIZE` with the size of the engine class of that name (`ALIASES`
maps our renamed classes, such as `CWidgetGta` to `CWidget`).

```sh
tools/port-libgtasa/port.py structs lib64.so lib32.so
```

On 2.10 all 180 comparable checks match, plus one known, harmless
difference (`KNOWN`). 48 of our structs have no engine counterpart in the
debug info. Run it on the new libraries too: if they still have DWARF,
it tells you whether any class changed size. If they're stripped, only
in-game testing will catch a changed class.

It found that our `CSprite2d` had a second texture pointer the engine's
lacks. That put every field of every widget after its sprite at the wrong
offset (8 bytes on arm64, 4 on armv7). It's now one pointer, as in the
engine.

## Test

`test/test.sh` builds two small stand-in libraries with the NDK. In the
second build every address has moved and one function body differs. The
test checks that each kind of address is moved, kept or reported correctly,
and that porting a build to itself changes nothing. CI runs it in the
client job.
