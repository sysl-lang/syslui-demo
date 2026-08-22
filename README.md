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
| 30 | 74 | 2.1 µs | 10.8 µs | 91 |
| 300 | 614 | 13.5 µs | 114.5 µs | 631 |
| 3000 | 6014 | 124.1 µs | 1400.8 µs | 6031 |

Linear in the node count, **one allocation a node and no more**, and about 22 ns a node to rebuild.
The largest of those is 0.75% of a 60 Hz frame to rebuild and 8.4% to rebuild, lay out and paint.

The table moved once since it was first taken, and by a little: the rows are produced by `sysl.seq`'s
`map` and a container takes its children with one `extend` rather than a `push` a child, so a rebuild
grows its storage once instead of doubling its way up. Measured against the older shape on the same
machine and the same compiler, seven runs each, 300 rows went from a median 16.8 µs and 641
allocations to 13.5 µs and 631. **The allocations that went are the reallocations**, and the one a
node is what remains.

## What a person driving it costs, which the batch numbers hide

Forty seconds of clicking and scrolling, 300 rows, logged a second at a time. These are the only
figures here a person had to produce, so they were not retaken when the batch table above moved;
what they compare is one kind of rebuild against another, which that change does not touch.

| what is happening | rebuilds a second | cost each |
|---|---:|---:|
| scrolling — a burst of wheel events | 11–20 | **20–22 µs** |
| one click, then a pause | 1–2 | **26–37 µs** |

**Sustained interaction is the cheap case**, by half again. The likely reason is warmth rather than
anything structural: a burst hands 614 boxes back to the allocator and immediately asks for 614
more, so the free list and the cache lines are exactly right, where an isolated rebuild after an
idle second finds neither. Worth a second measurement before it is called proven — but it is
consistent across forty samples, and it points the same way as everything else here.

**So the per-frame arena is refused.** It would attack the eighth of the frame that is allocation
and leave the seven-eighths that is layout and paint exactly where it was. What is worth attacking,
in the order the measurement puts it: **culling** — nothing here is culled, so all 3000 rows are
measured and painted with twenty visible — then the **double measure** a container does, then
allocation a distant third. And note which way the interactive numbers cut: the case a person
actually feels is the one where allocation is *cheapest*, so an arena would be attacking the 27 µs
nobody notices.

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
