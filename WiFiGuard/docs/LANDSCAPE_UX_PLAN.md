# WiFiGuard Landscape UI/UX Architecture — Risk-Score-Centric Redesign

## 0. Critical Discovery: Coordinate Mismatch

The current firmware sets `TFT_ROTATION 1` which on the LilyGO T-Display ST7789 produces
a **240-wide × 135-tall landscape** display. However, `Config.h` defines `TFT_WIDTH=135`
and `TFT_HEIGHT=240` (the physical panel dimensions), and `DisplayDriver::width()` returns
`TFT_WIDTH`. This means all existing UI code draws within a 135×240 portrait coordinate
space on a 240×135 landscape display — the bottom ~105 pixels of every screen are off-screen.

**Resolution**: Update `Config.h` to match the rotated dimensions:
```
#define TFT_WIDTH   240
#define TFT_HEIGHT  135
```
This single change makes every `TFT_WIDTH` and `TFT_HEIGHT` reference throughout the
firmware map to the actual visible canvas.

---

## 1. Landscape Display Fundamentals

### 1.1 Canvas Dimensions

```
+--------------------------------------------------+  y=0
|                                                  |
|              240 pixels wide                     |
|                                                  |
|              135 pixels tall                     |
|                                                  |
+--------------------------------------------------+  y=135
```

- **Horizontal real estate**: 240px = **40 characters** at size 1 (6px/char) or **20 characters** at size 2 (12px/char)
- **Vertical real estate**: 135px = **~16 lines** at 8px line height, or **~11 lines** at 12px line height
- **Density**: 260 PPI — size-1 text is small but legible; size-2 is comfortable

### 1.2 Fundamental Layout Advantage

Landscape gives 77% more horizontal pixels than portrait (240 vs 135). This enables:
- **Side-by-side panels** (impossible in portrait)
- **Risk score displayed large** alongside secondary data
- **List items with more columns** without truncation
- **Footer hints on one line** without abbreviation

### 1.3 Layout Zones

```
+---------------------------------------------------+  y=0
|  HEADER (12px)                                     |  Title + battery
+---------------------------------------------------+  y=12
|                                                    |
|  CONTENT AREA (240 × 111px)                        |  Screen-specific
|                                                    |
+---------------------------------------------------+  y=123
|  FOOTER (12px)                                     |  Button hints
+---------------------------------------------------+  y=135
```

- **Header**: 12px. Left: screen title. Right: battery %. Background: `COL_CHROME`.
  Separated from content by 1px `COL_DIM` line at y=12.
- **Content**: y=13 to y=122 = **110px tall**, 240px wide.
- **Footer**: 12px. Button hints. Background: `COL_CHROME`.
- **Margins**: 4px left, 2px right. Content width = 234px usable.

Header and footer are shorter than in portrait (12px vs 14px) because vertical space is
the scarce resource in landscape.

---

## 2. Risk Score as Central Visual Element

### 2.1 Design Philosophy

The user's primary question is: **"Is this network safe or dangerous?"**

The risk score (0–100) is the single best answer to that question. It must be:
- The **largest element** on the detail screen
- **Color-coded** so meaning is instant (green/orange/red)
- Visible **within 0.5 seconds** of looking at the screen
- Supported by a **text label** (LOW / MED / HIGH) for users who don't know the scale

Everything else is supporting context.

### 2.2 Risk Score Visual Specification

On the detail screen, the risk score occupies a dedicated **left panel**:

```
+----------+----------------------------------+
|          |  MyNetwork_5G                    |
|   72     |  WPA2  ch6  -45dBm  Cisco       |
|  HIGH    |  -- Safety --                    |
|          |  Open, Weak signal               |
|  [====]  |  ! Possible evil twin            |
|          |  -- Connection --                 |
|          |  Grade: Fast  Portal: OK         |
+----------+----------------------------------+
```

**Left panel** (70px wide):
- Risk score number: **size 3** (18×24px per digit) — 2–3 digits centered
- Risk label: **size 1**, centered below score
- Color gauge bar: 60px wide × 6px tall, filled proportionally
  - 0–34: green fill, green score text
  - 35–64: orange fill, orange score text
  - 65–100: red fill, red score text

**Right panel** (170px wide):
- All other network data, grouped into sections

This split layout is impossible in portrait (only 135px wide) but natural in landscape.

### 2.3 Risk Color Application

The risk color system applies everywhere:

| Element | Color Rule |
|---------|-----------|
| Score number (detail) | risk color, size 3 |
| Score label (detail) | risk color, size 1 |
| Gauge bar (detail) | risk color fill |
| Score in list item | risk color, size 1 |
| Risk reasons (detail) | risk color text |
| Alert banner (list) | `COL_DANGER` text |
| Riskiest network (env) | risk color |
| Stability verdict | green/orange/red |
| Avg risk (session) | risk color |

---

## 3. Screen-by-Screen Landscape Layout

### 3.1 Idle Screen

```
+---------------------------------------------------+
|  WiFiGuard                              87%        |
+---------------------------------------------------+
|                                                    |
|        WiFiGuard                                   |  size 2, centered
|        WiFi Security Scanner                       |  size 1, dim
|                                                    |
|     ---- Last scan ----                            |  dim separator
|     12 networks | 3 open | 1 risky                |
|                                                    |
+---------------------------------------------------+
|  Lh:Scan                            Rh:Settings    |
+---------------------------------------------------+
```

- If no previous scan: "No scans yet — hold LEFT to start"
- The 240px width allows the last-scan summary on **one line** instead of two
- "Lh" = hold LEFT, "Rh" = hold RIGHT

### 3.2 Scanning Screen

```
+---------------------------------------------------+
|  Scanning                               87%        |
+---------------------------------------------------+
|                                                    |
|           Scanning . . .                           |  size 2, green
|                                                    |
|           Elapsed: 3s                              |  size 1, dim
|                                                    |
+---------------------------------------------------+
|                                       Rh:Cancel    |
+---------------------------------------------------+
```

- Animated dots cycle (. .. ... ....)
- Elapsed time updated by clearing only the time region
- Only the dot and time regions need partial redraw

### 3.3 Network List

The landscape list is the most dramatically improved screen. With 240px width,
each list item can show **all key data on one line** without truncation:

```
+---------------------------------------------------+
|  Networks                               87%        |
+---------------------------------------------------+
|  |MyNetwork_5G      WPA2  -45  R:15 L  c6         |  <- selected (cyan bar)
|   CoffeeShop_Free   Open  -62  R:72 H  c1  N!     |
|   Airport_WiFi      Open  -78  R:85 H  c6  !      |
|   HomeNet           WPA3  -32  R: 5 L  c11        |
|   Guest_Net         WPA2  -55  R:28 L  c6         |
|   FreePublicWifi    Open  -81  R:91 H  c1  N      |
|   Hidden_AP         WPA2  -70  R:18 L  c3         |
|                                                   ||  <- scroll indicator
+---------------------------------------------------+
|  10 nets | Open:3 | Sort:Signal                    |
|  L:> Lh:Sel R:< Rh:Env                            |
+---------------------------------------------------+
```

**Column layout** (left to right):
- **Selection bar**: 3px cyan vertical bar on selected item
- **SSID**: up to 18 chars (108px), truncated with ".." — evil twin SSIDs in red
- **Auth**: 4 chars in auth color (Open=red, WPA2=green, etc.)
- **RSSI**: 3–4 chars in dim
- **Risk**: "R:NN" in risk color
- **Label**: "L"/"M"/"H" in risk color (single char)
- **Channel**: "cN" in dim
- **Badges**: "N" (cyan), "!" (red)

**Item height**: 12px (single line). In 110px content, that's **9 items** visible.
This is more than the portrait two-line layout which showed ~9 items at 20px each,
but each item now contains MORE data because of the wider canvas.

**Scroll indicator**: 2px track on right edge, proportional thumb.

**Status/footer**: With 240px width, the status line and button hints both fit
comfortably. No need for extreme abbreviation.

**Alert banner**: Red text at top of content area if high-risk alert active:
```
! HIGH RISK: Open network detected                  x
```
Dismissible with B2.

### 3.4 Network Detail — Risk-Centric Layout

This is the centerpiece of the redesign. The screen is split into two panels:

```
+---------------------------------------------------+
|  Detail                                 87%        |
+---------------------------------------------------+
|          |                                         |
|   72     |  MyNetwork_5G                           |
|  HIGH    |  -- Identity --                         |
|          |  WPA2  ch6  -45dBm  Cisco               |
|  [=====] |  Trend: +3 dBm                         |
|          |  -- Safety --                           |
|          |  Open, Weak signal, Dup SSID            |
|          |  WARNING: evil twin!                     |
|          |  -- Connection --                        |
|          |  Grade: Fast  Portal: OK  Ping:120ms    |
+---------------------------------------------------+
|  L:Test  Lh:Stability  R:Back                      |
+---------------------------------------------------+
```

**Left panel** (70px, x=0..69):
- Background: `COL_BG` (black) — same as content
- Risk score: **size 3** (each digit is 18px wide × 24px tall)
  - Centered horizontally in the 70px panel
  - Positioned at y=20 (within content area)
  - Color: risk color (green/orange/red)
- Risk label: **size 1**, centered below score at y=48
  - "LOW" / "MED" / "HIGH" in risk color
- Gauge bar: 60px wide × 6px tall at y=60
  - Background: `COL_CHROME`
  - Fill: risk color, width proportional to score (0=empty, 100=full)
- Vertical separator: 1px `COL_DIM` line at x=69, full content height

**Right panel** (170px, x=71..239):

All data organized in sections:

1. **SSID** (y=14, white, size 1): Full SSID, up to 28 chars on this panel width
2. **-- Identity --** (dim separator)
   - Auth (in auth color) + channel + RSSI + vendor (if available)
   - RSSI trend if previous scan data exists
3. **-- Safety --** (dim separator)
   - Risk reasons as comma-separated list (risk color)
   - Evil twin warning in `COL_DANGER` if applicable
   - "Protected - pwd required" in `COL_SAFE` if encrypted
4. **-- Connection --** (dim separator, only if tested)
   - Grade (in grade color) + Portal result
   - Benchmark ping/jitter if available

**Expert mode**: BSSID shown at very bottom in dim before footer.

**Footer**: Varies by network type:
- Open: `L:Test  Lh:Stab  R:Back`
- Protected: `Lh:Stab  R:Back`

This layout makes the risk score **immediately visible** — it's the largest visual
element on the entire screen. A user can glance at the screen and know: red 85 = danger.

### 3.5 Environment Summary — Two Pages

**Page 1: Environment**

```
+---------------------------------------------------+
|  Environment                            87%        |
+---------------------------------------------------+
|  Networks: 12    Open: 3    Enc: 8    Hid: 1       |
|  Dup SSID: 2     Portals: 0                        |
|  -- Channels --                                    |
|  Best: c1 (least busy)     Worst: c6 (busiest)    |
|  Congestion: 4/10                                  |
|  -- Quick Picks --                                 |
|  Strong: HomeNet -32dBm                            |
|  Risky: FreeWiFi 85       Open: CoffeeShop         |
+---------------------------------------------------+
|  L:Session  R:Back                                 |
+---------------------------------------------------+
```

Note how landscape allows **two-column layout** for Quick Picks (risky + open on same line).

**Page 2: Session**

```
+---------------------------------------------------+
|  Session                                87%        |
+---------------------------------------------------+
|  Scans: 5         Avg networks: 11                 |
|  Avg risk: 34     Most common ch: 6                |
|  Portals seen: 1  Open seen: 8                     |
|  Dup SSIDs: 3                                      |
|                                                    |
+---------------------------------------------------+
|  L:Environ  R:Back                                 |
+---------------------------------------------------+
```

B1 cycles between pages. The landscape layout allows **paired columns** so
all session stats fit on one page without scrolling.

### 3.6 Settings — Highlighted Scrollable List

```
+---------------------------------------------------+
|  Settings                               87%        |
+---------------------------------------------------+
|  -- Display --                                     |
|  Sort              Signal                          |  <- COL_SEL highlight
|  Filter            None                            |
|  Expert mode       OFF                             |
|  -- Behavior --                                    |
|  Monitor           OFF                             |
|  Alert             ON                              |
|  Low power         OFF                             |
|  Demo mode         OFF                             |
|  -- Data --                                        |  (scroll to see)
+---------------------------------------------------+
|  L:Next  R:Change  Rh:Done                         |
+---------------------------------------------------+
```

- Selected row highlighted with `COL_SEL` background
- Labels expanded (not abbreviated)
- Values in `COL_INFO` (cyan)
- 240px width means label + value fit on one line without truncation
- Section headers in `COL_DIM`
- Row height: 12px. Visible rows: ~9. Scrollable if more.

### 3.7 Testing — Phase Progress

```
+---------------------------------------------------+
|  Testing                                87%        |
+---------------------------------------------------+
|  CoffeeShop_Free                                   |
|                                                    |
|  [1/4] Connecting        OK                        |
|  [2/4] DNS check         OK                        |
|  [3/4] HTTP test         ...                       |
|  [4/4] Benchmark         ---                       |
|                                                    |
+---------------------------------------------------+
|  (please wait)                                     |
+---------------------------------------------------+
```

- Phase labels are wider (e.g., "Connecting" not "Conn")
- Results right-aligned: OK (green), FAIL (red), ... (animated), --- (pending)
- On completion, show "Complete!" in green

### 3.8 Stability Monitor — Bar Graph

```
+---------------------------------------------------+
|  Stability                              87%        |
+---------------------------------------------------+
|  MyNetwork_5G    Samples: 7/10                     |
|  Now:-45  Min:-52  Max:-41  Avg:-46                |
|                                                    |
|  |##  ## ###  # ##   |   <- bar graph (10 bars)    |
|  |##  ## ###  # ##   |                             |
|  |## ### #### # ###  |                             |
|  |## ### #### ####   |                             |
|                                                    |
|  Verdict: STABLE (spread: 11dB)                    |
+---------------------------------------------------+
|  R:Stop                                            |
+---------------------------------------------------+
```

- Bar graph: 10 bars × 20px wide = 200px. Fits easily in 240px landscape.
- Each bar colored by RSSI: green (>-50), orange (-50 to -70), red (<-70)
- Verdict word in semantic color (STABLE=green, MODERATE=orange, UNSTABLE=red)
- Numbers on one line (landscape advantage)

### 3.9 Export Screen

```
+---------------------------------------------------+
|  Export                                 87%        |
+---------------------------------------------------+
|                                                    |
|  Sent to Serial (USB)                              |
|  Format: CSV       Networks: 12                    |
|  BSSID: masked                                     |
|                                                    |
+---------------------------------------------------+
|  L:Done                                            |
+---------------------------------------------------+
```

### 3.10 Debug Screen

```
+---------------------------------------------------+
|  Debug                                  87%        |
+---------------------------------------------------+
|  Manufacturing Mode                                |
|  Chip: ESP32-D0WD r1    Heap: 142560 free          |
|  Flash: 4096KB           Sketch: 856KB             |
|  Uptime: 342s            Bat: 87% 3.92V            |
|  BTN1: ---               BTN2: ---                 |
+---------------------------------------------------+
|  R:Exit  Rh:NVS clear                              |
+---------------------------------------------------+
```

Landscape allows **two-column** layout for all debug info — everything fits
without scrolling.

### 3.11 Sleep Screen

Black screen, backlight off. Any button wakes to Idle.

---

## 4. Navigation Architecture

### 4.1 Screen Graph

```
Idle ──hold L──> Scanning ──auto──> List
Idle ──hold R──> Settings
List ──hold L──> Detail
List ──hold R──> Env Summary (page 1)
List ──both──> Export
Detail ──tap L (open)──> Testing ──auto──> Detail
Detail ──hold L──> Stability Monitor
Detail ──tap R──> List
Env (page 1) ──tap L──> Env (page 2) ──tap L──> Env (page 1)
Env ──tap R──> List
Settings ──hold R──> List (or Idle if no scan)
Settings ──both──> Debug
Stability ──tap R──> Detail
Debug ──tap R──> Idle
Sleep ──any button──> Idle
```

### 4.2 Button Convention

| Button | Tap | Hold |
|--------|-----|------|
| LEFT (B1) | Next / Forward / Confirm | Select / Drill down / Start operation |
| RIGHT (B2) | Previous / Back / Dismiss | Exit context / Secondary menu |
| Both | Rare action (export, debug) | — |

### 4.3 Footer Format

With 240px width, footers can be more descriptive:
```
L:Next  Lh:Select  R:Previous  Rh:Environment
```
No need for extreme abbreviation. Use "Lh" for "hold LEFT" and "Rh" for "hold RIGHT".

---

## 5. Alert and Warning Design

### 5.1 Alert Hierarchy

**Level 1 — Banner** (list screen top):
- Red text on black: `! HIGH RISK: Open network detected    x`
- Dismissible with R tap
- Triggered by: high-risk open network, anomaly (many new APs)

**Level 2 — Toast** (above footer, any screen):
- Orange bar, black text
- Auto-dismisses after 3 seconds
- Used for: low battery skip, target AP not found, info messages

**Level 3 — Inline** (within screen content):
- Colored text integrated into the screen
- Used for: evil twin warning, risk reasons, auth type coloring
- Always visible when viewing the relevant screen

### 5.2 Evil Twin Indicators

- **List view**: SSID rendered in `COL_DANGER` (red), "!" badge in red
- **Detail view**: Full red line: "WARNING: Possible evil twin!"
- **Left panel**: Score likely already high (red), reinforcing danger

---

## 6. Color System (Unchanged from Portrait Plan)

The semantic color palette remains the same — it's device-independent:

| Purpose | Constant | Hex |
|---------|----------|-----|
| Background | `COL_BG` | `0x0000` |
| Primary text | `COL_FG` | `0xFFFF` |
| Secondary text | `COL_DIM` | `0x7BEF` |
| Safe / LOW | `COL_SAFE` | `0x07E0` |
| Warning / MED | `COL_WARN` | `0xFD20` |
| Danger / HIGH | `COL_DANGER` | `0xF800` |
| Info / badges | `COL_INFO` | `0x04FF` |
| Selected item | `COL_SEL` | `0x18E3` |
| Chrome | `COL_CHROME` | `0x1082` |

---

## 7. Typography in Landscape

| Size | Char dimensions | Max chars at 240px | Usage |
|------|----------------|-------------------|-------|
| Size 3 | 18×24px | ~13 | Risk score number ONLY |
| Size 2 | 12×16px | ~20 | "WiFiGuard" title, "Scanning", stability verdict |
| Size 1 | 6×8px | ~40 | Everything else |

Size 3 is newly introduced for the risk score. At 18px per digit, a 3-digit score ("100")
takes 54px — fits comfortably in the 70px left panel.

---

## 8. Landscape vs Portrait Comparison

| Aspect | Portrait (135×240) | Landscape (240×135) |
|--------|-------------------|-------------------|
| List items per screen | ~9 (20px two-line) | ~9 (12px single-line) |
| Data per list item | SSID + badges only | SSID + auth + RSSI + risk + ch + badges |
| Detail layout | Linear top-to-bottom | Split panel (score left, data right) |
| Risk score size | Size 1 (6px) inline | Size 3 (18px) dedicated panel |
| Env summary | Two pages needed | Two pages, but each has two-column layout |
| Settings | 1 column | Label + value side by side |
| Footer | Abbreviated | Full words |
| Header | Abbreviated | Full title + battery |
| Scroll before data loss | ~15 lines before overflow | ~9 lines, but wider columns reduce need |

The landscape layout trades vertical item count for dramatically better information
density per item and the ability to use side-by-side panels.

---

## 9. Rendering Performance

### 9.1 Flicker Reduction

Same strategy as portrait:
- `fillScreen()` ONLY on screen transitions (state or view change)
- Within a screen, clear only the content area with `fillRect()`
- Track `lastDrawnState_` and `lastDrawnView_` for transition detection
- Header and footer persist without redraw (drawn on `COL_CHROME` background)

### 9.2 Partial Redraw Optimization

**List view**: When only selection changes, redraw only the old and new selected items.
Track `prevListIndex_`; if scroll position didn't change, only 2 items need redraw.

**Scanning**: Only update the dot animation and elapsed time regions.

**Detail**: Full redraw on entry (acceptable since it only happens on navigation).
No redraw while viewing same network.

**Settings**: Redraw only old and new selected rows on cursor move.

### 9.3 Timing

| Screen | Redraw interval |
|--------|----------------|
| General | 100ms |
| Scanning | 500ms |
| Stability | 500ms |
| Debug | 200ms |

### 9.4 Memory Consideration

The landscape canvas is 240×135×2 = 64,800 bytes for a full-screen sprite.
This is feasible on ESP32 (320KB RAM) if a double-buffer approach is desired.
However, the partial-redraw strategy is lighter and sufficient.

---

## 10. Implementation Notes

### 10.1 Config.h Changes Required

```c
#define TFT_WIDTH   240   // was 135
#define TFT_HEIGHT  135   // was 240
```

This is the foundational change. Every UI calculation that references `TFT_WIDTH`
or `TFT_HEIGHT` will automatically adapt.

### 10.2 New Layout Constants

```c
#define HDR_H       12    // was 14 in portrait
#define FTR_H       12    // was 14 in portrait
#define CONTENT_Y   13    // HDR_H + 1px separator
#define CONTENT_H   110   // 135 - 13 - 12
#define FTR_Y       123   // 135 - 12
#define ITEM_H      12    // single-line items (was 20 two-line in portrait)
#define SCORE_PANEL_W  70 // left panel for risk score on detail screen
```

### 10.3 Detail View Split Panel

The detail screen needs a fundamentally different draw function:

```
drawDetailLeftPanel()   — risk score, label, gauge (x=0..69)
drawDetailRightPanel()  — sections with network data (x=71..239)
```

The left panel is a self-contained drawing region. The vertical separator at x=69
is a 1px dim line.

### 10.4 List Item Layout

Single-line items at 12px height with columnar data:

```
x=0..2:    selection bar (3px)
x=4..111:  SSID (up to 18 chars × 6px = 108px)
x=114..137: auth (4 chars × 6px = 24px)
x=140..163: RSSI (4 chars × 6px = 24px)
x=166..195: risk "R:NN L" (5 chars × 6px = 30px)
x=198..215: channel "cNN" (3 chars × 6px = 18px)
x=218..237: badges "N!" (2 chars × 6px + gaps)
x=238..239: scroll indicator (2px)
```

Total: fits in 240px with clear column gaps.

---

## 11. Summary: Why Landscape + Risk-Centric is Superior

1. **Risk score visibility**: Size 3 in a dedicated panel vs. size 1 inline text
2. **Information density**: Single-line list items show ALL key data
3. **Split panels**: Impossible in 135px portrait, natural in 240px landscape
4. **Footer clarity**: Full button labels without abbreviation
5. **Two-column data**: Debug, env summary, session stats all fit better
6. **Fewer pages**: Each screen can show more data per page
7. **Better hierarchy**: Large score + small supporting text = instant comprehension

The landscape orientation transforms WiFiGuard from a device that requires reading
into a device that can be understood at a glance.
