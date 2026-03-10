# WiFiGuard UX Redesign v2 — Safety-First for Non-Technical Users

## 0. Critique of Current UI Direction

### What works
- Risk scoring engine is solid (0–100 with explainable reasons)
- Evil twin / duplicate SSID detection is genuinely useful
- Async scan pipeline is efficient and non-blocking
- Stability monitor provides real value for power users
- Two-button navigation is consistent (L=forward, R=back)
- Landscape layout plan is well-suited to the hardware

### What needs to change

1. **Technically-oriented language everywhere.** The list screen shows
   `WPA2  -45  R:15  c6` — none of which means anything to a non-technical user.
   Auth types, RSSI in dBm, channel numbers, and bare risk scores are
   implementation artifacts, not user-facing information.

2. **No verdicts, no recommendations.** The detail screen shows risk reasons
   like "Open, Weak signal, Duplicate SSID" — but never tells the user what to
   *do*. There is no "Safe to use" or "Avoid this network" anywhere.

3. **Risk score is a number without context.** A score of 42 means nothing
   unless the user knows the scale. The current UI shows the number and a
   label (LOW/MED/HIGH) but never translates it to a decision.

4. **Testing progress is technical.** "Joining WiFi / Checking DNS / Testing
   internet / Speed test" exposes implementation phases. Users don't know
   what DNS is, and the four-step progress creates anxiety.

5. **Environment summary is a data dump.** Channel counts, congestion scores,
   and "best/worst channel" are for network engineers, not travelers checking
   coffee-shop WiFi.

6. **Settings overwhelm.** Ten options with names like "Filter," "Monitor,"
   "Diagnostics," and "Export mode" assume technical expertise.

7. **No safety guidance for open networks.** The device detects open networks
   and can test them, but never tells the user "use a VPN" or "don't enter
   passwords" — the most important advice.

8. **Captive portal detection stops at "Login page required."** The redirect
   URL is available in firmware but not surfaced. Users have no way to open
   the portal on their phone.

9. **No clear mental model.** Users don't know which screens exist, how to
   get to them, or what the button conventions are.

---

## 1. Proposed UX Philosophy

### 1.1 Core Principles

```
VERDICT FIRST → DATA SECOND → EXPERT OPTIONAL
```

Every screen answers four questions in order:
1. **Is it safe?**      → Large, color-coded verdict
2. **Will it work?**    → Connection status / test result
3. **Why?**             → One plain-English sentence
4. **What should I do?** → Concrete recommended action

### 1.2 Design Rules

| Rule | Application |
|------|-------------|
| Plain English by default | "Password protected" not "WPA2-PSK" |
| Verdicts before numbers | "SAFE" shown before risk score |
| Color = meaning | Green=safe, Orange=caution, Red=avoid, Cyan=info |
| Signal as bars not dBm | ▓▓▓░ not "-45 dBm" |
| Actions not descriptions | "Use with VPN" not "Open network detected" |
| Progressive disclosure | Simple mode by default, expert mode opt-in |
| Consistent buttons | L=primary/next, R=secondary/back. Always. |

### 1.3 Safety Verdict System

The device reduces all analysis to one of six verdicts:

| Verdict | Color | Meaning | When |
|---------|-------|---------|------|
| **SAFE** | Green | Use without worry | Encrypted + strong signal + no risk flags |
| **CAUTION** | Orange | Usable but be careful | Moderate risk or weak signal |
| **AVOID** | Red | Do not connect | High risk, evil twin, or serious flags |
| **LOGIN REQ** | Cyan | Portal login needed | Tested, captive portal detected |
| **NO INTERNET** | Dim/gray | Connected but offline | Tested, no internet connectivity |
| **UNTESTED** | Dim/gray | Open, not yet tested | Open network, connectivity unknown |

For encrypted networks that cannot be tested:

| Sub-verdict | Color | When |
|-------------|-------|------|
| **PROTECTED** | Green | WPA2/WPA3, good signal, no flags |
| **WEAK LOCK** | Orange | WEP or WPA-only (deprecated security) |

### 1.4 Mode System

| Mode | Audience | What's visible |
|------|----------|----------------|
| **Simple** (default) | Non-technical users | Verdicts, signal bars, recommendations, explanations |
| **Expert** | Technical users | All of Simple + RSSI, channel, auth type, BSSID, risk score breakdown, raw diagnostics |

Mode toggle is the first item in Settings.

---

## 2. Information Architecture & Screen Flow

### 2.1 Screen Map

```
                         ┌──────────┐
                         │   IDLE   │  Power-on landing
                         └────┬─────┘
                              │
              Hold L          │          Hold R
         ┌────────────────────┼────────────────────┐
         ▼                    │                    ▼
   ┌──────────┐               │             ┌──────────┐
   │ SCANNING │               │             │ SETTINGS │
   └────┬─────┘               │             └──────────┘
        │ auto                │
        ▼                     │
   ┌──────────┐               │
   │  RESULTS │  (network list, env, detail)
   └────┬─────┘               │
        │                     │
        ├── Detail ──L──► TESTING ──auto──► TEST RESULT
        │                                       │
        ├── Detail ──Hold L──► STABILITY        │
        │                                       │
        └── Chord (both) ──► EXPORT             │
                                                │
   Any state + inactivity ──────────────► SLEEP │
   Settings + chord ────────────────────► DEBUG │
```

### 2.2 View System (within RESULTS state)

```
RESULTS state contains sub-views:

  LIST ◄──────────── DETAIL ◄──────── TEST_RESULT
   │                    │
   └──► ENVIRONMENT     └──► STABILITY
          │
          └──► SESSION
```

### 2.3 Global Button Convention

| Context | L tap | L hold | R tap | R hold |
|---------|-------|--------|-------|--------|
| **All screens** | Primary action | Deep action / Start | Secondary / Back | Exit context |
| **Idle** | — | Start scan | — | Settings |
| **Scanning** | — | — | — | Cancel |
| **List** | Next network ▼ | View detail | Prev network ▲ | Environment |
| **Detail** | Test (open) / — | Stability | Back to list | — |
| **Test Result** | Back to detail | — | Back to list | — |
| **Environment** | Next page | — | Back to list | — |
| **Settings** | Next option | — | Change value | Done (exit) |
| **Stability** | — | — | Stop, back | — |
| **Export** | Done | — | — | — |
| **Sleep** | Wake | — | Wake | — |
| **Debug** | — | — | Exit | Factory reset |

---

## 3. Screen-by-Screen Redesign

### 3.1 IDLE — "Ready" Screen

**Purpose:** Communicate readiness. Show last-scan safety summary if available.

```
┌────────────────────────────────────────────┐
│ WiFiGuard                           92%    │ Header
├────────────────────────────────────────────┤
│                                            │
│          ▓▓ WiFiGuard ▓▓                   │ Size 2, white
│        WiFi Safety Checker                 │ Size 1, dim
│                                            │
│  ─── Last check ───                        │ Dim separator
│  8 networks nearby                         │ White
│  ● 5 safe  ● 2 caution  ● 1 avoid         │ Colored dots
│                                            │
├────────────────────────────────────────────┤
│ Hold L = Check WiFi          Hold R = Menu │ Footer
└────────────────────────────────────────────┘
```

**No previous scan variant:**

```
│                                            │
│          ▓▓ WiFiGuard ▓▓                   │
│        WiFi Safety Checker                 │
│                                            │
│     Hold the LEFT button to scan           │
│     nearby WiFi networks                   │
│                                            │
```

**Key changes from current:**
- "WiFi Safety Checker" replaces "WiFi Security Scanner" (friendlier)
- Last scan shows colored verdict counts, not raw open/encrypted counts
- "Check WiFi" replaces "Scan" in the footer (less technical)
- "Menu" replaces "Settings" (simpler word)

### 3.2 SCANNING

**Purpose:** Reassure user something is happening.

```
┌────────────────────────────────────────────┐
│ Checking WiFi                       92%    │
├────────────────────────────────────────────┤
│                                            │
│         Checking nearby WiFi               │ Size 2, green
│                                            │
│         Finding networks . . .             │ Size 1, dim, animated
│                                            │
│                                            │
│                                            │
├────────────────────────────────────────────┤
│                              Hold R = Stop │
└────────────────────────────────────────────┘
```

**Key changes:**
- "Checking WiFi" not "Scanning" — friendlier
- "Finding networks" not "Elapsed: 3s" — the timer creates anxiety
- No elapsed timer in Simple mode (Expert mode can show it)
- Animated dots: `. .` → `. . .` → `. . . .` cycling

### 3.3 NETWORK LIST — The Major Redesign

**Purpose:** Show all nearby networks ranked by safety and usefulness.
The user should instantly see which networks are safe and which to avoid.

#### Simple Mode Layout

```
┌────────────────────────────────────────────┐
│ Nearby WiFi (8)                     92%    │
├────────────────────────────────────────────┤
│ ● HomeNet_5G            ▓▓▓▓   SAFE     ▶ │ ← selected (bar)
│ ● CoffeeShop            ▓▓▓   CAUTION     │
│ ● Airport_Free          ▓▓    UNTESTED     │
│ ● Guest_Network         ▓▓▓   SAFE         │
│ ● FreePublicWifi        ▓     AVOID      ! │
│ ● (hidden)              ▓▓    CAUTION      │
│ ● Hotel_Lobby           ▓▓▓   LOGIN REQ    │
│                                            │
├────────────────────────────────────────────┤
│ 5 safe  2 caution  1 avoid                 │ Status bar
│ L = Next   Hold L = Details    R = Prev    │
└────────────────────────────────────────────┘
```

**Each list row (left to right):**

| Element | Width | Content | Color |
|---------|-------|---------|-------|
| Verdict dot | 8px | ● (filled circle) | Verdict color |
| SSID | ~120px | Network name, max 20 chars | White (red if evil twin) |
| Signal bars | ~30px | ▓▓▓░ (4-bar indicator) | Dim |
| Verdict word | ~70px | SAFE / CAUTION / AVOID / etc. | Verdict color |
| Alert badge | ~8px | ! (if evil twin or high risk) | Red |
| Selection | 3px | Cyan bar on left edge | Cyan |

**Signal bars mapping:**

| RSSI range | Bars | Label (expert) |
|------------|------|----------------|
| > -50 dBm  | ▓▓▓▓ | Excellent |
| -50 to -65 | ▓▓▓░ | Good |
| -65 to -75 | ▓▓░░ | Fair |
| -75 to -85 | ▓░░░ | Weak |
| < -85      | ░░░░ | Very weak |

#### Expert Mode List (additional columns)

```
│ ● HomeNet_5G    ▓▓▓▓ SAFE   WPA2 -42 c6  │
│ ● CoffeeShop   ▓▓▓  CAUT   Open -58 c1 N │
│ ● FreeWifi      ▓    AVOID  Open -82 c6 ! │
```

Expert mode adds: auth type, RSSI number, channel, New/Gone badges.

#### Sort Order: "Usefulness Score"

Networks are sorted by a composite score optimized for user decision-making:

```
usefulnessScore = safetyScore + signalScore + connectivityBonus

safetyScore:
  SAFE        = 100
  CAUTION     =  60
  UNTESTED    =  50
  LOGIN REQ   =  40
  NO INTERNET =  20
  AVOID       =   0

signalScore:
  (rssi + 100) clamped to 0..50    // -50dBm → 50, -100dBm → 0

connectivityBonus:
  Tested & working       = +20
  Tested & login needed  = +10
  Untested               =  +0
  Tested & failed        = -10
```

**Result:** Safe, strong, working networks appear first. Risky, weak networks sink
to the bottom. This is dramatically more useful than sorting by raw RSSI.

### 3.4 NETWORK DETAIL — Verdict-First Layout

**Purpose:** Answer "should I connect to this?" in under 2 seconds.

#### Simple Mode

```
┌────────────────────────────────────────────┐
│ Network Detail                      92%    │
├──────────┬─────────────────────────────────┤
│          │ HomeNet_5G                      │
│   12     │                                │
│  SAFE    │ Password-protected network      │ One-sentence explanation
│          │ with strong encryption.          │
│ [==    ] │                                │
│          │ ✓ Safe for everyday use          │ Recommended action (green)
│  ▓▓▓▓   │                                │
│ Strong   │                                │
│          │                                │
├──────────┴─────────────────────────────────┤
│ Hold L = Signal test             R = Back  │
└────────────────────────────────────────────┘
```

**Left panel (70px):**
- Risk score: Size 3, verdict color
- Verdict word: Size 1, verdict color
- Score gauge bar: 60px wide
- Signal bars: 4-bar indicator
- Signal word: "Strong" / "OK" / "Weak"

**Right panel (170px):**
- SSID: Size 1, white
- One-sentence explanation: Size 1, dim — plain English
- Recommended action: Size 1, verdict color, prefixed with ✓ or ✗ or ⚠

#### Simple Mode — Open Network (Untested)

```
│          │ Airport_Free                    │
│   65     │                                │
│ UNTESTED │ Open network — no password.     │
│          │ Anyone can see your traffic.     │
│ [=====]  │                                │
│          │ ⚠ Test before connecting         │
│  ▓▓░░   │                                │
│  Fair    │ Tap L to check this network     │
│          │                                │
│ L = Test connection    Hold L = Signal     │
```

#### Simple Mode — Open Network (Tested, Working)

```
│          │ CoffeeShop                      │
│   45     │                                │
│ CAUTION  │ Open network — working but      │
│          │ not encrypted. Use VPN if        │
│ [===   ] │ entering passwords.             │
│          │                                │
│  ▓▓▓░   │ ⚠ Use with VPN for safety       │
│  Good    │                                │
│          │                                │
```

#### Simple Mode — Portal Detected

```
│          │ Hotel_Lobby                      │
│   55     │                                │
│LOGIN REQ │ This network requires you to    │
│          │ log in through a web page.       │
│ [====  ] │                                │
│          │ ⚠ Open browser to log in         │
│  ▓▓▓░   │                                │
│  Good    │ Scan QR on phone to open login  │ If QR available
│  [QR]    │                                │
```

#### Simple Mode — Evil Twin Warning

```
│          │ FreePublicWifi                   │
│   91     │                                │
│  AVOID   │ DANGER: Fake network! Another   │
│          │ network has the same name but    │
│ [======] │ different security. This could   │
│          │ be an attacker.                  │
│  ▓░░░   │                                │
│  Weak    │ ✗ Do NOT connect                 │
│          │                                │
```

#### Expert Mode — Additional Data Below

In expert mode, append below the recommendation:

```
│          │ ── Technical ──                  │
│          │ WPA2-PSK  ch6  -42 dBm  +3dB    │
│          │ BSSID: AA:BB:CC:DD:EE:FF        │
│          │ Vendor: TP-Link                  │
│          │ Reasons: Open, Weak signal       │
│          │ Grade: Fast  Portal: None        │
│          │ Latency: 45ms  Jitter: 12ms      │
```

### 3.5 TESTING — Simplified Progress

**Purpose:** Show something is happening without technical phases.

#### Simple Mode

```
┌────────────────────────────────────────────┐
│ Testing Network                     92%    │
├────────────────────────────────────────────┤
│                                            │
│  Testing: CoffeeShop                       │
│                                            │
│  Connecting . . .                          │ Animated, green
│                                            │
│  ████████░░░░░░░░░░░░                      │ Simple progress bar
│                                            │
│  This takes a few seconds                  │ Dim, reassuring
│                                            │
├────────────────────────────────────────────┤
│ Please wait...                             │
└────────────────────────────────────────────┘
```

Progress bar fills as phases complete (25% per phase).
Phase text cycles: "Connecting..." → "Checking internet..." → "Almost done..."

#### Expert Mode — Phase Detail

```
│  [1/4] Joining WiFi        ✓               │
│  [2/4] DNS resolution      ✓               │
│  [3/4] HTTP connectivity   ...             │
│  [4/4] Speed benchmark     —               │
```

### 3.6 TEST RESULT — Clear Verdict

**Purpose:** Tell the user the test outcome in one glance.

```
┌────────────────────────────────────────────┐
│ Test Result                         92%    │
├────────────────────────────────────────────┤
│                                            │
│         ✓ Ready to use                     │ Size 2, GREEN
│                                            │
│   CoffeeShop                               │
│   Internet is working                      │ Size 1
│   Speed: Good (45ms)                       │
│                                            │
│   ⚠ This is an open network.               │ Orange warning
│     Use VPN for sensitive browsing.         │
│                                            │
├────────────────────────────────────────────┤
│ L = Back to detail             R = List    │
└────────────────────────────────────────────┘
```

**Other test result variants:**

| Result | Size-2 text | Color | Subtitle |
|--------|------------|-------|----------|
| Ready to use | ✓ Ready to use | Green | "Internet is working" |
| Login required | ⚠ Login required | Cyan | "Open browser to log in" + optional QR |
| No internet | ✗ No internet | Orange | "Connected but no internet access" |
| Slow connection | ⚠ Slow connection | Orange | "Internet works but is slow" |
| Could not connect | ✗ Could not connect | Red | "Failed to join this network" |

### 3.7 ENVIRONMENT SUMMARY — Actionable, Not Descriptive

#### Page 1: "Area Safety"

```
┌────────────────────────────────────────────┐
│ Area Safety                         92%    │
├────────────────────────────────────────────┤
│                                            │
│  WiFi Safety:  ● GOOD                      │ Green/Orange/Red
│  8 networks nearby                         │
│                                            │
│  ● 5 safe   ● 2 caution   ● 1 avoid       │ Colored
│                                            │
│  Best choice: HomeNet_5G                   │ Green, actionable
│  Watch out for: FreeWifi (possible fake)   │ Red, actionable
│                                            │
│  Tip: Avoid open networks without VPN      │ Dim, educational
│                                            │
├────────────────────────────────────────────┤
│ L = Session stats                R = Back  │
└────────────────────────────────────────────┘
```

**"WiFi Safety" rating:**

| Condition | Rating |
|-----------|--------|
| No high-risk networks, mostly encrypted | GOOD (green) |
| Some open or moderate-risk networks | FAIR (orange) |
| Evil twins, many open, high avg risk | POOR (red) |

#### Page 2: "Session Stats"

```
┌────────────────────────────────────────────┐
│ Session Stats                       92%    │
├────────────────────────────────────────────┤
│                                            │
│  Checks performed: 5                       │
│  Networks seen: 23 total                   │
│                                            │
│  Average safety: ● GOOD                    │ Colored
│  Login pages found: 1                      │
│  Suspicious networks: 3                    │
│                                            │
│  Tip: Re-check periodically — networks     │
│  can change.                               │
│                                            │
├────────────────────────────────────────────┤
│ L = Area safety                  R = Back  │
└────────────────────────────────────────────┘
```

**Key changes from current:**
- "Checks" not "scans"
- "Safety" not "risk score"
- Named "suspicious" not "duplicate SSIDs seen"
- Tip at bottom for education
- Expert mode adds: channel analysis, congestion score, avg RSSI, most common channel

### 3.8 SETTINGS — Simplified for Simple Mode

#### Simple Mode Settings

```
┌────────────────────────────────────────────┐
│ Menu                                92%    │
├────────────────────────────────────────────┤
│                                            │
│  Mode              Simple                  │ ← selected
│  Brightness        ████░░                  │
│  Auto-check        OFF                     │
│  Sleep after       1 min                   │
│  Alerts            ON                      │
│                                            │
│  ── About ──                               │
│  WiFiGuard v1.0                            │
│  Free heap: 142KB                          │
│                                            │
├────────────────────────────────────────────┤
│ L = Next    R = Change    Hold R = Done    │
└────────────────────────────────────────────┘
```

5 options. That's it.

#### Expert Mode Settings (all options)

```
│  ── Display ──                             │
│  Mode              Expert                  │
│  Sort              Signal                  │
│  Filter            None                    │
│  Brightness        ████░░                  │
│  ── Behavior ──                            │
│  Auto-monitor      ON  (60s)               │
│  Alert on risk     ON                      │
│  Low power         OFF                     │
│  Sleep after       1 min                   │
│  ── Data ──                                │
│  Export mode       Full CSV                │
│  Show BSSID        ON                      │
│  Privacy mode      OFF                     │
│  Diagnostics       OFF                     │
│  Demo mode         OFF                     │
```

### 3.9 STABILITY MONITOR

Simple mode changes are minor here — it's already a reasonably visual screen.

```
┌────────────────────────────────────────────┐
│ Signal Monitor                      92%    │
├────────────────────────────────────────────┤
│ HomeNet_5G                   5 of 10       │
│                                            │
│ Signal: Strong (-42 dBm)                   │ Expert: show dBm
│ Signal: Strong                             │ Simple: just word
│                                            │
│  █  ██ ███  █ ██                           │ Bar graph
│  ██ ██ ███  █ ██                           │
│  ██ ██████ ██ ██                           │ Color-coded bars
│  ████████████ ██                           │
│                                            │
│  Connection: STABLE                        │ Green
│                                            │
├────────────────────────────────────────────┤
│                        R = Stop and back   │
└────────────────────────────────────────────┘
```

**Verdict words:**

| Spread | Word | Color | Simple explanation |
|--------|------|-------|-------------------|
| ≤ 6 dB | STABLE | Green | "Signal is steady" |
| 7–15 dB | VARIABLE | Orange | "Signal fluctuates" |
| > 15 dB | UNSTABLE | Red | "Signal is unreliable" |

### 3.10 EXPORT

```
┌────────────────────────────────────────────┐
│ Export Data                          92%    │
├────────────────────────────────────────────┤
│                                            │
│  Data sent to USB                          │
│                                            │
│  Format: Summary report                    │ or "Full CSV"
│  Networks: 8                               │
│  Privacy: BSSID hidden                     │ or "BSSID visible"
│                                            │
│  Connect USB cable to computer             │
│  and open serial monitor to view.          │
│                                            │
├────────────────────────────────────────────┤
│ L = Done                                   │
└────────────────────────────────────────────┘
```

### 3.11 SLEEP

Black screen, backlight off. Any button press wakes to Idle.

### 3.12 DEBUG (unchanged from current, accessed via chord from Settings)

Expert-only screen. Keep current layout.

### 3.13 ALERTS & TOASTS

#### Alert Banner (top of list screen)

| Trigger | Text | Color |
|---------|------|-------|
| High-risk open network | ⚠ Risky open network nearby | Red |
| Evil twin detected | ⚠ Fake network detected! | Red |
| Many new APs appeared | ⚠ WiFi environment changed | Orange |
| Low battery | ⚠ Battery low — some tests skipped | Orange |

Dismissible with R tap. Alert H = 11px, reduces list space by one row.

#### Toast (above footer, any screen)

Auto-dismiss after 3 seconds. Orange bar with black text.

| Trigger | Text |
|---------|------|
| Stability target lost | "Network not found" |
| Test skipped (battery) | "Low battery — test skipped" |
| Privacy mode activated | "Privacy mode ON" |

---

## 4. Text/Copy for Every Verdict and State

### 4.1 Verdict Explanations (one-sentence, per network type)

**SAFE — Encrypted, no flags:**
```
explanation: "Password-protected network with strong encryption."
action:      "✓ Safe for everyday use"
```

**SAFE — Encrypted, WPA3:**
```
explanation: "Latest security standard. Very well protected."
action:      "✓ Safe for everyday use"
```

**CAUTION — Encrypted but WEP/WPA:**
```
explanation: "Outdated security — password can be cracked."
action:      "⚠ Avoid for sensitive activity"
```

**CAUTION — Encrypted, weak signal:**
```
explanation: "Protected but signal is weak. May disconnect."
action:      "⚠ Move closer for better connection"
```

**CAUTION — Open, tested working:**
```
explanation: "Open network — no password. Anyone nearby can see your traffic."
action:      "⚠ Use VPN if entering passwords"
```

**CAUTION — Duplicate SSID:**
```
explanation: "Multiple networks share this name. Could be normal or suspicious."
action:      "⚠ Verify with staff before connecting"
```

**AVOID — Evil twin suspect:**
```
explanation: "Same name as another network but different security. Likely fake."
action:      "✗ Do NOT connect"
```

**AVOID — High risk open + weak:**
```
explanation: "Open, weak signal, and suspicious name."
action:      "✗ Do NOT connect"
```

**LOGIN REQ — Portal detected:**
```
explanation: "This network requires login through a web page."
action:      "⚠ Open browser to log in, then re-test"
```

**NO INTERNET — Tested offline:**
```
explanation: "Connected but no internet access."
action:      "✗ Not usable for browsing"
```

**UNTESTED — Open, not tested:**
```
explanation: "Open network — not yet tested."
action:      "⚠ Tap L to check if it works"
```

**PROTECTED — Encrypted, can't test:**
```
explanation: "Password-protected. Cannot test without credentials."
action:      "✓ Generally safe if you trust the owner"
```

### 4.2 Test Result Copy

| Outcome | Headline (size 2) | Subtitle |
|---------|-------------------|----------|
| Fast + no portal | ✓ Ready to use | Internet working, good speed |
| Normal + no portal | ✓ Ready to use | Internet working |
| Slow | ⚠ Slow connection | Internet working but slow |
| Portal redirect | ⚠ Login required | Open browser to log in |
| Portal intercept | ⚠ Login required | Traffic is being redirected |
| No internet | ✗ No internet | Connected but no internet access |
| Failed to connect | ✗ Connection failed | Could not join this network |

### 4.3 Environment Copy

| Condition | "WiFi Safety" label |
|-----------|-------------------|
| Avg risk < 25, no evil twins | ● GOOD — "This area looks safe" |
| Avg risk 25–50 or some open | ● FAIR — "Some open networks nearby. Be selective." |
| Avg risk > 50 or evil twins | ● POOR — "Risky networks detected. Be careful." |

### 4.4 Stability Copy

| Spread | Verdict | Explanation |
|--------|---------|-------------|
| ≤ 6 dB | STABLE | "Signal is steady — good for video calls" |
| 7–15 dB | VARIABLE | "Signal fluctuates — may drop occasionally" |
| > 15 dB | UNSTABLE | "Signal is unreliable — expect disconnections" |

---

## 5. Captive Portal UX Improvements

### 5.1 Current State

The firmware already:
- Detects HTTP 301/302 redirects (portal detection)
- Tests a fallback URL
- Reports `PORTAL_REDIRECT_LOGIN` or `PORTAL_INTERCEPT`

### 5.2 Proposed Improvements

#### A. Capture redirect URL

When HTTP GET returns 301/302, the `Location` header contains the portal URL.
Store this in the connectivity result.

```cpp
struct ConnectivityResult {
  // ... existing fields ...
  char portalUrl[128];    // NEW: captured redirect URL
};
```

In `ConnectivityTest::update()`, phase 3:
```cpp
if (result_.redirect) {
  String location = http.header("Location");
  strncpy(result_.portalUrl, location.c_str(), 127);
  result_.portalUrl[127] = '\0';
}
```

#### B. Display portal information

On the test result screen, when LOGIN REQ:

```
│                                            │
│       ⚠ Login required                     │  Size 2, cyan
│                                            │
│  CoffeeShop                                │
│  This network has a login page.            │
│  Open your phone browser to log in.        │
│                                            │
│  Portal: captive.example.com               │  Dim, truncated URL
│                                            │
│  ┌──────────┐                              │
│  │  QR CODE │  Scan to open login page     │
│  │          │                              │
│  └──────────┘                              │
│                                            │
```

#### C. QR Code Display

Use a compact QR library (e.g., `qrcode` Arduino library by Richard Moore).
QR Version 6 encodes up to 134 chars, produces a 41×41 module grid.
At 2px per module: 82×82 pixels — fits in the 110px content height.

When portal URL is available and user is on detail/test-result screen,
render QR code in the left panel or inline.

**Important constraints:**
- Only display the URL — never auto-open or bypass login
- Do not store or transmit portal credentials
- Do not automate terms-of-service acceptance
- QR is a convenience feature: "scan this on your phone to open the login page"

#### D. Portal Result Categories

Expand the portal detection to provide more useful verdicts:

| Detection | User-facing text |
|-----------|-----------------|
| 204 / 200 success | "No login needed" |
| 301/302 to login page | "Login page required — scan QR to open" |
| 302 to different domain | "Traffic redirected — possible interception" |
| Connected, DNS fails | "No internet access" |
| Connected, HTTP fails | "Internet blocked" |

---

## 6. Recommended Feature Upgrades (Priority Ranked)

### Tier 1 — High Impact, Low Effort

| # | Feature | Effort | Value | Notes |
|---|---------|--------|-------|-------|
| 1 | **Safety verdict system** | Medium | Critical | Core UX improvement — enum + text lookup |
| 2 | **Signal bars** (replace dBm) | Trivial | High | 5 lines of drawing code |
| 3 | **Plain-English explanations** | Low | High | String table, no logic change |
| 4 | **Usefulness sort** | Low | High | Composite score function |
| 5 | **Simple/Expert mode toggle** | Low | High | Bool flag gates UI sections |
| 6 | **Capture portal redirect URL** | Low | High | Read Location header |

### Tier 2 — Medium Impact, Medium Effort

| # | Feature | Effort | Value | Notes |
|---|---------|--------|-------|-------|
| 7 | **QR code for portal URL** | Medium | High | Requires qrcode library (~2KB flash) |
| 8 | **Deauth frame detection** | Medium | High | ESP32 promiscuous mode; detect attack |
| 9 | **WiFi environment change alerts** | Low | Medium | Compare consecutive scans, alert on delta |
| 10 | **Test result screen** (separate from detail) | Medium | Medium | New view state |
| 11 | **Simplified settings** (mode-dependent) | Low | Medium | Gate settings list by mode bool |

### Tier 3 — High Impact, High Effort

| # | Feature | Effort | Value | Notes |
|---|---------|--------|-------|-------|
| 12 | **Local web dashboard** | High | High | ESP32 AP + HTTP server; show results on phone |
| 13 | **BLE companion** | High | Medium | GATT service for phone app |
| 14 | **Saved location profiles** | Medium | Medium | NVS store of known-safe BSSIDs |
| 15 | **Deauth attack alert** | Medium | High | If promiscuous mode; flash red + buzzer |

### Tier 4 — Nice to Have

| # | Feature | Effort | Value | Notes |
|---|---------|--------|-------|-------|
| 16 | **Boot animation** | Low | Low | Brief splash screen |
| 17 | **Haptic/buzzer alert** | Low | Low | If hardware supports it |
| 18 | **Multi-language** | Medium | Medium | String table indirection |
| 19 | **OTA firmware update** | Medium | Medium | Via AP mode web interface |

---

## 7. Implementation Plan

### Phase 1: Core Verdict System (foundation for everything else)

1. Add `SafetyVerdict` enum and `VerdictInfo` struct to `Types.h`
2. Add `VerdictEngine` module — computes verdict from `NetworkRecord`
3. Add verdict string tables (explanations, actions)
4. Update `UI::drawDetail()` to show verdict-first layout
5. Update `UI::drawList()` to show verdict + signal bars
6. Add `expertMode` gating to all draw functions

### Phase 2: Testing & Portal UX

7. Add `portalUrl[128]` to `ConnectivityResult`
8. Capture `Location` header in `ConnectivityTest::update()`
9. Add `VIEW_TEST_RESULT` sub-view
10. Implement test result screen with verdict headlines
11. Add QR code library, implement portal QR rendering

### Phase 3: Environment & Summary Redesign

12. Add area safety rating computation
13. Redesign `drawEnvSummary()` with actionable text
14. Redesign `drawIdle()` with verdict counts
15. Update session stats with plain-English labels

### Phase 4: Settings & Polish

16. Implement dual settings layout (simple vs. expert)
17. Rename all footer text to plain English
18. Add educational tips to environment/session screens
19. Polish transitions, toast text, alert banners

### Phase 5: Advanced Features

20. Implement deauth detection (promiscuous mode)
21. Implement local web dashboard (AP + HTTP)
22. Implement saved location profiles

---

## 8. Data Structures, Enums, State Machine

### 8.1 New Enums

```cpp
enum SafetyVerdict {
  VERDICT_SAFE,           // Encrypted, strong signal, no flags
  VERDICT_CAUTION,        // Moderate risk or concerns
  VERDICT_AVOID,          // High risk, do not connect
  VERDICT_LOGIN_REQ,      // Captive portal detected
  VERDICT_NO_INTERNET,    // Connected but offline
  VERDICT_SLOW,           // Working but slow
  VERDICT_UNTESTED,       // Open, not tested yet
  VERDICT_PROTECTED,      // Encrypted, cannot test
  VERDICT_COUNT
};

enum AreaSafety {
  AREA_GOOD,     // Low avg risk, mostly encrypted
  AREA_FAIR,     // Some open/moderate risk
  AREA_POOR      // Evil twins, many open, high risk
};

enum SignalLevel {
  SIGNAL_EXCELLENT,  // > -50
  SIGNAL_GOOD,       // -50 to -65
  SIGNAL_FAIR,       // -65 to -75
  SIGNAL_WEAK,       // -75 to -85
  SIGNAL_VERY_WEAK   // < -85
};
```

### 8.2 New Structs

```cpp
struct VerdictInfo {
  SafetyVerdict verdict;
  const char*   explanation;   // "Password-protected with strong encryption."
  const char*   action;        // "Safe for everyday use"
  uint16_t      color;         // display color for verdict
  char          symbol;        // '+' = checkmark, '!' = warning, 'x' = danger
};

struct AreaSafetyInfo {
  AreaSafety   rating;
  const char*  label;          // "GOOD" / "FAIR" / "POOR"
  const char*  tip;            // "This area looks safe"
  uint16_t     color;
};
```

### 8.3 Expanded ConnectivityResult

```cpp
struct ConnectivityResult {
  ConnectivityGrade grade;
  PortalResult      portal;
  int32_t           associationMs;
  int32_t           dnsMs;
  int32_t           httpMs;
  bool              dnsOk;
  bool              httpOk;
  int               httpCode;
  bool              redirect;
  int16_t           benchmarkAvgMs;
  int16_t           benchmarkJitterMs;
  uint8_t           benchmarkPings;
  char              portalUrl[128];   // NEW: captured redirect URL
};
```

### 8.4 Updated UIView

```cpp
enum UIView {
  VIEW_LIST,
  VIEW_DETAIL,
  VIEW_TEST_RESULT,     // NEW: dedicated test result screen
  VIEW_ENV_SUMMARY,
  VIEW_SESSION_STATS,   // NEW: separate from env (cleaner)
  VIEW_SETTINGS,
  VIEW_COUNT
};
```

### 8.5 State Machine — Minor Changes

The state machine itself is mostly fine. Suggested changes:

```cpp
enum DeviceState {
  STATE_IDLE,
  STATE_SCANNING,
  // STATE_PROCESSING removed — it's instantaneous, fold into scan-complete
  STATE_BROWSING,         // contains sub-views
  STATE_TESTING,
  STATE_EXPORT,
  STATE_SETTINGS,
  STATE_SLEEP,
  STATE_STABILITY_MONITOR,
  STATE_DEBUG
};
```

Remove `STATE_PROCESSING` — currently `onScanComplete()` transitions to
`STATE_PROCESSING`, then `onProcessingComplete()` immediately transitions to
`STATE_BROWSING`. This intermediate state serves no purpose; the transition
happens in the same loop iteration.

---

## 9. Code Architecture Recommendations

### 9.1 New Module: VerdictEngine

```cpp
// VerdictEngine.h
#ifndef VERDICTENGINE_H
#define VERDICTENGINE_H

#include "Types.h"

class VerdictEngine {
public:
  // Compute safety verdict for a single network
  static SafetyVerdict computeVerdict(const NetworkRecord& net);

  // Get display info for a verdict
  static VerdictInfo getVerdictInfo(SafetyVerdict v);

  // Get signal level from RSSI
  static SignalLevel getSignalLevel(int8_t rssi);

  // Compute area safety from scan
  static AreaSafetyInfo computeAreaSafety(const ScanRecord& scan);

  // Compute usefulness score for sorting
  static int16_t computeUsefulnessScore(const NetworkRecord& net);

  // Get signal bars string (e.g. "▓▓▓░")
  static const char* signalBarsStr(SignalLevel level);

  // Get signal word (e.g. "Strong")
  static const char* signalWordStr(SignalLevel level);
};

#endif
```

### 9.2 VerdictEngine Implementation Logic

```cpp
SafetyVerdict VerdictEngine::computeVerdict(const NetworkRecord& net) {
  // Tested networks — use test results
  if (net.tested) {
    if (net.grade == GRADE_PORTAL)
      return VERDICT_LOGIN_REQ;
    if (net.grade == GRADE_OFFLINE || net.grade == GRADE_FAILED)
      return VERDICT_NO_INTERNET;
    if (net.grade == GRADE_SLOW)
      return VERDICT_SLOW;
  }

  // Evil twin — always AVOID regardless of other factors
  if (net.possibleEvilTwin)
    return VERDICT_AVOID;

  // High risk — AVOID
  if (net.riskScore >= 70)
    return VERDICT_AVOID;

  // Open network, not tested
  if (net.auth == AUTH_OPEN || net.auth == AUTH_WEP) {
    if (!net.tested)
      return VERDICT_UNTESTED;
    // Tested and working but still open
    if (net.riskScore >= 40)
      return VERDICT_CAUTION;
    return VERDICT_CAUTION;  // open networks are never "SAFE"
  }

  // Encrypted but weak security (WPA1)
  if (net.auth == AUTH_WPA || net.auth == AUTH_WPA_WPA2) {
    if (net.riskScore >= 50) return VERDICT_CAUTION;
    return VERDICT_CAUTION;  // legacy encryption
  }

  // Encrypted with modern security
  // WPA2, WPA3, WPA2/WPA3
  if (net.riskScore >= 50)
    return VERDICT_CAUTION;
  if (net.riskScore >= 30)
    return VERDICT_SAFE;  // minor flags but fundamentally safe
  return VERDICT_SAFE;
}
```

### 9.3 Verdict String Tables (PROGMEM-friendly)

```cpp
struct VerdictText {
  const char* label;        // "SAFE", "CAUTION", "AVOID", etc.
  const char* symbol;       // "+" / "!" / "x"
};

static const VerdictText verdictLabels[] = {
  { "SAFE",       "+" },
  { "CAUTION",    "!" },
  { "AVOID",      "x" },
  { "LOGIN REQ",  "!" },
  { "NO INTERNET","x" },
  { "SLOW",       "!" },
  { "UNTESTED",   "?" },
  { "PROTECTED",  "+" },
};

// Explanation lookup — indexed by verdict + subcondition
struct VerdictExplanation {
  SafetyVerdict   verdict;
  uint16_t        conditionMask;   // bitmask of conditions for this variant
  const char*     explanation;
  const char*     action;
};

static const VerdictExplanation explanations[] = {
  { VERDICT_SAFE, 0,
    "Password-protected with strong encryption.",
    "Safe for everyday use" },
  { VERDICT_CAUTION, (1 << RISK_OPEN),
    "Open network. Anyone nearby can see traffic.",
    "Use VPN if entering passwords" },
  { VERDICT_CAUTION, (1 << RISK_WEAK_SIGNAL),
    "Protected but signal is weak. May disconnect.",
    "Move closer for better connection" },
  { VERDICT_CAUTION, (1 << RISK_WPA_LEGACY),
    "Outdated security. Password could be cracked.",
    "Avoid for sensitive activity" },
  { VERDICT_CAUTION, (1 << RISK_DUPLICATE_SSID),
    "Multiple networks share this name.",
    "Verify with staff before connecting" },
  { VERDICT_AVOID, (1 << RISK_EVIL_TWIN_SUSPECT),
    "Same name, different security. Likely fake.",
    "Do NOT connect" },
  { VERDICT_AVOID, 0,
    "Multiple security concerns detected.",
    "Do NOT connect" },
  { VERDICT_LOGIN_REQ, 0,
    "This network requires login through a web page.",
    "Open browser to log in" },
  { VERDICT_NO_INTERNET, 0,
    "Connected but no internet access.",
    "Not usable for browsing" },
  { VERDICT_SLOW, 0,
    "Internet works but connection is slow.",
    "Usable for basic browsing only" },
  { VERDICT_UNTESTED, 0,
    "Open network. Not yet tested.",
    "Tap L to check if it works" },
  { VERDICT_PROTECTED, 0,
    "Password-protected. Cannot test without credentials.",
    "Generally safe if you trust the owner" },
};
```

### 9.4 Usefulness Sort Function

```cpp
int16_t VerdictEngine::computeUsefulnessScore(const NetworkRecord& net) {
  int16_t score = 0;

  // Safety component (0–100)
  SafetyVerdict v = computeVerdict(net);
  switch (v) {
    case VERDICT_SAFE:       score += 100; break;
    case VERDICT_PROTECTED:  score +=  90; break;
    case VERDICT_CAUTION:    score +=  60; break;
    case VERDICT_SLOW:       score +=  50; break;
    case VERDICT_UNTESTED:   score +=  45; break;
    case VERDICT_LOGIN_REQ:  score +=  40; break;
    case VERDICT_NO_INTERNET:score +=  20; break;
    case VERDICT_AVOID:      score +=   0; break;
    default:                 score +=  30; break;
  }

  // Signal component (0–50)
  int16_t sigScore = (int16_t)(net.rssi + 100);
  if (sigScore < 0) sigScore = 0;
  if (sigScore > 50) sigScore = 50;
  score += sigScore;

  // Connectivity bonus
  if (net.tested) {
    if (net.grade == GRADE_FAST || net.grade == GRADE_NORMAL)
      score += 20;
    else if (net.grade == GRADE_PORTAL)
      score += 10;
    else if (net.grade == GRADE_FAILED || net.grade == GRADE_OFFLINE)
      score -= 10;
  }

  return score;
}
```

### 9.5 UI Draw Architecture

The `draw()` function should be refactored to support mode-dependent rendering:

```cpp
void UI::draw() {
  // ... existing throttle / dirty logic ...

  bool expert = settings.get().expertMode;

  if (st == STATE_IDLE)         drawIdle(expert);
  else if (st == STATE_SCANNING) drawScanning(expert);
  else if (st == STATE_BROWSING) {
    if (view_ == VIEW_LIST)         drawList(expert);
    else if (view_ == VIEW_DETAIL)  drawDetail(expert);
    else if (view_ == VIEW_TEST_RESULT) drawTestResult(expert);
    else if (view_ == VIEW_ENV_SUMMARY) drawEnvSummary(expert);
    else if (view_ == VIEW_SESSION_STATS) drawSessionStats(expert);
    else if (view_ == VIEW_SETTINGS) drawSettings(expert);
  }
  // ... etc ...
}
```

Each draw function receives the `expert` bool and conditionally renders
additional technical data when true.

### 9.6 List Item Rendering (Simple vs Expert)

```cpp
void UI::drawListItem(int x, int y, const NetworkRecord* n,
                      bool selected, bool expert) {
  SafetyVerdict v = VerdictEngine::computeVerdict(*n);
  VerdictInfo vi = VerdictEngine::getVerdictInfo(v);
  SignalLevel sig = VerdictEngine::getSignalLevel(n->rssi);

  // Selection bar
  if (selected)
    displayDriver.fillRect(0, y, 3, ITEM_H - 1, COL_INFO);

  // Verdict dot
  displayDriver.fillCircle(x + 4, y + ITEM_H/2, 3, vi.color);

  // SSID
  uint16_t ssidCol = n->possibleEvilTwin ? COL_DANGER : COL_FG;
  displayDriver.setTextColor(ssidCol, COL_BG);
  displayDriver.setCursor(x + 10, y + 4);
  // ... truncated SSID ...

  // Signal bars
  drawSignalBars(x + 130, y + 3, sig);

  // Verdict word
  displayDriver.setTextColor(vi.color, COL_BG);
  displayDriver.setCursor(x + 162, y + 4);
  displayDriver.print(vi.label);

  if (expert) {
    // Auth type, RSSI number, channel after verdict
    // ... compact expert suffix ...
  }
}
```

### 9.7 Signal Bars Drawing

```cpp
void UI::drawSignalBars(int x, int y, SignalLevel level) {
  int bars = 4 - (int)level;  // EXCELLENT=4, GOOD=3, FAIR=2, WEAK=1, VERY_WEAK=0
  int barW = 4, gap = 1, baseH = 3;
  for (int i = 0; i < 4; i++) {
    int barH = baseH + i * 2;  // 3, 5, 7, 9
    int bx = x + i * (barW + gap);
    int by = y + (9 - barH);   // bottom-aligned
    uint16_t col = (i < bars) ? COL_FG : COL_CHROME;
    displayDriver.fillRect(bx, by, barW, barH, col);
  }
}
```

### 9.8 QR Code Rendering

```cpp
// Requires: #include <qrcode.h>  (Arduino QR library)
void UI::drawQRCode(int x, int y, int size, const char* url) {
  if (!url || !url[0]) return;

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(6)];  // Version 6 = 41x41
  qrcode_initText(&qrcode, qrcodeData, 6, ECC_LOW, url);

  int moduleSize = size / qrcode.size;
  if (moduleSize < 1) moduleSize = 1;

  for (int my = 0; my < qrcode.size; my++) {
    for (int mx = 0; mx < qrcode.size; mx++) {
      uint16_t col = qrcode_getModule(&qrcode, mx, my) ? COL_FG : COL_BG;
      displayDriver.fillRect(
        x + mx * moduleSize, y + my * moduleSize,
        moduleSize, moduleSize, col);
    }
  }
}
```

At 2px per module: 82×82 pixels for a Version 6 QR code. This fits
within the 110px content area and leaves room for text beside it.

### 9.9 Deauth Detection (Promiscuous Mode)

```cpp
// New module: DeauthDetector.h
// ESP32 promiscuous mode can sniff management frames.
// Deauth frames (type 0, subtype 12) indicate active attack.

#include <esp_wifi.h>

static volatile uint16_t deauthCount = 0;

static void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t frameType = (pkt->payload[0] >> 2) & 0x03;
  uint8_t frameSubtype = (pkt->payload[0] >> 4) & 0x0F;
  if (frameType == 0 && (frameSubtype == 12 || frameSubtype == 10)) {
    deauthCount++;  // deauth or disassociation frame
  }
}

class DeauthDetector {
public:
  void begin() {
    // Enable promiscuous mode between scans
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(snifferCallback);
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filter);
  }
  uint16_t getAndResetCount() {
    uint16_t c = deauthCount;
    deauthCount = 0;
    return c;
  }
  void pause() { esp_wifi_set_promiscuous(false); }
  void resume() { esp_wifi_set_promiscuous(true); }
};
```

Pause promiscuous mode during active scans and connectivity tests.
If `deauthCount > threshold` between scans, show red alert:
"⚠ WiFi attack detected! Someone is disrupting connections nearby."

### 9.10 File Organization Summary

```
WiFiGuard/
├── WiFiGuard.ino          (main loop — minimal changes)
├── Config.h               (add new feature flags)
├── Types.h                (add SafetyVerdict, AreaSafety, SignalLevel, etc.)
├── VerdictEngine.h/.cpp   (NEW — verdict computation + string tables)
├── VerdictStrings.h       (NEW — all user-facing text in one place)
├── WiFiScanner.h/.cpp     (unchanged)
├── RiskEngine.h/.cpp      (unchanged — still computes raw scores)
├── EnvironmentAnalysis.h/.cpp (unchanged)
├── ScanHistory.h/.cpp     (unchanged)
├── ConnectivityTest.h/.cpp (add portalUrl capture)
├── StateMachine.h/.cpp    (remove STATE_PROCESSING)
├── DisplayDriver.h/.cpp   (add fillCircle, drawSignalBars helpers)
├── InputHandler.h/.cpp    (unchanged)
├── UI.h/.cpp              (major rewrite of draw functions)
├── Settings.h/.cpp        (unchanged)
├── PowerManager.h/.cpp    (unchanged)
├── Export.h/.cpp           (unchanged)
├── SessionStats.h/.cpp    (unchanged)
├── OuiLookup.h/.cpp       (unchanged)
├── QRRender.h/.cpp        (NEW — QR code rendering wrapper)
├── DeauthDetector.h/.cpp  (NEW — promiscuous mode deauth sniffer)
└── docs/
    └── UX_REDESIGN_V2.md  (this document)
```

### 9.11 Memory Budget

| Component | RAM | Flash | Notes |
|-----------|-----|-------|-------|
| Verdict string tables | ~0 (PROGMEM) | ~1.5 KB | Stored in flash |
| `portalUrl[128]` in ConnectivityResult | 128 B | 0 | One instance |
| QR code buffer (Version 6) | ~180 B | ~2 KB lib | Stack-allocated during render |
| Deauth sniffer callback | ~4 B (counter) | ~0.5 KB | ISR + filter setup |
| VerdictEngine module | ~0 | ~1.5 KB | Stateless functions |
| **Total new** | **~312 B** | **~5.5 KB** | Well within ESP32 limits |

ESP32 has ~320 KB RAM and 4 MB flash. This budget is negligible.

---

## 10. Migration Path — What Changes, What Stays

| Module | Change level | What happens |
|--------|-------------|--------------|
| RiskEngine | **None** | Still computes 0–100 scores. VerdictEngine wraps it. |
| WiFiScanner | **None** | Scan pipeline unchanged. |
| ScanHistory | **None** | Storage unchanged. |
| EnvironmentAnalysis | **None** | Computation unchanged. UI interprets differently. |
| ConnectivityTest | **Small** | Add `portalUrl` capture (5 lines). |
| StateMachine | **Small** | Remove STATE_PROCESSING. |
| Types.h | **Small** | Add new enums and structs. |
| Settings | **Small** | No structural change, UI presents differently. |
| UI | **Large** | All draw functions rewritten for verdict-first. |
| InputHandler | **None** | Button handling unchanged. |
| WiFiGuard.ino | **Small** | Remove processing-state transition, add deauth check. |

The risk engine and scanner are decoupled from the UI. The verdict system
is a pure presentation layer on top of existing data — no backend changes
needed for the core redesign.
