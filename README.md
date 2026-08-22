# syslui-demo

The worked example for [`syslui`](https://github.com/sysl-lang/syslui), and the program that
answered card `0203`'s fourth question.

Two things in one binary, deliberately:

```
sysl build . --lib ../syslui
./syslui-demo              # a window: a counter, two buttons, a 300-row scrolling list
./syslui-demo --bench      # no window, no display server — prints what a rebuild costs
./syslui-demo --bench 3000 # …at whatever tree size you ask for
```

The window is what a person can click. The benchmark is what a machine can check, and it is the
half that decided something.

## What it measured

Every allocation the program makes goes through a counter — `package.hocon`'s `allocator` block
points sysl's storage at libc's `malloc` with a `++` in front of it, so these are the program's
real allocation traffic rather than a model of it.

| rows | boxed nodes | rebuild | + layout and paint | allocations a rebuild |
|---:|---:|---:|---:|---:|
| 30 | 74 | 2.7 µs | 13.3 µs | 93 |
| 300 | 614 | 15.5 µs | 118.0 µs | 641 |
| 3000 | 6014 | 141.8 µs | 1337.3 µs | 6047 |

Linear in the node count, **one allocation a node and no more**, and about 24 ns a node to rebuild.
The largest of those is 0.85% of a 60 Hz frame to rebuild and 8% to rebuild, lay out and paint.

**So the per-frame arena is refused.** It would attack the eighth of the frame that is allocation
and leave the seven-eighths that is layout and paint exactly where it was. What is worth attacking,
in the order the measurement puts it: **culling** — nothing here is culled, so all 3000 rows are
measured and painted with twenty visible — then the **double measure** a container does, then
allocation a distant third.

## Why the text cache is not an optimization

Rasterizing a glyph run is a shaping pass and a texture upload. A loop that did it per row per frame
would report SDL's cost as syslUI's, which is exactly the mistake this binding makes easy — so the
canvas remembers both the texture and the measurement. Card `0203` warned the probe not to prove the
architecture fast by accident, and this is that warning obeyed.

## What the program is

```
column(spacing = 10):
    text(s"count: ${count.read()}").padding(6)

    row([
        button("increment", () -> count.set(count.read() + 1)),
        button("decrement", () -> count.set(count.read() - 1))
    ], 10)

    scroll(column(items.view(), 0), offset, 420).background(PANEL)
```

The whole tree is rebuilt on every signal write — deliberately the whole tree, so the number is the
worst case rather than what rebuilding only what read the signal would buy later.
