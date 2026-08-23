# syslui-demo

The worked example for [`syslui`](https://github.com/sysl-lang/syslui) — every control the toolkit
has, drawn by a software rasterizer and handed to SDL3 as one streaming texture.

![the demo](shot.png)

That picture is not a photograph of a window. It is `./syslui-demo --shot`, which draws one frame
and asks PlutoVG to write its own surface out — so it is byte for byte what the window shows, and
it is produced by the same program with no display server anywhere near it.

```
sysl build . --lib ../syslui
./syslui-demo              # a window: buttons, a switch, a checkbox, a slider, a scrolling list
./syslui-demo --shot       # no window — draws one frame and writes shot.png
./syslui-demo --bench      # no window — prints what a rebuild, a rasterize and an upload cost
./syslui-demo --bench 3000 # …at whatever list length you ask for
```

The window is what a person can click. The benchmark is what a machine can check. The screenshot is
the only automated thing that says what the interface *looks* like — every test in the package
asserts a recording, and a recording cannot tell you a label has fallen off its button.

## The split this demonstrates

**syslUI draws; it does not present.** What comes out of it is a block of premultiplied ARGB32
pixels in a PlutoVG surface. Getting those onto a screen is the application's job, and here it is
one `TextureAccess.Streaming` texture uploaded once a frame — `PixelFormat.Argb8888` is SDL's name
for exactly the bytes PlutoVG writes, so nothing is converted.

That split is why the same tree, the same widgets and the same backend serve a desktop and a phone:
SDL3 is the presenter on both, and cairo is available on neither.

**There is no SDL_ttf here, and no FreeType and no fontconfig.** PlutoVG rasterizes glyphs through
the `stb_truetype` it vendors, so text is measured and drawn by the same library that draws
everything else — which also means the metrics are identical on every platform, where two font
stacks would have disagreed.

## What it measures

Every allocation the program makes goes through a counter — `package.hocon`'s `allocator` block
points sysl's storage at libc's `malloc` with a `++` in front of it, so these are the program's real
allocation traffic rather than a model of it. A frame at 900×660:

| rows | boxed nodes | rebuild | + rasterize | upload | allocations a rebuild |
|---:|---:|---:|---:|---:|---:|
| 30 | 92 | 2.8 µs | 4.71 ms | 65 µs | 124 |
| 300 | 632 | 14.6 µs | 4.65 ms | 66 µs | 664 |
| 3000 | 6032 | 126.9 µs | 5.21 ms | 64 µs | 6064 |

**The rebuild is free** — linear in the node count, one allocation a node and no more, about 21 ns a
node, and 0.76% of a 60 Hz frame at three thousand rows.

**The frame is nearly flat in the list length**, which it was not before culling landed. Three
hundred rows used to cost 7.74 ms against twenty rows' 4.98 ms, because every row outside the
window was painted and then clipped away by the backend. A container now asks the canvas whether a
child is reachable before painting it, and the three numbers above are the same to within noise.

**The upload is not the problem anybody expects it to be.** 2.3 MiB across to the GPU every frame is
0.4% of the budget. Whole-surface uploads are what streaming textures are for.

What is left is the rasterizer, and it is almost all **text**: the same window with no list at all
draws in 0.51 ms. PlutoVG rasterizes each glyph's outline afresh every frame, so the next thing
worth attacking is a glyph cache in that package, not anything in syslUI. In practice the demo holds
a vsync-locked 120 fps.

**So the per-frame arena the mobile survey argued for is refused, and by a wider margin than
before**: allocation is three parts in a thousand of a frame, and the thing next to it is four
orders of magnitude larger.

## What a person driving it costs, which the batch numbers hide

Forty seconds of clicking and scrolling, 300 rows, logged a second at a time. These predate the
PlutoVG backend and measure a **rebuild**, which is the half that does not depend on how the frame
is drawn.

| what is happening | rebuilds a second | cost each |
|---|---:|---:|
| scrolling — a burst of wheel events | 11–20 | **20–22 µs** |
| one click, then a pause | 1–2 | **26–37 µs** |

**Sustained interaction is the cheap case**, by half again. The likely reason is warmth rather than
anything structural: a burst hands hundreds of boxes back to the allocator and immediately asks for
hundreds more, so the free list and the cache lines are exactly right, where an isolated rebuild
after an idle second finds neither. Note which way that cuts — the case a person actually feels is
the one where allocation is *cheapest*.

## What the program is

```
column(spacing = 12):
    text("syslUI — every control there is").padding(4)

    divider(EDGE)

    row([
        text(s"count: ${m.count.read()}").padding(6).frame(150, 30),
        button("increment", bump(m.count, 1)),
        button("decrement", bump(m.count, -1), 0x8A4A3Au32)
    ], 10)

    row([
        checkbox(m.roomy, "roomy rows"),
        spacer().frame(40, 1),
        text("night").padding(2),
        switch(m.night)
    ], 12)

    row([
        text("volume").padding(4).frame(90, 26),
        slider(m.volume, 0, 100, 260),
        text(s"${m.volume.read()}%").padding(4)
    ], 10)

    row([progress(real(m.volume.read()) / 100.0, 380), spacer()], 0)

    divider(EDGE)

    scroll(column(rows.map(r -> text(r).padding(gap)), 0), m.offset, 420)
        .background(ground, 8)
```

**Two of the controls change the screen rather than decorate it**, which is the point: the checkbox
sets how much room a row gets and the switch sets what the list sits on. Each is a signal write, a
rebuild, and a visibly different tree — a demo whose switches only moved themselves would be showing
the animation and hiding the architecture.

**The last line is a call chain broken before its dot**, which no released compiler could read when
this was written. A chain of modifiers is how anything gets styled, so a chain that cannot be broken
is a chain that has to fit on one line.

**The progress bar sits in a `Row` with a spacer** because a `Column` gives every child the column's
whole width — which is what the dividers and the panel want and not what a bar 380 wide wants. A
`Row` honours what a child measured to.

The whole tree is rebuilt on every signal write — deliberately the whole tree, so the number is the
worst case rather than what rebuilding only what read the signal would buy later. The frame is drawn
every time round regardless, because a hover easing toward its colour is a change to the picture and
not to the state.
