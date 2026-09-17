# Twitch automation and scene layers

This feature is included in DeckStatus 2.1.0. It works in both Rekordbox and ProLink modes. The integration has automated protocol tests; it has **not yet been tested with a live Twitch account**.

## Viewer sign-in and ratings

Full History remains publicly readable. To submit or change a rating, viewers click **Sign in with Twitch**, confirm the displayed code on Twitch and return to Full History. Sign-in completes automatically while the page is open. No chat, email or channel-management permissions are requested. The page tells viewers that the streamer can see their Twitch username and rating.

Configure the **Public** application's Client ID in **Admin → Twitch** first. Viewer sign-in shares this Client ID but is independent of streamer/bot authorization and the automation enabled switch. It needs no client secret, registered callback URL or inbound Twitch callback. The native application needs outbound HTTPS access to Twitch. Existing Host/Origin protections, local access and the configured reverse-proxy domain continue to apply; spectator sign-in/voting does not require the remote-control switch.

Each verified Twitch account has one vote per track across browsers. Clicking another star updates that vote. Usernames come from Twitch token validation, never from the rating request. Existing anonymous votes remain in totals and are labelled separately; they cannot be assigned to a Twitch account afterwards.

In **Admin → Track ratings**, click the vote count to see usernames and individual stars in a modal. Only DeckStatus administrators can retrieve this list. Full History shows a **Manage ratings** link only to such administrators after their required password change. Twitch viewer sign-in does not create a DeckStatus account or grant administrative permissions, even for the streamer.

Viewer sessions use separate random HttpOnly/SameSite cookies (Secure on the configured HTTPS domain). Tokens are never returned to JavaScript or stored on disk. Sessions expire after at most one hour, the Twitch token's remaining lifetime, a Client ID change, sign-out or DeckStatus restart; sign in again afterwards. Every rating submission validates the token with Twitch, so revoked credentials fail closed and network outages temporarily prevent voting. Completed viewer login tokens have no requested scopes; refresh tokens are discarded. Pending login codes expire after at most 30 minutes. Login requests and token polling are bounded/rate-limited separately from votes. Browser-local sign-out discards the DeckStatus session; the user can also revoke the app through Twitch connections.

## Scene layers

The Scene Editor lists layers from front to back, with a type icon, name, visibility indicator and ordering controls. Select a row to edit its properties. The visibility button hides or shows a component without deleting it; arrows move it forward/backward. Hidden components remain editable in the list.

Click **Save** to update the persistent scene and its OBS output. Hidden overlay frames do not load until shown. Configured overlay backgrounds remain intentional; unused iframe regions stay transparent.

## Connect Twitch

1. In the [Twitch developer console](https://dev.twitch.tv/console/apps), register an application with client type **Public**. If the console requires a redirect URL, use `http://localhost`; DeckStatus uses device authorization, not that redirect. Do not choose a Confidential client or enter a client secret in DeckStatus.
2. Open **Admin → Twitch**, paste the application's **Client ID**, and save.
3. Choose **Link account** for the streamer. A code and Twitch authorization link appear. Open the link, sign into the channel owner's account and approve the requested permissions. DeckStatus polls for completion even if you close this page.
4. Optionally link a separate bot account. Use a private browser window or a different browser profile so Twitch authorizes the intended account. Without a bot, chat replies use the streamer account.
5. Open **Stream → Automations**, enable **Twitch automation** and save. This preference survives restart. When disabled, no EventSub connection is maintained; explicit account-linking requests still work. Admin → Twitch contains only Client ID and account linking controls.
6. Check the connection and individual subscription statuses. Channel Points require an eligible channel. An unavailable rewards subscription does not disable otherwise available chat/raid events.

The streamer grants `user:read:chat`, `user:write:chat` and `channel:read:redemptions`. The optional bot grants `user:write:chat`. The streamer receives events; the bot only sends messages. Bot moderation, bans, AutoMod and Twitch limits still apply.

Tokens stay in the native process and are encrypted with Windows DPAPI in the private portal store. They are never returned to the browser. After moving to another Windows user/PC, relink Twitch accounts; rules and scenes remain portable. Unlinking forgets the saved credentials and attempts to revoke the token at Twitch. Pending device authorizations expire and are not persisted.

DeckStatus opens outbound HTTPS/WebSocket connections; no public callback server or inbound Twitch port is required. Admin authentication and the existing remote-control permission apply to all Twitch changes, including when using a reverse proxy. Account linking does not start audio capture or change the DJ source.

## Example: show a component for 60 seconds

1. Add the component to a scene, hide it with its visibility button, and save the scene.
2. In **Stream → Automations**, load the channel's rewards, then add a rule.
3. Choose **Reward redeemed**, select a reward (or enter its exact ID), then **Show layer**.
4. Select the saved scene and layer, set **Duration = 60**, and choose a cooldown such as 5 seconds.
5. Save the automation settings. OBS continues using the same scene URL.

On redemption, the layer is shown. After 60 seconds it returns to the saved hidden state. The timer runs in DeckStatus and does not depend on an admin page or an active OBS viewer. The rendered scene polls approximately once per second, so changes can take about a second to appear.

For a personalized message, use **Add action** in the same rule and choose **Change text** with a text component as its target. For example: `Thanks, {user}! {message}`. Give both actions the same duration. The text component must also be visible, either in the saved design or through a show action. Add another action to send a chat reply or enable a component's audio reactivity with its own timer.

## Triggers and actions

| Trigger | Match behavior |
|---|---|
| Custom reward redeemed | Exact reward ID; empty matches any custom reward |
| Chat command | `!lights` also matches `!lights now`, but not `!lightsbad` |
| Chat contains text | Literal substring, no regular expression |
| Exact chat text | Entire message must match |
| Incoming raid | Optional minimum viewer count |
| Stream online / offline | Corresponding EventSub transition, not a periodic check |

Chat matching ignores ASCII letter case. Chat rules can allow everyone, moderators plus the streamer, or only the streamer. Rules use Twitch's supplied badges. Messages from the linked bot and DeckStatus's own sent replies are ignored to prevent feedback loops. Automatic/built-in rewards, subscriptions, follows and bits are not implemented in this version.

Actions can show, hide or toggle a layer, replace a text component's text, set opacity, X/Y position or rotation, or send a chat reply. Each rule contains **1–16 ordered actions**, with a shared trigger, permission filter and cooldown. Each action has its own target, value and duration. Use the arrow, duplicate and remove buttons to manage the list. Actions are evaluated immediately in order; duration controls how long a change lasts, not a delay before the next action. Multiple matching rules also run in list order. A missing target does not prevent the remaining actions from running.

**Enable audio reactive** and **Disable audio reactive** switch the target component's saved audio-reaction feature on/off at runtime. They support all six component types and the same duration/cooldown controls. For example, choose **Enable audio reactive**, select your layer and set duration to **60** to enable its configured audio transforms for one minute. Afterwards the saved audio-reaction setting is restored. A later enable/disable action replaces the earlier one for that layer.

Configure the layer's reactive properties (scale, position, rotation, opacity or FX settings) in the Scene Editor first. These actions use the shared audio input already configured/running in Admin; they never start or stop capture. Disabling audio reactivity does not hide the layer or disable a waveform's normal visualization.

Text/chat templates accept `{user}`, `{message}`, `{reward}` (the reward ID) and `{viewers}`. Substitutions are literal and not expanded recursively. Text is never interpreted as HTML, JavaScript, a URL or an executable command.

## Timers and saved designs

- Runtime actions never rewrite saved scenes, presets or their revisions.
- Duration **0** keeps a change until reset, settings save, scene revision change, streamer unlink or DeckStatus restart. Positive durations restore the saved property at expiry.
- A later action replaces an earlier action for the same scene/layer/property. Repeated triggers restart the timer after the rule's cooldown. Old timers cannot undo a newer action; expired actions do not resurrect an older override.
- Properties are independent: a text change and a visibility change can have different lifetimes.
- Saving a scene invalidates its runtime overrides. Saving Twitch settings clears all overrides and cooldowns. **Reset live changes** restores saved designs immediately; it does not disable future events.
- The editor displays the saved design and your local edits. The `/scene` OBS renderer displays runtime actions. Unsaved editor changes are not Twitch targets.
- Deleted scenes/layers produce an action-log error. Choose a replacement target and save the rule. Scene read keys only gain access to previously hidden content while a valid runtime action makes it visible.

Use **Test rules without sending** to inspect matching saved rules. This ignores cooldowns but never sends chat or changes live scenes. It uses `TestViewer` and 10 viewers for raid tests. The recent-action log is limited to this run and contains rule names/results, not a chat archive.

## Limits and recovery

- Up to 64 rules with 1–16 actions each. Cooldown/duration: 0–86,400 seconds. Existing scenes allow 32 layers each.
- Text substitutions are capped at 2,000 UTF-8 bytes; chat replies at 450. Chat output is queued and limited to one attempted message per two seconds. Up to 20 pending replies are retained for 60 seconds; overflow is reported and discarded. Settings saves, reset, unlink or reconnect discard pending replies. HTTP 429 pauses chat attempts for 30 seconds. Twitch can impose additional limits.
- Event IDs are deduplicated in memory for up to ten minutes (2,048 IDs). There is no durable event replay across a restart. Lost events during a network outage cannot be recovered by the WebSocket transport.
- The native worker validates linked tokens before connecting and at least every 55 minutes while enabled, rotates refresh tokens, monitors keepalives and reconnects on failure. Server-requested session migration avoids creating duplicate subscriptions. Other disconnects create a fresh session and subscriptions.
- Rules/settings use atomic persistence and a revision check. If another admin window saves first, reload before saving again. Existing user, scene, preset, media and rating data is preserved.
- Existing single-action rules upgrade automatically to one-item action lists at startup. The migration preserves their trigger, target, duration and other values, and advances the settings revision once. Both pages require an administrator; moving the editor does not grant operators access. Connection-only saves preserve rules, and automation-only saves preserve the Client ID.
- Integration actions cannot modify accounts, start capture, invoke processes or send arbitrary HTTP requests. Remote-control policy and administrator checks cannot be bypassed using an OBS read key.

## Implementation and tests

`src/twitch.cpp` implements Windows WinHTTP transport, account lifecycle, EventSub subscriptions and Helix calls. `src/automation.h` implements bounded rule matching and scene overrides. `Portal` stores settings and DPAPI-protected credentials. `/api/admin/twitch` and `/api/admin/automations` are administrator-only and retain the remote-control gate. `/api/scene` merges active overrides into the saved scene for rendering. `web/admin-twitch.js` owns account settings; `web/automations.js` owns the rule editor, tests and logs. Legacy full-settings API saves remain compatible and normalize single-action rules.

Run `twitch_test` through CTest for deterministic matching, timers, role/cooldown restrictions, deduplication, persistence, secret redaction, device authorization, refresh, reward listing, EventSub and chat tests. The test transport never connects to Twitch. `node tests/browser_twitch_layers_test.cjs` checks layers and admin UI; the existing portal/network suites check HTTP access boundaries.

The production WinHTTP WebSocket path and live Twitch permissions/channel eligibility still need a real-account validation. Synthetic tests do not constitute that validation. See [validation.md](validation.md).

Protocol references: [Twitch device authorization](https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/#device-code-grant-flow), [token validation](https://dev.twitch.tv/docs/authentication/validate-tokens/), [EventSub WebSockets](https://dev.twitch.tv/docs/eventsub/handling-websocket-events), [sending chat messages](https://dev.twitch.tv/docs/chat/send-receive-messages/), and [WinHTTP WebSocket receive](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpwebsocketreceive).
