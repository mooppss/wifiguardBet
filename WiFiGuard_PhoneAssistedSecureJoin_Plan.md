## 1. Executive summary

**WiFiGuard’s new feature** will let a user securely provide WiFi credentials from their phone, have WiFiGuard join an authorized password‑protected network, and then run **defensive, post‑connection safety checks**. The feature is strictly limited to user‑authorized networks and user‑supplied credentials, and focuses on **verifying safety and quality**, not on obtaining or abusing access.

**Recommended MVP architecture**: WiFiGuard starts a temporary local SoftAP, serves a minimal config page where the user enters SSID + password, then joins the target network, runs connectivity and anomaly checks, presents a simple verdict on the device, and wipes credentials unless the user explicitly opts in to save a non‑password “trusted profile”.

**V2+ direction**: Add **BLE credential handoff** (for native or PWA apps) and an optional USB/serial dev‑only path, improve stability/latency metrics, and strengthen privacy and anti‑misuse controls. Throughout, WiFiGuard remains a **defensive, user‑consent‑based verifier**, not a penetration tool.

---

## 2. Safety boundaries and non-goals

**Allowed / in-scope**

- **User‑authorized networks only**:
  - User explicitly states they have the right to join (e.g., cafe guest WiFi with posted password).
  - UI must reinforce “You must be authorized to use this network.”
- **User‑supplied credentials only**:
  - SSID + password (or auth token) are provided by the user via phone or device.
  - No auto‑discovery or guessing of passwords.
- **Defensive, post‑connection verification**:
  - Only after successful association and DHCP does WiFiGuard perform checks.
  - Checks are limited to **network quality, stability, and suspicious redirections/intercepts**.

**Strictly out of scope / non‑goals**

- **No password guessing / cracking**:
  - No brute force, dictionary attacks, or repeated join attempts with generated passwords.
  - No PMKID or handshake capture for offline cracking.
- **No active attacks or disruption**:
  - No deauthentication frames.
  - No attempts to bypass or tamper with captive portals.
  - No automatic submission of forms or terms pages.
- **No unauthorized reconnaissance**:
  - No active scanning of other clients, ports, or services on the LAN.
  - No attempts to evade ACLs or firewall rules.
- **No credential misuse or exfiltration**:
  - No silent transmission of credentials to external servers or phones.
  - No default long‑term storage of passwords.
  - No logging of raw passwords to flash, serial console, or debug output.
- **No abuse as a “WiFi hacking” gadget**:
  - Documentation and UX clearly frame the feature as **risk assessment for networks you’re already allowed to join**, not as a way to gain access.

---

## 3. Recommended architecture

**Overall shape**

- **MVP architecture**: **SoftAP + local browser config page**
  - WiFiGuard starts a short‑lived WiFi SoftAP (e.g., `WiFiGuard-Setup-XXXX`).
  - Device hosts a small web server:
    - Landing page with explanation and consent.
    - Form inputs: SSID (text or from scan list), password, optional notes (e.g., “Cafe A, 2nd floor”).
    - Clear statements: “Use only networks you are allowed to join. WiFiGuard will not try to bypass any restrictions.”
  - On submit, credentials are passed directly to WiFiGuard’s in‑RAM structures, not logged or persisted by default.
  - SoftAP shuts down once:
    - The target connection attempt is complete; or
    - A short timeout elapses without submission.

**High‑level data flow**

1. User starts Phone‑Assisted Secure Join on device.
2. Device:
   - Suspends normal scanning/monitoring.
   - Starts temporary SoftAP + web server in “config mode”.
3. Phone:
   - Connects to SoftAP, opens `http://192.168.4.1/` (or an mDNS name if supported).
4. User:
   - Reads safety statement.
   - Enters SSID and password for an authorized network.
   - Submits form.
5. WiFiGuard:
   - Receives credentials into volatile memory.
   - Stops SoftAP.
   - Joins target network.
   - Runs post‑connection tests.
6. Device:
   - Shows progress and final verdict on the 240x135 display.
   - Explicit screen to clear credentials (default) or save profile (without password by default).

**Why this is recommended for MVP**

- **Browser‑only workflow**: Works with any modern smartphone without requiring apps or BLE permissions.
- **ESP32 proven pattern**: SoftAP config portals are common and well‑documented.
- **Tight control**: Device remains the only holder of SSID/password; no cloud or external relay.
- **Clear security boundaries**: Local link only, short lifetime, session‑only password handling.

---

## 4. Alternative architecture comparison

### 4.1 SoftAP + captive config webpage (recommended MVP)

- **Security**
  - Local only; no internet dependency.
  - Can run over HTTP with local‑only risk (password visible to anyone on that SoftAP).
  - Risk mitigations:
    - Short SoftAP lifetime.
    - Unique SSID (non‑identifying) and optional small random suffix.
    - Only accept a **single client** at a time if feasible.
  - TLS is difficult (cert management), so likely **HTTP only**, but acceptable given strictly local and ephemeral nature with clear warnings.
- **User experience**
  - Familiar captive portal style config.
  - Works with any phone browser; no install.
  - Simple: connect, open page, type, done.
- **Implementation difficulty**
  - Low–medium: ESP32 libraries exist (WiFiManager‑like patterns).
  - Need a small HTML/JS UI, request handlers, and state machine.
- **Reliability**
  - Very reliable on ESP32 (SoftAP + web server is mature).
  - Must manage RAM carefully for web server and scan results.
- **Native app required?**
  - **No**, browser‑only.
- **Plain browser flow possible?**
  - **Yes**; this is the core design.

### 4.2 BLE credential handoff from phone (strong V2 candidate)

- **Security**
  - BLE pairing can provide some confidentiality vs. open HTTP.
  - Less visible to nearby observers than an open AP, but BLE sniffing is still possible without pairing/bonding if misconfigured.
  - Need explicit bonding/pairing or out‑of‑band consent (QR code, pairing confirmation).
- **User experience**
  - Requires either:
    - A **companion app** (native or PWA with BLE support), or
    - A phone BLE debugging app (not acceptable for normal users).
  - UX can be slick:
    - App lists nearby WiFiGuard devices.
    - User selects device, enters SSID + password, taps “Send”.
  - But raises install friction and permission prompts (Bluetooth, Location).
- **Implementation difficulty**
  - Medium–high:
    - Define BLE GATT service and characteristics for:
      - Device discovery and naming.
      - Credential write (avoiding fragmentation issues).
      - Status updates and verdict streaming.
    - Handle BLE security (bonding, whitelisting).
  - Phone side: app implementation is significant work.
- **Reliability**
  - BLE on ESP32 can be finicky under heavy WiFi use; need careful coexistence and testing.
  - Connection drops and fragmentation must be handled robustly.
- **Native app required?**
  - **Effectively yes** for a good UX.
- **Plain browser flow possible?**
  - Limited: some browsers support Web Bluetooth, but support is uneven and UX is less discoverable.

**Recommended role**: **V2** enhancement once MVP is stable, especially if targeting power users who will install a dedicated app.

### 4.3 USB serial / dev‑only fallback

- **Security**
  - Physically tethered; strong protection against remote interception.
  - Exposes credentials to the host device (e.g., laptop) rather than over air.
- **User experience**
  - Poor for typical cafe/guest scenarios.
  - Requires cable, drivers, and some tool/UI on the host (CLI or small desktop app).
- **Implementation difficulty**
  - Low on ESP32 side (simple serial protocol).
  - Medium on host side (create a small cross‑platform utility or leave it as advanced/CLI).
- **Reliability**
  - High once connected; serial communication is simple.
- **Native app required?**
  - Some host‑side tool needed for non‑technical users.
- **Plain browser flow possible?**
  - Potentially via Web Serial (Chrome‑based), but very niche.

**Recommended role**: **Developer / lab‑only fallback** (e.g., QA rigs, automated verifications), not part of mainstream end‑user workflow.

### 4.4 Architecture recommendation summary

- **MVP**: **SoftAP + local config webpage**.
- **V2**: Add **BLE credential handoff** with a companion app or PWA. Keep USB/serial as a **developer‑only tool**, clearly separated from consumer UX.

---

## 5. Credential lifecycle design

### 5.1 Where credentials live temporarily

- **In‑RAM only**:
  - Credentials are held in:
    - A short‑lived in‑memory structure (e.g., `current_session_credentials`).
    - WiFi driver’s internal RAM while associating.
  - **No writes to flash/NVS** by default.
- **No logs**:
  - Never log SSID or password in full:
    - If logging is enabled for debugging, only log:
      - SSID in full or partially masked.
      - Password **never** or at most length only (e.g., “password length: 10”).
- **No serial/debug leak**:
  - Ensure monitor/debug functions do not print credential fields.
  - Add static checks or macros to avoid accidentally logging.

### 5.2 Persistence and clearing policy

- **Default policy: session‑only**
  - Credentials exist from:
    - Form submission / BLE receipt
    - Until:
      - Connection + tests complete, or
      - A hard timeout (e.g., 5–10 minutes), or
      - User aborts.
  - After any of these:
    - Overwrite password string in RAM (e.g., explicit zeroing).
    - Clear references to ensure it’s eligible for garbage or reuse.
- **On reboot / power loss**
  - Session credentials are inherently lost.
  - On boot, show a simple message:
    - “Last test interrupted. No credentials stored.”
    - Offer to start a new Phone‑Assisted Secure Join.

### 5.3 Avoid showing full password

- On device:
  - **Never show the full password** on the 240x135 screen.
  - Show masked version (e.g., `********`) and maybe a simple length indicator:
    - “Password length: 10 characters”.
- On phone:
  - Use standard password input that hides characters.
  - Optional “Show password” toggle on **phone side only**, under user’s local control.
- For confirmations:
  - Display SSID and high‑level description (e.g., “CafeGuest (WPA2)”), not the password.

### 5.4 “Save trusted network” and password storage

- **MVP stance**:
  - **Do not store passwords at all** in user‑accessible profiles.
  - Trusted profiles store **only metadata**, not PSKs.
- **If password storage is considered later**:
  - Must be a **separate, explicit opt‑in** with:
    - Clear warning: “WiFiGuard will remember this password. Anyone with this device can use it to join this network.”
    - Possibly gated behind a device‑level PIN, if such a concept exists.
  - Even then, passwords should be:
    - Encrypted at rest with a device‑unique key.
    - Not exportable.

### 5.5 Credential export / sharing

- **No export** path:
  - No UI to show, copy, or transmit stored PSKs.
  - Trusted profile data should only contain non‑secret descriptors.

---

## 6. Post-connection safety checks

All checks run **after** WiFiGuard successfully associates and obtains IP configuration. They must be **lightweight and non‑intrusive**, focusing on *WiFiGuard’s own connectivity*, not scanning other devices.

### 6.1 Basic connection checks

1. **Association success**
   - Confirm WiFi driver reports connected to target SSID/BSSID.
   - Record BSSID, RSSI, channel.
2. **DHCP success**
   - Obtain IPv4 address, subnet mask, gateway, DNS servers.
   - Fail with “COULD NOT VERIFY” / “NO INTERNET” if DHCP fails.
3. **Gateway presence**
   - Ping default gateway \(X times with small payload\).
   - Record:
     - Reachability result.
     - Average latency.
     - Packet loss.

### 6.2 DNS resolution

4. **DNS sanity check**
   - Attempt to resolve a set of known domains:
     - One or more reliable targets (e.g., `example.com`, one popular site, one public test domain).
   - Note:
     - Resolution success/failure.
     - Whether multiple domains resolve to same IP (possible interception, but also common for CDNs).

### 6.3 HTTP/HTTPS reachability

5. **HTTP tests**
   - Simple HTTP GET to:
     - `http://example.com` or another known URL with stable behavior.
   - Observe:
     - Status codes (3xx, 4xx, 5xx).
     - Presence of `Location` headers (for captive portals).
6. **HTTPS tests**
   - HTTPS GET to:
     - `https://example.com` or other trusted high‑uptime sites.
   - Record:
     - TLS handshake success/failure.
     - Certificate validation issues (if basic root store is available) or at least handshake failures.

### 6.4 Captive portal / redirect detection

7. **Captive portal detection**
   - Strategy:
     - Call a known URL expected to return **static, known content** (or at least pattern).
     - Compare:
       - If HTTP 200 with unexpected HTML/CSS/JS structure or a redirect to a login/terms page, treat as **captive portal**.
   - Extract:
     - Portal domain (e.g., `wifi.cafe-portal.com`).
     - Whether connection to other internet sites is similarly redirected.

8. **Portal domain analysis (defensive only)**
   - High‑level heuristics only:
     - Is portal domain a private IP or hostname vs. a public domain?
     - Does it use HTTPS?
   - **No form submission.**
   - **No attempts to bypass login**.

### 6.5 Latency and basic stability

9. **Latency check**
   - Ping a small set of well‑known public IPs/domains (e.g., one or two) with small packet counts and low frequency.
   - Categorize:
     - Low latency (e.g., \<80 ms).
     - Medium (80–200 ms).
     - High (\>200 ms).
10. **Optional jitter / short stability sampling**
   - Over a short window (e.g., 10–20 seconds):
     - Repeat a small set of pings.
     - Compute basic jitter or % packet loss.
   - Used to classify as **STABLE / SPOTTY**.

### 6.6 No internet / suspicious redirect detection

11. **No internet detection**
   - If:
     - DNS fails for all tested domains, OR
     - All HTTP(S) requests fail or get captive portal‑like responses,
   - Flag as **NO INTERNET** or **LOGIN REQUIRED** (if portal detected).

12. **Suspicious redirect/intercept detection**
   - Heuristics (defensive, conservative):
     - HTTPS test failures + HTTP test redirect to unknown/out‑of‑pattern domains.
     - All popular targets resolving to the **same unusual IP** that is not common CDN/public IP (lightweight, with caveats).
   - Mark as **CAUTION** or **SUSPICIOUS BEHAVIOR** only when signals are strong, and messaging remains cautious (“may be misconfigured or intercepting traffic”).

### 6.7 Optional gateway/DNS sanity checks

13. **Gateway plausibility**
   - Check:
     - Gateway is inside local subnet.
     - Not equal to 0.0.0.0.
   - Note anomalies as “unusual configuration”, not as definitive attack.

14. **DNS plausibility**
   - Check DNS IPs:
     - Are they common public resolvers (e.g., well‑known ranges) or random private IPs?
   - Used for nuance in verdict/explanation, not strong blocking behavior.

---

## 7. Verdict model

User‑facing verdicts should be **simple** and mapped to clear technical triggers and guidance.

### 7.1 Verdicts, triggers, explanations, recommendations

- **READY TO USE**
  - **Triggers**:
    - Associated successfully, DHCP OK.
    - Gateway reachable with low/medium latency and low loss.
    - DNS resolution OK for multiple domains.
    - HTTP/HTTPS tests succeed without captive portal behavior.
  - **Explanation**:
    - “Connected and internet access looks normal.”
  - **Recommendation**:
    - “You can safely use this network. Continue to watch for unusual login prompts or warnings on your own device.”

- **LOGIN REQUIRED**
  - **Triggers**:
    - Association and DHCP succeed.
    - Captive portal behavior detected (consistent redirect or portal page).
    - Internet access blocked until login.
  - **Explanation**:
    - “This network requires you to log in or accept terms on a captive portal page.”
  - **Recommendation**:
    - “Use your phone or laptop to open a browser and follow the portal’s instructions. Verify that the portal address and content look legitimate.”

- **NO INTERNET**
  - **Triggers**:
    - Connected and DHCP OK, but:
      - DNS consistently fails, OR
      - All HTTP/HTTPS tests fail (no response, timeouts).
    - No clear captive portal signals.
  - **Explanation**:
    - “This network is connected, but it does not currently provide internet access.”
  - **Recommendation**:
    - “Ask the network owner or try again later. Avoid relying on this network for important tasks.”

- **SLOW CONNECTION**
  - **Triggers**:
    - Internet reachable but:
      - High latency and/or high packet loss over short sampling.
  - **Explanation**:
    - “Internet access works, but the connection is slow or unstable.”
  - **Recommendation**:
    - “Expect delays and possible dropouts. For sensitive tasks, consider another network or mobile data.”

- **CAUTION**
  - **Triggers** (soft anomalies):
    - Unusual DNS or gateway configuration (e.g., very odd IPs).
    - Some HTTPS failures while HTTP works, without clear portal.
    - Repeated minor anomalies that don’t meet “SUSPICIOUS” threshold.
  - **Explanation**:
    - “This network is working, but its configuration looks unusual or inconsistent.”
  - **Recommendation**:
    - “Use with caution. Avoid sensitive logins (e.g., banking) if possible, or prefer a trusted network or mobile data.”

- **SUSPICIOUS BEHAVIOR**
  - **Triggers** (stronger anomalies, still conservative):
    - Many HTTPS requests fail while HTTP works and redirects to unfamiliar domains.
    - Multiple unrelated domains resolving to the same unusual IP not matching common CDN ranges.
    - Combined gateway/DNS anomalies (e.g., gateway and DNS pointing to unexpected addresses) plus content mismatch.
  - **Explanation**:
    - “This network’s behavior suggests possible interception, misconfiguration, or manipulation of your traffic.”
  - **Recommendation**:
    - “Avoid using this network for sensitive information. Consider leaving it and informing the owner if you are concerned.”

- **COULD NOT VERIFY**
  - **Triggers**:
    - Association or DHCP repeatedly fail.
    - Tests interrupted or time out.
    - User aborts before sufficient data is collected.
  - **Explanation**:
    - “WiFiGuard could not complete its checks on this network.”
  - **Recommendation**:
    - “Try again, move closer to the access point, or choose another network. If problems persist, ask the network owner.”

---

## 8. UI/UX flow

Constraints: **240x135 display**, **two buttons**, non‑technical users. Assume:

- **Left button**: “Back / Cancel / No / Scroll Up”.
- **Right button**: “Next / OK / Yes / Scroll Down”.

### 8.1 Device-side UX

1. **Starting setup mode**
   - Menu entry: “Phone‑Assisted Secure Join” (or “Authorized Network Check”).
   - Screen 1:
     - Title: “Phone‑Assisted Secure Join”
     - Text: “Use this only for networks you are allowed to join. WiFiGuard will check safety after connection.”
     - Buttons:
       - Left: “Back”
       - Right: “Start”

2. **SoftAP mode / waiting for credentials**
   - Screen:
     - “On your phone:
        1. Join WiFi: WiFiGuard‑Setup‑XXXX
        2. Open browser to: 192.168.4.1
        3. Enter WiFi name and password”
     - Optional small symbol indicating SoftAP is on.
     - Buttons:
       - Left: “Cancel”
       - Right: “Help” (simple scroll hints, or cycles to another explanatory screen).

3. **After form submit – connecting**
   - Screen:
     - “Connecting to:
        SSID: [MyCafeGuest]
        Please wait…”
     - Progress indicator (simple spinner or dots).
   - If connection fails quickly:
     - Show “Could not join. Check password.” with retry option (no password shown).

4. **Running safety checks**
   - Screen updates in phases (non‑technical language):
     - “Checking network…”
     - “Testing internet access…”
     - “Checking for login portal…”
     - “Measuring speed and stability…”
   - Optionally show a single line of status that advances.

5. **Showing final verdict**
   - Screen:
     - Big verdict label (e.g., “READY TO USE” in green, “LOGIN REQUIRED” in yellow, “CAUTION”/“SUSPICIOUS” in orange/red).
     - Below: 1‑2 line plain‑language explanation.
     - Buttons:
       - Left: “Details” (expert summary in a separate view, optional).
       - Right: “Next”.

6. **Details (optional expert view)**
   - Compact 2–3 pages navigated by buttons:
     - Page 1: Connection stats (RSSI, gateway, DNS, latency).
     - Page 2: Portal info (if any).
     - Page 3: Anomaly summary (if CAUTION/SUSPICIOUS).
   - This remains read‑only, no extra actions.

7. **Clearing credentials / saving profile**
   - Screen:
     - “Clear password and end test?”
     - Options:
       - Left: “Clear” (default highlighted).
       - Right: “More…”
   - If user chooses “More…”:
     - Screen:
       - “Save this as a trusted network profile (without password)?  
          WiFiGuard can remember basic info for future checks.”
       - Options:
         - Left: “No, just clear”
         - Right: “Save profile”
   - After action:
     - Confirmation screen:
       - “Credentials cleared. Profile saved.” or “Credentials cleared. No profile saved.”
     - Then return to main menu.

### 8.2 Phone-side UX (SoftAP web page)

- **Landing page**:
  - Header: “WiFiGuard – Phone‑Assisted Secure Join”.
  - Short safety text:
    - “Use this only for networks you are authorized to join.  
       WiFiGuard does not guess passwords or bypass logins.  
       It only checks the safety and quality of your connection.”
- **Form**:
  - Field: “Network name (SSID)”
    - Text input, optional “Scan nearby” toggle (retrieved by device and listed as options).
  - Field: “Password”
    - Password input, with “Show” checkbox.
  - Optional: “Note for yourself” (stored in trusted profile only; not sensitive).
- **Buttons**:
  - Primary: “Send to WiFiGuard and Start Check”.
  - Secondary: “Cancel / Disconnect”.
- **After submit**:
  - Status page:
    - “WiFiGuard is connecting and running checks. Watch the device screen for the verdict.”
    - Optionally show a simple live status via polling (e.g., “Connected”, “Testing internet…”, “Done: READY TO USE”).
  - Reminder:
    - “WiFiGuard will clear your password after this check unless you explicitly choose to save a trusted profile on the device (without password).”

---

## 9. Threat model and privacy considerations

### 9.1 What this feature protects against

- Joining a **misconfigured or broken network** that appears fine but has:
  - No internet connectivity.
  - Severe instability.
- Basic detection of:
  - **Captive portals** requiring login/acceptance.
  - **Consistent redirections** that may indicate interception or misconfiguration.
- Increased awareness:
  - Encourages user to **treat unknown networks cautiously**, especially if flagged CAUTION/SUSPICIOUS.

### 9.2 What it does NOT protect against

- Advanced, targeted attackers performing:
  - Sophisticated TLS interception with valid certificates.
  - Traffic pattern analysis or side‑channel attacks on user devices.
- Device‑specific exploits:
  - Malware on the user’s phone or laptop.
  - OS‑level vulnerabilities unrelated to network quality.
- All forms of **Evil Twin / Rogue AP** attacks:
  - It may detect some anomalies, but cannot guarantee detection.
- User mistakes like:
  - Entering credentials into a phishing captive portal.

### 9.3 Misuse it must prevent

- **No use as a password harvesting tool**:
  - No options to export or display passwords.
  - No remote APIs to send credentials elsewhere.
- **No offensive capabilities**:
  - Explicitly block development of:
    - Automated password guessing.
    - Handshake capture routines tied to this feature.
    - Deauth or PMKID attacks triggered from this flow.
- **No automated portal bypass**:
  - No form automation, credential stuffing, or token stealing via this feature.
- **No abusive LAN scanning**:
  - All tests target only:
    - Gateway.
    - Small set of well‑known public endpoints.
  - No port scans, host enumeration, or ARP sweeps.

### 9.4 Why it remains defensive and legitimate

- The user must:
  - Explicitly start the flow.
  - Explicitly provide credentials for an authorized network.
- Feature’s stated purpose and outputs:
  - Clearly about **safety and quality assessment**, not obtaining access.
- All actions:
  - Are within behavior of a normal client device verifying its own connection.
  - Do not attempt to exceed the privileges of a legitimate client.
- Documentation:
  - Should include a clear “Legitimate Use Only” section reinforcing this positioning.

---

## 10. ESP32 feasibility assessment

### 10.1 SoftAP + config webpage

- **Feasible**: ESP32 natively supports:
  - Simultaneous SoftAP+STA (though for MVP we can run them sequentially).
  - Lightweight HTTP servers.
- Constraints:
  - Keep HTML/JS minimal to fit flash/RAM constraints.
  - Limit concurrent connections and resource usage.

### 10.2 BLE credential handoff

- ESP32 supports BLE with WiFi:
  - Known coexistence considerations.
- Feasibility:
  - GATT service design is straightforward.
  - Must manage memory and concurrency carefully: avoid heavy WiFi traffic while BLE is active.

### 10.3 Session-only storage

- ESP32 RAM is adequate for short strings:
  - Passwords up to typical lengths (e.g., 64 chars).
- Explicit zeroization:
  - Implementable with C/C++ utilities.
- Avoiding flash:
  - Simple: do not call NVS write APIs for password fields.

### 10.4 Joining WPA2/WPA3 public/business guest networks

- **WPA2‑PSK**:
  - Fully supported and common.
- **WPA3‑Personal**:
  - ESP32 variants may require updated SDK; check device class and firmware.
- **Enterprise (WPA2‑Enterprise)**:
  - More complex (certs, usernames). For MVP, **keep feature limited to PSK guest networks**.
  - Future extension could add enterprise support if necessary.

### 10.5 Lightweight connectivity tests

- Pings and simple HTTP(S) requests:
  - Easily within ESP32’s capability.
- HTTPS:
  - Using mbedTLS (or similar) with small CA set.
  - Need to manage heap usage; limit number of simultaneous TLS connections.

### 10.6 Returning results to device UI and phone

- Device UI:
  - Simple state machine drives 240x135 screen updates.
- Phone UI (MVP):
  - Status page can poll a simple `/status` endpoint while SoftAP is active.
  - After SoftAP shut down (once joining target network), rely on **device screen** as canonical UI.
- V2:
  - BLE notifications or app‑side status display.

---

## 11. Phased implementation roadmap

**Phase 1: Planning / architecture (current phase)**

- Finalize:
  - Safety boundaries and non‑goals.
  - Credential lifecycle and threat model.
  - SoftAP + web page architecture.
- Design:
  - High‑level state machine (IDLE → SETUP_AP → WAIT_CREDS → CONNECTING → TESTING → VERDICT → CLEANUP).
  - Data structures for session credentials and results.

**Phase 2: Credential handoff MVP (SoftAP)**

- Implement:
  - SoftAP enable/disable logic with timeouts.
  - Minimal HTTP server and form handling:
    - Landing page + safety text.
    - SSID/password capture, basic validation.
  - RAM‑only storage of received credentials.
- UX:
  - Device screens for:
    - Start flow.
    - Instructions for connecting with phone.

**Phase 3: Join + safety checks**

- Implement:
  - Connection logic using session credentials only.
  - Test runner:
    - Association/DHCP checks.
    - Gateway reachability.
    - DNS tests.
    - HTTP/HTTPS tests.
    - Captive portal detection.
    - Latency and basic stability sampling.
- Build:
  - Internal result structure with normalized fields for verdict engine.

**Phase 4: Verdict UX**

- Implement:
  - Mapping from test results to verdicts (per model in this document).
  - Device‑side:
    - Verdict display.
    - Simple explanations and recommendations.
    - Optional expert details view.
- Optional:
  - Simple status feedback on phone while tests run (during SoftAP phase).

**Phase 5: Optional trusted profile (metadata-only)**

- Implement:
  - Local non‑secret trusted profile store:
    - SSID.
    - Security type.
    - BSSID/Vendor fingerprint (optional).
    - Portal domain pattern (optional).
    - Expected gateway/DNS hints (optional).
  - UX:
    - After verdict, offer to save profile without password.
    - Profile list view for later manual selection.
- Ensure:
  - Profiles cannot be used to extract or reconstruct passwords.

**Phase 6: Hardening / privacy review**

- Security review:
  - Confirm no password writes to flash or logs in default builds.
  - Check buffer lifetimes and zeroization.
- Abuse prevention:
  - Confirm no APIs or hidden commands enable offensive uses.
  - Add documentation clarifying legitimate use.
- Resilience:
  - Test behavior on flaky networks, captive portals, and common cafe/guest setups.
  - Watchdog and timeout handling so the device always returns to a known state.

**Future V2+**

- Add **BLE credential handoff**:
  - BLE GATT service and phone app.
  - Optional live verdict display on phone.
- Explore:
  - Basic enterprise support (if user base demands and security model can be kept simple/defensive).
  - Local PIN on device to gate profile changes.

---

## 12. Open questions / risks / tradeoffs

- **HTTP vs HTTPS for config page**:
  - Likely must be HTTP due to cert issues.
  - Risk: Nearby user on same SoftAP could see password.
  - Mitigation: Single client allowed, short lifetime, clear warnings. Consider simple pre‑shared “setup PIN” on device screen to gate submission (optional).
- **Captive portal detection robustness**:
  - Simple heuristics can misclassify.
  - Need conservative thresholds to avoid false “SUSPICIOUS” labeling.
- **BLE coexistence and user adoption**:
  - BLE complexity vs. real added value over SoftAP.
  - Need to validate if target users will install a companion app.
- **Handling complex enterprise/business guest flows**:
  - Many business guest WiFi setups use more elaborate authorization.
  - MVP should clearly document that the feature is optimized for **PSK‑based guest networks**.
- **Trusted profile storage growth and management**:
  - Need limits on number of profiles and simple UI to delete them.
- **Localization and accessibility**:
  - Limited screen space vs. multi‑language support.
  - May need simple icons and very short text for international users.

This plan keeps WiFiGuard clearly within **defensive, user‑consented network verification** and is realistic for ESP32 hardware and a small handheld form factor.

