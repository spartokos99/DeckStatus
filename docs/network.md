# Network access

Network access is optional and works in both modes. With no saved configuration or command-line override, DeckStatus listens only on `127.0.0.1:18740`.

## Configure in the web interface

1. On the DeckStatus PC, open **Connections → Network** (`/network/settings`).
2. Choose **This PC only**, **All IPv4 network interfaces**, or **One network interface**. The last option lists the PC's active IPv4 adapters.
3. Choose the HTTP port (1–65535; default 18740).
4. Optionally enable **Allow remote controls** if other PCs should manage audio capture or ProLink connections.
5. Click **Save for next start**, stop DeckStatus and start it again. Restarting starts a new track-history session.
6. Open a displayed LAN address on the other PC. Configure overlays from that address so generated OBS URLs reference the DeckStatus PC.

The page shows active and saved settings separately. Saving does not interrupt the current stream. Only administrators can inspect network settings; editing additionally requires the DeckStatus PC. Client permissions use the actual TCP peer, never forwarding headers. See the [technical wiki](WIKI.md) for accounts, public history and OBS access keys.

![Network settings in English with synthetic adapter data](images/network-settings-en.png)

## Access levels

| Setting | Result |
|---|---|
| This PC only | Other PCs cannot connect; local audio/ProLink controls remain available |
| Network access, remote controls off | Authenticated clients can use their account permissions; audio/ProLink control from another PC is disabled. Full History remains public. OBS keys allow their matching read routes. |
| Network access, remote controls on | Authenticated users on other PCs can additionally start/switch/stop audio capture and discover/connect/disconnect ProLink devices |

Signed-in clients can edit overlay designs and shared scenes. The remote-controls switch affects audio/ProLink controls, not scene editing or user-management permissions. Blocked source-control requests receive HTTP 403.

Use trusted networks: accounts protect access, but **HTTP traffic is not encrypted**. `0.0.0.0` listens on every IPv4 interface, including VPN adapters. Select a specific interface when desired. Host and same-origin checks remain enabled. Use numeric IPv4 addresses; IPv6 listeners, custom DNS names and reverse-proxy configuration are not included.

## Firewall and OBS

Both PCs must have a route to one another. Allow **DeckStatus.exe** through Windows Firewall on the private network for the selected **TCP** port. DeckStatus does not create firewall rules or router port forwards. Do not expose the service to the internet.

On the OBS PC, open an address such as `http://192.168.1.20:18740`, configure an overlay and copy its URL into an OBS Browser Source. `127.0.0.1` refers to the PC running that browser or OBS. Existing localhost URLs must be regenerated from the LAN address, or have their host/port replaced while retaining the path and query parameters. Browser designs stored under localhost are separate from designs stored under the LAN address.

HTTP LAN pages may not support the clipboard API. The copy buttons then select the URL and ask you to press Ctrl+C.

The audio source belongs to the **DeckStatus PC**, even when viewed elsewhere. ProLink's Java firewall requirements are separate from the HTTP port; see [ProLink setup](prolink.md).

## Command line and persistence

```powershell
# All IPv4 interfaces; remote controls use the saved value (initially off).
.\DeckStatus.exe --bind 0.0.0.0

# One interface and a different port.
.\DeckStatus.exe --bind 192.168.1.20 --port 18741

# ProLink with remote controls explicitly enabled.
.\DeckStatus.exe --mode prolink --bind 0.0.0.0 --allow-remote-control

# Override saved LAN access for this launch.
.\DeckStatus.exe --bind 127.0.0.1

# Separate settings file for a separate instance.
.\DeckStatus.exe --network-config 'D:\DeckStatus settings\network.json'
```

UTF-8 JSON is stored in `DeckStatus.network.json` beside the EXE, or the file selected by `--network-config`. Its folder must exist and be writable to save settings. Initial values:

```json
{
  "bind": "127.0.0.1",
  "port": 18740,
  "allowRemoteControl": false
}
```

Command-line options override supplied fields for that launch without rewriting the file. Remove conflicting arguments on the next launch to use saved values. `--allow-remote-control` enables remote controls; otherwise the saved value applies. Turn it off persistently on the page or in JSON.

Failed saves preserve the previous file through temporary-file replacement. Invalid/unreadable configurations stop startup with a diagnostic. An unavailable selected IP or occupied port causes a bind failure instead of opening another interface. Correct or rename the configuration file to recover. The standard configuration filename is excluded from Git; portable packages contain no saved network configuration.

## Validation

`network_access` covers validation, persistence, failed-save preservation, CLI precedence, wildcard destination/Host/Origin handling and local/remote peer policies. Additional source-bound remote HTTP checks run when Windows permits that local socket arrangement; they were unavailable on the development PC.

`node tests/browser_network_test.cjs` covers the page, restart notices, unsaved edits during polling, address/port selection, safe adapter names, EN/DE, mobile layout, connection failure and disabled remote controls.

`node tests/network_smoke.cjs [PATH_TO_BUILD_OR_EXTRACTED_PACKAGE]` launches the real EXE with isolated configuration files, saves settings and restarts it in both modes, checks CLI precedence and requests the host's actual LAN address. It uses demo metadata or an idle ProLink source, never production injection, device connection or audio capture.

The host's LAN URL was checked locally. A second physical PC and its OBS Browser Source have not been validated. Existing Rekordbox and ProLink compatibility limits remain unchanged.
