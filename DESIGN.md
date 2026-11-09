# Design Specification (DESIGN.md) — Game Boy 8-bit

**English** · [Türkçe](DESIGN.tr.md)

<!-- impeccable:design-schema 1 -->

## 1. Visual identity

The app behaves like a **Game Boy DMG** screen: only four shades of green, sharp
pixels, hard frames and a system font. The goal is a nostalgic game-console feel,
not decoration. Every element either serves a function or takes no space.

**Physical scene:** a family member at home in the evening, at the kitchen table,
pasting a link with one hand. That is why the text is large and clear, targets are
easy to click, and the window is fixed and predictable.

### Ground rules
1. **Four shades:** no intermediate colors. A 1 px dither pattern is used when needed.
2. **Pixel corners:** corners are rounded but never blurred: 5 px on large surfaces,
   3 px on controls, cut with a 45° pixel stair-step (`gb_fill_round`,
   `gb_frame_round`, `gb_round_window`). Anti-aliased (blended) rounding is banned.
   Frames and shadows are 2 px and hard.
3. **Shadow = 2 px offset:** no blurred shadow, gradient or glow. The shadow is
   clipped to the area outside the body (`gb_shadow`), so the cut corner gets no notch.
4. **Custom drawing:** buttons, chips, checkbox, dropdown, progress bar, toast,
   confirmation dialog and title bar are drawn with `BS_OWNERDRAW`/GDI. Native
   `COMBOBOX`, `BUTTON` (radio/checkbox) and `PROGRESS_CLASS` are not used.
   The only exception is the URL input box (`EDIT`), because IME, clipboard and text
   selection reliability require a native control.
5. **No external assets:** no image, icon file or font is shipped. Everything except
   the tray icon is drawn in code.
6. **Fixed layout:** the window is 646×485 design units; it does not jump when content
   changes. Only DPI scales it.

---

## 2. Palette

| Token | Hex | Role |
| :--- | :--- | :--- |
| `GB_DARKEST` | `#0F380F` | Text, frame, console outer frame, panel header strip, selected surface, filled progress |
| `GB_DARK` | `#306230` | Page background, button face, hover, shadow, divider, secondary surface |
| `GB_LIGHT` | `#8BAC0F` | Progress channel, recessed area, passive dither |
| `GB_LIGHTEST` | `#9BBC0F` | Panel and input surface, light text |

Contrast: `GB_DARKEST` / `GB_LIGHTEST` ≈ **6.0:1** (enough for body text).
`GB_LIGHTEST` / `GB_DARK` ≈ **3.7:1**; therefore only large, bold text (title,
badge, strip) is written in `GB_LIGHTEST` on a dark background. Small body text is
always `GB_DARKEST` on a light panel.

---

## 3. Typography

The installed **Consolas** is used, drawn with `NONANTIALIASED_QUALITY`. This gives
both 8-bit sharpness and guaranteed accented characters (ğ, ş, ı, İ, ç, ö, ü) without
increasing the package size.

| Token | Definition | Usage |
| :--- | :--- | :--- |
| `FONT_TITLE` | Consolas 20 bold | App name |
| `FONT_BOLD` | Consolas 14 bold | Section heading, chip, important button |
| `FONT_BODY` | Consolas 14 | Body, media title |
| `FONT_SMALL` | Consolas 12 | Technical data, small buttons, notes |

Hierarchy: **app name → section strip → media title → body → technical data.**

---

## 4. Metrics and layout

- **Window:** 646×485 client area; fixed size, its own title bar.
  Corners are cut at 5 px with a region (`gb_round_window`).
- **Title bar:** 44 px. Draggable (`HTCAPTION`), minimize/close buttons on the right
  (26×24). The platform badge sits left of the buttons.
- **Console frame:** 6 px `GB_DARKEST` outer frame + 1 px `GB_LIGHTEST` inner line.
  Background `GB_DARK`.
- **8 px grid:** outer padding 16–22 px, section gap 14 px, inner padding 8 px.
- **Section panels:** `GB_LIGHTEST` background, 2 px `GB_DARKEST` frame, 5 px cut
  corners. A full-width 20 px dark header strip on top
  (`1 LINK`, `2 MEDIA & FORMAT`, `3 DOWNLOAD STATUS`); the strip sits inside the
  panel frame and its corners stay under the frame.
- **Fixed media height:** 212 px; the empty state, metadata, options and note share
  this area. The window does not grow with the content.

---

## 5. Component language

### A. Button (`gb_draw_control`, role `PUSH` / `PRIMARY` / `DANGER`)
Face `GB_LIGHTEST` (`GB_DARKEST` for primary/danger actions), 2 px `GB_DARKEST`
frame, 2 px hard `GB_DARK` shadow at the bottom-right, 3 px cut corner. When pressed
the face turns `GB_DARK` and the content moves down 1 px. When disabled the background
is filled with a `GB_LIGHT` dither pattern. On hover the face turns `GB_DARK` and the
text `GB_LIGHTEST`. Focus adds a 1 px inner frame.

### B. Choice chip (`CHIP`)
Video / Audio selection. Selected: `GB_DARKEST` background + `GB_LIGHTEST` text.
Unselected: `GB_LIGHTEST` background + `GB_DARK` frame. Hover: `GB_LIGHT`.

### C. Checkbox (`CHECK`)
14×14 px square; `GB_LIGHTEST` background, 2 px `GB_DARKEST` frame. When checked the
inside becomes `GB_DARKEST` and a 2 px pixel check mark is drawn in `GB_LIGHTEST`.

### D. Dropdown (`DROP` + `gb_dropdown_open`)
Looks like a button when closed; it has a pixel down-arrow on the right. Clicking opens
a `WS_POPUP` list: `GB_LIGHTEST` background, 2 px frame, 24 px rows. Selected row
`GB_DARKEST`/`GB_LIGHTEST`, hover `GB_DARK`/`GB_LIGHTEST`, with a 2×4 px mark left of
the selected row. `↑/↓`, `Enter`, `Esc` and clicking outside are supported.

### E. Progress bar
14 px high; `GB_LIGHT` channel, 2 px `GB_DARKEST` frame, 3 px cut corner. The empty
channel has 1 px `GB_DARK` divider lines, so the bar is not mistaken for the URL input.
The fill consists of `GB_DARKEST` blocks with a 7 px block + 2 px gap rhythm. No
animation; it updates with real data.

### F. URL input
Native `EDIT` control; surrounded by a 2 px `GB_DARKEST` frame. Background
`GB_LIGHTEST`, text `GB_DARKEST`, 8 px horizontal inner padding. The caret is
`GB_DARKEST` when focused.

### G. Toast
370×96 px, `GB_LIGHTEST` background, 2 px frame. A 22 px `GB_DARKEST` strip on top:
platform badge (`GB_DARK`), duration, `X` on the right. Below it the title,
"READY TO DOWNLOAD - CLOSES IN 4 S" and a `DOWNLOAD` button on the right. It closes
after 4 seconds; hovering resets the timer.

### H. Modal message box (`gb_message_box`)
Opens centered over its owner. `GB_LIGHTEST` background, 2 px frame, 26 px
`GB_DARKEST` title strip, 30×30 icon square on the left (`X`, `!`, `?`). Buttons are
right-aligned at the bottom: `OK`, `YES`, `NO`, `CANCEL`. `Enter` triggers the default,
`Esc` triggers cancel. The owner window is disabled while the modal is open.

---

## 6. States

| State | Appearance |
| :--- | :--- |
| Empty | Media panel shows "Enter or paste a link." + supported platforms |
| Fetching | `Fetch Info` → `Fetching...`, button disabled, status "Fetching info..." |
| Ready | Title + duration/platform/stream count + target container note; `DOWNLOAD` enabled |
| Downloading | Blocks fill; `%`, speed, remaining time and `MB / MB` on the status line |
| Processing | Bar at 100%, "Merging streams and finishing the container..." |
| Finished | "File saved successfully.", `Open File Location` visible |
| Error | Modal message box with an `X` icon; error summary on the status line |
| Disabled | Dithered background, `GB_DARK` frame/text, no click |

---

## 7. Prohibitions

- Any color outside the four shades; gradient, blur, glow, soft shadow.
- Anti-aliased (blurry) corners; cards inside cards.
- Native `COMBOBOX`, radio/checkbox, `PROGRESS_CLASS` (the URL `EDIT` is the exception).
- Icons imitated with emoji or unicode symbols.
- Decorative animation; staggered entrance effects.
- Adding external image/font files (size and determinism).
- Writing small text on a `GB_DARK` background (contrast).

---

## 8. Implementation notes

- Palette and drawing primitives: `src/gb_ui.h`, `src/gb_ui.c`.
- Corner/shadow primitives: `gb_fill_round`, `gb_frame_round`, `gb_shadow`,
  `gb_round_window`. Radii are `GB_RADIUS_SMALL` (3) / `GB_RADIUS_LARGE` (5).
- Main window, layout and state management: `src/ui.c`.
- Scaling: `gb_scale()` / `S()` — 96 DPI base, Per-Monitor V2.
- Size budget: **< 1 MB mandatory**; the current build is ~355 KB.
- When adding a new component: pick the color from the palette, use a 2 px frame, draw
  all five states (normal, hover, pressed, disabled, focus), and keep the text short and
  plain English.
