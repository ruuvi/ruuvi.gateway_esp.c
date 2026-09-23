# Ruuvi Gateway Firmware HTTP API Reference

**Status: implementation-derived, not a published stable API.**

This document is reverse-engineered from the firmware source code of the current
working tree (commit `b5f28727` at the time of writing) and describes exactly what
the on-device HTTP server dispatches. It is **not** a contract or a versioned public
API: routes, request/response shapes and status codes may change between firmware
revisions. Where a response body shape is not established by the C code itself, this
document links to the JSON schemas under [`../schemas/`](../schemas/) instead of
inventing a schema.

Endpoints that exist only in the Python test simulator
(`ruuvi.gwui.html/scripts/ruuvi_gw_http_server.py`) are **not** documented here unless
the firmware also dispatches them.

## Table of contents

- [Request lifecycle and dispatch](#request-lifecycle-and-dispatch)
- [LAN vs hotspot (access origin)](#lan-vs-hotspot-access-origin)
- [Captive portal / redirect / fallback behavior](#captive-portal--redirect--fallback-behavior)
- [Authentication model](#authentication-model)
- [ECDH session encryption](#ecdh-session-encryption)
- [Per-method authorization summary](#per-method-authorization-summary)
- [GET endpoints](#get-endpoints)
- [POST endpoints](#post-endpoints)
- [DELETE endpoints](#delete-endpoints)
- [Static web assets and fallback](#static-web-assets-and-fallback)
- [Endpoint index](#endpoint-index)

---

## Request lifecycle and dispatch

The gateway runs a custom (non-`esp_http_server`) lwIP `netconn` server inside the
esp32-wifi-manager component. Requests flow through the following chain:

1. **Accept + buffering** —
   [`http_server_accept_and_handle_conn()`](../components/esp32-wifi-manager/src/http_server_accept_and_handle_conn.c#L434)
   accepts a connection and
   [`http_server_netconn_serve()`](../components/esp32-wifi-manager/src/http_server_accept_and_handle_conn.c#L347)
   accumulates the raw request (header + `Content-Length` body) into a heap buffer.
   Request size is capped by `HTTP_SERVER_MAX_REQUEST_SIZE` and body by
   `HTTP_SERVER_MAX_ENCRYPTED_CONTENT_SIZE`; oversize requests are dropped/closed.
2. **Parse + origin + captive portal** —
   [`http_server_netconn_serve_handle_req()`](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L117)
   parses the request line, decides LAN-vs-hotspot, applies the captive-portal
   redirect, and applies the "block LAN while AP active" rule.
3. **Method dispatch** —
   [`http_server_handle_req()`](../components/esp32-wifi-manager/src/http_server_handle_req.c#L679)
   branches on `GET` / `DELETE` / `POST` (any other method -> `400`). GET and POST are
   wrapped by ECDH helpers
   ([GET L575](../components/esp32-wifi-manager/src/http_server_handle_req.c#L575),
   [POST L608](../components/esp32-wifi-manager/src/http_server_handle_req.c#L608)).
4. **Built-in routes** — the wifi-manager handles `auth`, `ap.json`, `status.json`,
   `connect.json`, `connect_wps` directly
   ([`http_server_handle_req_get()`](../components/esp32-wifi-manager/src/http_server_handle_req.c#L83),
   [`_delete()`](../components/esp32-wifi-manager/src/http_server_handle_req.c#L203),
   [`_post()`](../components/esp32-wifi-manager/src/http_server_handle_req.c#L507)).
5. **Application callbacks** — anything else is forwarded to the application callbacks
   registered in
   [`network_subsystem.c`](../main/network_subsystem.c#L553):
   `cb_on_http_get` -> [`http_server_cb_on_get()`](../main/http_server_cb_on_get.c#L542),
   `cb_on_http_post` -> [`http_server_cb_on_post()`](../main/http_server_cb_on_post.c#L310),
   `cb_on_http_delete` -> [`http_server_cb_on_delete()`](../main/http_server_cb.c#L107).

All paths are matched by exact `strcmp` on the leading-slash-stripped URI path; there
are no regex/parameterized path segments. "Parameters" are always URL query
parameters, parsed with `http_server_get_from_params*()` helpers.

## LAN vs hotspot (access origin)

Origin is computed in
[`http_server_netconn_serve_handle_req()` L139](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L139):

```
flag_access_from_lan = (local_ip != AP_ip)
```

- **`flag_access_from_lan == false`** — the request arrived on the SoftAP interface
  (the configuration hotspot). Shared auth checks return "allow" (see
  [Authentication model](#authentication-model)); endpoint-specific checks such as
  the [DELETE /auth](#delete-auth) logout-session requirement still apply.
- **`flag_access_from_lan == true`** — the request arrived over the STA/Ethernet LAN
  interface. Full authentication is enforced.

Additional origin rules:

- **Block LAN while AP active** — if the SoftAP is up and the "block requests from LAN
  while AP is active" bit is set
  ([`wifi_manager_is_req_from_lan_blocked_while_ap_is_active()`](../components/esp32-wifi-manager/src/wifi_manager.c#L414)),
  any LAN request returns **`503`**
  ([L157-165](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L157)).
- Some endpoints are LAN-forbidden regardless of auth (`POST`/`DELETE /connect.json`,
  `POST /connect_wps` -> `403`) and one is hotspot-only (`GET /info.json`).

## Captive portal / redirect / fallback behavior

- **Captive-portal redirect (`302`)** — for hotspot requests whose `Host:` header does
  **not** target the AP IP, the server replies `302` to bounce clients to the AP IP
  ([L169-175](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L169)).
  This is transport-level captive-portal behavior, not an API route. DNS-based captive
  portal is handled separately by the DNS server.
- **Parse failure** -> `400`
  ([L133](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L133)).
- **Auth redirect (`302`)** — for `RUUVI`/`DEFAULT` auth types, an unauthenticated LAN
  GET covered by the [GET path authentication rule](#authentication-model) is
  redirected `302` when the auth check returns `401` without a recognized bearer
  scheme. `/auth` has its own handler. The response normally includes a previous-URL `Set-Cookie`;
  `/ap.json` and `/status.json` are still redirected but omit that cookie
  ([L153-167](../components/esp32-wifi-manager/src/http_server_handle_req.c#L153)).
- **Static fallback** — unmatched GET paths other than those ending in `.json` reach
  the GWUI FATFS file handler after any applicable authentication check. This includes
  paths with no dot; see [Static web assets and fallback](#static-web-assets-and-fallback).

## Authentication model

The shared authorization check is enforced only for LAN requests. The configured `auth_type` selects the scheme
(`ALLOW`, `BASIC`, `DIGEST`, `RUUVI`, `DEFAULT`, `DENY`; `BEARER` is API-key only). The
central check is
[`http_server_handle_req_check_auth()`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L330),
which returns "allow" immediately when `flag_access_from_lan == false`
([L336](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L336)).

**Which GET resources require auth:** in
[`http_server_handle_req_get()` L135-173](../components/esp32-wifi-manager/src/http_server_handle_req.c#L135),
the server uses `strrchr(p_file_name, '.')` on the **entire path**, excluding the
query string. The shared auth check runs when there is **no dot anywhere in the
path**, or when the substring from the last dot to the end is exactly **`.json`**
(case-sensitive). It does not extract an extension from only the final path segment.
For example, `/assets/readme` is auth-checked, but `/assets.v1/readme` and
`/data.JSON` skip this check. These example paths are not additional API routes;
subsequent dispatch still determines whether a file exists or returns `404`.

Two earlier special cases apply: the empty path becomes `index.html`, and `/auth`
uses its dedicated authentication handler. Thus the web-UI root and usual static
assets (`.html`, `.js`, `.css`, `.png`, `.svg`, `.ttf`) skip the shared auth check,
while the `.json` data endpoints and dot-free action paths (`metrics`, `history`,
`validate_url`, `extra_cfg`) are auth-checked. Static fallback alone does not imply
unauthenticated access: a dot-free fallback path still passes through this check.

### Bearer API-key (RO vs RW)

Bearer tokens are checked in
[`http_server_handle_req_check_auth_bearer()`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L49)
against two configured keys:

- `auth_api_key_rw` — read/write key. Always accepted when present and matching.
- `auth_api_key` — read-only key. Accepted **only** when the caller does not require RW
  access (`flag_check_rw_access == false`).

`flag_check_rw_access_with_bearer_token` is set per method/route:

| Context                                                          | RW required for bearer? | Source                                                                                        |
|------------------------------------------------------------------|-------------------------|-----------------------------------------------------------------------------------------------|
| GET covered by the path auth rule, except `/auth` and `/ap.json` | No (RO key accepted)    | [handle_req_get L129-136](../components/esp32-wifi-manager/src/http_server_handle_req.c#L129) |
| GET `/ap.json`                                                   | **Yes** (RW only)       | [L130-133](../components/esp32-wifi-manager/src/http_server_handle_req.c#L130)                |
| POST except `/auth`                                              | **Yes** (RW only)       | [handle_req_post L537](../components/esp32-wifi-manager/src/http_server_handle_req.c#L537)    |
| DELETE except `/auth`                                            | **Yes** (RW only)       | [handle_req_delete L219](../components/esp32-wifi-manager/src/http_server_handle_req.c#L219)  |

`/auth` has method-specific behavior: [GET](#get-auth) and [POST](#post-auth)
do not use bearer authorization. [DELETE](#delete-auth) first runs the shared
LAN check (RW required if a bearer header is supplied), then requires a valid
interactive session cookie in `RUUVI`/`DEFAULT` mode. An RW key alone cannot log out
a session; the logout handler can still return `401` or `503` after the shared
check succeeds.

A recognized bearer key returns an auth JSON with `200` (allowed) or `401`
(prohibited: e.g. RO key on a RW route) via
[`http_server_handle_req_auth_bearer()`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L257).

### Interactive schemes

- **`BASIC`** — HTTP Basic; missing/invalid -> `401` with `WWW-Authenticate: Basic`
  ([L84-123](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L84)).
- **`DIGEST`** — HTTP Digest (MD5)
  ([L125-184](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L125)).
- **`RUUVI`/`DEFAULT`** — cookie session flow. `GET /auth` issues/validates a session
  cookie; `POST /auth` performs a SHA-256 challenge-response login; `DELETE /auth`
  logs out. Unauthenticated -> `401` (with a fresh session id / challenge).
- **`ALLOW`** — always authorized (still issues a session id for RUUVI cookie flow).
- **`DENY`** — always `403`
  ([`http_server_resp_403_auth_deny`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L252)).

The auth JSON payload (returned by `/auth` and inline auth failures) is produced by
`http_server_fill_auth_json()`; its exact fields are defined in wifi-manager source and
are not schema-tracked in this repo.

## ECDH session encryption

- **GET** responses can carry a server ECDH public key: if a request includes a
  `Ruuvi-Ecdh-Pub-Key:` header, the handshake runs and the response adds a
  `Ruuvi-Ecdh-Pub-Key:` header
  ([`http_server_handle_req_get_with_ecdh_key()` L575](../components/esp32-wifi-manager/src/http_server_handle_req.c#L575),
  [`http_server_ecdh.c`](../components/esp32-wifi-manager/src/http_server_ecdh.c)).
- **POST** bodies can be encrypted: if a request includes `Ruuvi-Ecdh-Encrypted: true`,
  the body is a JSON envelope `{"encrypted","iv","hash"}` that is AES-decrypted before
  dispatch
  ([`http_server_handle_req_post_with_ecdh_key()` L608](../components/esp32-wifi-manager/src/http_server_handle_req.c#L608),
  [`http_server_decrypt()` L331](../components/esp32-wifi-manager/src/http_server_handle_req.c#L331)).
  Decryption failure -> `400`. Decrypted content over
  `HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE` -> `400`.

This is a transport wrapper applied to the routes below; it does not add new paths.

## Per-method authorization summary

| Method                                                            | Auth from hotspot       | Auth from LAN                     | Bearer scope                 | Notes                                                            |
|-------------------------------------------------------------------|-------------------------|-----------------------------------|------------------------------|------------------------------------------------------------------|
| GET (path contains a dot and does not end in `.json`)             | none                    | **none**                          | n/a                          | skips shared auth check; static file lookup follows              |
| GET (no dot anywhere in path, or ends in `.json`), except `/auth` | allow                   | enforced                          | RO or RW                     | `ap.json` requires RW bearer                                     |
| POST except `/auth`                                               | allow                   | enforced                          | **RW only**                  | body may be ECDH-encrypted                                       |
| DELETE except `/auth`                                             | allow                   | enforced                          | **RW only**                  |                                                                  |
| GET `/auth`                                                       | allow                   | configured scheme                 | ignored                      | reports scheme/session auth state                                |
| POST `/auth`                                                      | allow                   | interactive login                 | ignored                      | LAN requires `RUUVI`/`DEFAULT`, cookie, and challenge response   |
| DELETE `/auth`                                                    | logout session required | shared check, then logout session | RW for shared LAN check only | `RUUVI`/`DEFAULT` and authorized cookie required on both origins |

Common status codes (enum:
[`wifi_manager_defs.h` L256+](../components/esp32-wifi-manager/src/include/wifi_manager_defs.h#L256)):
`200, 302, 400, 401, 403, 404, 409, 500, 502, 503, 504`.

---

## GET endpoints

Unless noted, "Auth" describes LAN behavior; hotspot access is always allowed. Success
responses are `Content-Type: application/json` unless stated otherwise.

### GET /auth

- **Purpose:** issue/validate an interactive auth session; report auth state.
- **Query params:** none.
- **Auth:** scheme-dependent. Returns auth JSON with `200` when authorized, else `401`
  (`BASIC`/`DIGEST`/`RUUVI`/`DEFAULT`) or `403` (`DENY`).
  Bearer API keys are not checked by this handler.
- **Source:** [handle_req_get L115](../components/esp32-wifi-manager/src/http_server_handle_req.c#L115)
  -> [`http_server_handle_req_get_auth()`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c#L347).

### GET /ap.json

- **Purpose:** trigger a synchronous Wi-Fi scan and return the visible access points.
- **Query params:** none.
- **Auth:** enforced on LAN; **RW bearer required** for API-key auth. Under
  `RUUVI`/`DEFAULT`, unauthenticated access returns `302` but omits the previous-URL
  cookie.
- **Success:** `200` JSON list of APs (heap-generated by `wifi_manager_scan_sync()`).
- **Errors:** `503` if scan/JSON allocation fails.
- **Source:** [handle_req_get L175-185](../components/esp32-wifi-manager/src/http_server_handle_req.c#L175).

### GET /status.json

- **Purpose:** current network/connection status.
- **Query params:** none.
- **Auth:** enforced on LAN (RO bearer accepted). Under `RUUVI`/`DEFAULT`,
  unauthenticated access returns `302` but omits the previous-URL cookie.
- **Success:** `200` JSON network status generated by
  [`json_network_info_do_generate_internal()`](../components/esp32-wifi-manager/src/json_network_info.c#L232):
  connection fields such as `ssid`, `ip`, `netmask`, `gw`, `dhcp`, `urc`, and (depending on
  firmware version) `is_time_valid`, plus optional `extra` update-progress information.
  It does not expose `ESP_FW`, `NRF_FW`, or `DEVICE_ADDR`.
  [`ruuvi_gw_status.schema.json`](../schemas/ruuvi_gw_status.schema.json) describes the outbound
  HTTP statistics report generated by `http_json_create_status_str()` and sent through
  [`http_post_stat.c`](../main/http_post_stat.c), not this GET endpoint.
- **Errors:** `503` on mutex/timeout.
- **Side effects:** calls `wifi_manager_cb_on_request_status_json()`.
- **Source:** [handle_req_get L187-198](../components/esp32-wifi-manager/src/http_server_handle_req.c#L187).

### GET /ruuvi.json

- **Purpose:** return the UI-facing gateway configuration with password hiding
  enabled and **enter configuration mode** (not a full raw configuration export).
- **Query params:** none.
- **Auth:** enforced on LAN (RO bearer accepted).
- **Success:** `200` JSON gateway config; conforms to
  [`ruuvi_gw_cfg.schema.json`](../schemas/ruuvi_gw_cfg.schema.json).
- **Runtime device information:** `fw_ver` (ESP32 firmware), `nrf52_fw_ver` (co-processor
  firmware), and `gw_mac` (nRF52 device MAC), emitted by
  [`gw_cfg_json_add_items_device_info()`](../main/gw_cfg_json_generate.c#L69).
- **Credential visibility:** LAN-authentication secrets `lan_auth_pass`,
  `lan_auth_api_key` (RO), and `lan_auth_api_key_rw` (RW) are omitted, not returned
  as empty strings or masked values. `lan_auth_type` and `lan_auth_user` remain
  visible. API-key presence is reported by `lan_auth_api_key_use` and
  `lan_auth_api_key_rw_use`: each is `true` when its corresponding stored key is
  non-empty, otherwise `false`. Clients must use these booleans to detect
  configured keys; absent credential fields do not mean credentials are unset.
- **Errors:** `503` if config JSON generation fails.
- **Side effects:** activates cfg-mode and (re)starts the cfg-mode deactivation timer.
- **Source:** [`http_server_resp_json_ruuvi()` L47](../main/http_server_cb_on_get.c#L47)
  via [`http_server_resp_json()` L227](../main/http_server_cb_on_get.c#L227);
  [`gw_cfg_ruuvi_json_generate()`](../main/gw_cfg_ruuvi_json.c#L13) selects the
  [UI-client generator](../main/gw_cfg_json_generate.c#L865), whose
  [LAN-auth serialization](../main/gw_cfg_json_generate.c#L521) omits the secrets.

### GET /firmware_update.json

- **Purpose:** fetch available firmware-update info from the configured update URL.
- **Query params:** none. Honors the `X-Request-Timestamp:` request header to set the
  clock when NTP is unused/unsynced.
- **Auth:** enforced on LAN (RO bearer accepted).
- **Success:** `200` JSON (proxied from the remote update server).
- **Errors:** `504` if the remote info download fails.
- **Source:** [`http_server_resp_json_firmware_update()` L69](../main/http_server_cb_on_get.c#L69).

### GET /info.json

- **Purpose:** device/build info (FW versions, MACs, device id, tags-seen count,
  button presses).
- **Query params:** none.
- **Auth:** enforced on LAN (RO bearer accepted), before endpoint dispatch. In default auth mode,
  missing/Basic/Digest credentials without an authorized cookie return `302`; a rejected bearer
  returns `401` ([GET authentication](../components/esp32-wifi-manager/src/http_server_handle_req.c#L153)).
  **Hotspot-only:** after authorization succeeds, a LAN request reaches the application callback,
  where `flag_access_from_lan == true` makes the resource unknown and returns `404`
  (see [L239](../main/http_server_cb_on_get.c#L239)). It is served from the AP side.
- **Success:** `200` JSON.
- **Errors:** `503` on allocation failure.
- **Source:** [`http_server_resp_json_info()` L211](../main/http_server_cb_on_get.c#L211).

### GET /&lt;other&gt;.json

- Any other `*.json` GET that reaches the app callback returns `404`
  ([`http_server_resp_json()` L243](../main/http_server_cb_on_get.c#L243)).

### GET /metrics

- **Purpose:** Prometheus-style metrics.
- **Query params:** none.
- **Auth:** enforced on LAN (RO bearer accepted).
- **Success:** `200`, `Content-Type: text/plain; version=0.0.4`, no-cache.
- **Errors:** `503` on allocation failure.
- **Source:** [`http_server_resp_metrics()` L247](../main/http_server_cb_on_get.c#L247).

### GET /history

- **Purpose:** buffered advertisement history as a streamed JSON generator.
- **Query params:**
  - `time=<seconds>` — used when NTP timestamps are enabled; selects the time window
    (default interval `60`s).
  - `counter=<n>` — used when NTP timestamps are disabled; selects starting counter.
  - `decode=<true|false>` — decode sensor payloads (default `true`).
  - Parsing: [`http_server_get_filter_from_params()` L270](../main/http_server_cb_on_get.c#L270),
    [`http_server_get_decode_from_params()` L304](../main/http_server_cb_on_get.c#L304).
- **Auth:** enforced on LAN (RO bearer accepted).
- **Success:** `200` streamed JSON; conforms to
  [`ruuvi_history.schema.json`](../schemas/ruuvi_history.schema.json).
- **Errors:** `503` on allocation failure.
- **Side effects:** updates network-timeout timestamp; `main_task_on_get_history()`.
- **Source:** [`http_server_resp_history()` L322](../main/http_server_cb_on_get.c#L322).

### GET /validate_url

- **Purpose:** server-side connectivity/credential validation for a target URL/MQTT
  broker/remote-config/firmware-update-url/file. Dispatched by `validate_type`.
- **Query params (parsed in
  [`validate_url()` L1095](../main/validate_url.c#L1095)):**
  - `validate_type=` selects the check; supply one of the full names
    `check_post_advs`, `check_post_stat`, `check_mqtt`,
    `check_remote_cfg`, `check_fw_update_url`, `check_file`
    ([L124](../main/validate_url.c#L124)).
  - `url=` and `auth_type=` are required for **every** check. `auth_type` must be
    `none`, `basic`, `bearer`, `token`, or `api_key`; use `auth_type=none` even when
    the target needs no credentials. Missing/unknown `auth_type` returns HTTP `400`
    ([L1025](../main/validate_url.c#L1025)).
  - `user=` and the encrypted secret triplet (`encrypted_password=`,
    `encrypted_password_iv=`, `encrypted_password_hash=`) supply target credentials
    when needed, not caller authentication ([L83](../main/validate_url.c#L83)).
    The triplet carries the password, bearer/token value, or API key, according to
    `auth_type`; it is not universally required for unauthenticated checks.
  - Boolean options: `use_saved_password=`, `use_ssl_client_cert=`,
    `use_ssl_server_cert=`, `use_extra_http_path=`, `use_extra_http_query=`,
    `use_extra_http_headers=`. Send `true` or `false`; omitted options default to
    `false`. Their effect depends on the selected check.
- **Check-specific requirements:**
  - `check_post_advs`, `check_post_stat`, `check_mqtt`, and `check_remote_cfg`
    support `use_saved_password=true` to use that configuration's saved secret
    instead of the encrypted triplet. This does not supply `user=` automatically.
  - `check_mqtt` requires `mqtt_topic_prefix=` and `mqtt_client_id=` to be present
    (empty values are permitted). `url` must have the form `scheme://host:port`,
    with scheme `mqtt`, `mqtts`, `mqttws`, or `mqttwss`, an explicit decimal port
    in `1..65535`, and no trailing path. With `auth_type` other than `none`, both
    `user` and a supplied/saved password must be available. Missing required
    fields, oversized fields, or a malformed MQTT URL return HTTP `400`.
    `mqtt_disable_retained_messages=true|false` is optional (default `false`)
    ([L313-531](../main/validate_url.c#L313)).
  - `check_remote_cfg` requires `user` and a supplied/saved password for Basic,
    or a supplied/saved secret for bearer/token/API-key auth. Missing or oversized
    credential fields return HTTP `400`; unlike `check_file`, empty strings pass
    this local credential check ([L534-681](../main/validate_url.c#L534)).
  - `check_file` does **not** use `use_saved_password`: Basic requires a nonempty
    `user` and decrypted password; bearer/token/API-key auth requires a nonempty
    decrypted secret. Missing/empty credentials return HTTP `400`
    ([L843-978](../main/validate_url.c#L843)).
  - `check_fw_update_url` does not use the supplied target credentials, but the
    common parser still requires `auth_type`; use `none`
    ([L811-840](../main/validate_url.c#L811)).
- **Caller auth:** enforced on LAN (RO bearer or an authorized interactive
  session accepted). The query's `auth_type` controls authentication to the
  **external target**, not access to this endpoint. Shared LAN auth runs before
  parameter validation: in `RUUVI`/`DEFAULT` mode without an authorized cookie,
  a request without a recognized bearer header gets HTTP `302`, while an explicit
  invalid bearer gets HTTP `401`. Neither is a JSON check result; see
  [Authentication model](#authentication-model).
- **HTTP transport status after caller authorization:** completed checks return `200` with an
  `application/json` body, including when the check reports failure. Direct error
  paths return `400` for missing/invalid parameters (including check-specific
  credential/MQTT parameter validation), `500` for allocation/internal failures or
  a missing/unrecognized `validate_type` (after the common `auth_type` and `url`
  checks pass), and `409` if a firmware update is already in progress
  ([cb_on_get L568-582](../main/http_server_cb_on_get.c#L568)).
- **Application status:** the JSON body is `{"status": <code>, "message": <string>}`;
  `status` carries the check result, such as `200`, `400` for an incorrect URL,
  `401` for rejected MQTT credentials, or a remote server's error code. These are
  not the outer HTTP status. Successful firmware-update URL validation instead
  returns `{"status": 200, "json": <release metadata>}`. Both response helpers send
  HTTP `200`, or HTTP `500` if response construction fails
  ([`http_server_cb_gen_resp()` / `_json()`](../main/http_server_cb.c#L644)).
- **Side effects:** clears saved TLS session tickets; suspends/resumes data relaying
  around the check.
- **Source:** [cb_on_get L568](../main/http_server_cb_on_get.c#L568),
  [`validate_url()`](../main/validate_url.c#L1095).

### GET /extra_cfg

- **Purpose:** read a stored blob config file (e.g. certificates) from gw_cfg storage.
- **Query params:** `file=<name>` (URL-decoded, required).
- **Auth:** enforced on LAN (RO bearer accepted).
- **Success:** `200`, body is the file content as text.
- **Errors:** `400` (missing/unknown `file`), `403` (file is not a blob / access
  denied), `404` (file missing), `500` (read failure).
- **Source:** [`http_server_cb_on_get_extra_cfg()` L500](../main/http_server_cb_on_get.c#L500).

### GET /&lt;path&gt; (static asset fallback)

- See [Static web assets and fallback](#static-web-assets-and-fallback).

---

## POST endpoints

POST routes **except `/auth`** require LAN auth with **RW bearer** for API-key auth;
hotspot access is allowed. `/auth` uses the interactive login flow below, not the
shared bearer check. Bodies may be ECDH-encrypted (see [ECDH](#ecdh-session-encryption)).
For routes dispatched to the application callback, two guards run first in
[`http_server_cb_on_post()` L310](../main/http_server_cb_on_post.c#L310):

- If a firmware update is in progress -> **`409`**
  ([L318](../main/http_server_cb_on_post.c#L318)).
- After the `fw_update_reset` special-case, if cfg updating is prohibited
  (updating mode) -> **`403`** ([L331](../main/http_server_cb_on_post.c#L331)).

### POST /auth

- **Purpose:** interactive login (`RUUVI`/`DEFAULT` challenge-response), establishing an
  authorized session.
- **Body:** JSON `{"login": <string>, "password": <sha256-challenge-response-hex>}`
  ([`json_ruuvi_auth_parse()` L50](../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c#L50)).
  On LAN, requires the session cookie from a prior `GET /auth`.
- **Auth:** on LAN, only valid for `RUUVI`/`DEFAULT`; other auth types -> `503`.
  The route is dispatched before the shared authorization check, so a bearer key
  cannot substitute for the cookie and challenge-response login. From the hotspot,
  the handler returns `200` auth JSON without checking the mode, cookie, or body.
- **Success:** `200` auth JSON; sets/clears prev-url cookie; may add `Ruuvi-prev-url:`.
- **Errors:** `401` (missing session cookie / bad session / wrong user or password,
  each with a fresh session id), `500` (allocation), `503` (wrong auth type).
- **Source:** [handle_req_post L521](../components/esp32-wifi-manager/src/http_server_handle_req.c#L521)
  -> [`http_server_handle_req_post_auth()` L248](../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c#L248).

### POST /connect.json

- **Purpose:** connect to Wi-Fi (or Ethernet) with provided credentials.
- **Body:** JSON `{"ssid": <string|null>, "password": <string|null>}`
  ([`http_server_parse_cjson_wifi_ssid_password()` L378](../components/esp32-wifi-manager/src/http_server_handle_req.c#L378)).
  `ssid=null,password=null` -> connect to Ethernet; `password=null` with matching saved
  ssid -> reconnect; otherwise store creds and connect async.
- **Auth:** enforced; **LAN forbidden** -> `403` even if authenticated
  ([L556-560](../components/esp32-wifi-manager/src/http_server_handle_req.c#L556)).
- **Success:** `200 {}`.
- **Errors:** `400` (bad/missing ssid), `403` (from LAN).
- **Source:** [`http_server_handle_req_post_connect_json()` L444](../components/esp32-wifi-manager/src/http_server_handle_req.c#L444).

### POST /connect_wps

- **Purpose:** start Wi-Fi WPS pairing.
- **Body:** none required.
- **Auth:** enforced; **LAN forbidden** -> `403`
  ([L565-569](../components/esp32-wifi-manager/src/http_server_handle_req.c#L565)).
- **Success:** `200 {}`.
- **Source:** [`http_server_handle_req_post_connect_wps()` L498](../components/esp32-wifi-manager/src/http_server_handle_req.c#L498).

### POST /ruuvi.json

- **Purpose:** update gateway configuration (network cfg from hotspot, ruuvi cfg from
  LAN). From LAN the network portion is not applied
  (`flag_access_from_lan ? NULL : &flag_network_cfg`).
- **Body:** JSON gateway config; parsed by `json_ruuvi_parse_http_body()`. Related
  schema: [`ruuvi_gw_cfg.schema.json`](../schemas/ruuvi_gw_cfg.schema.json).
- **Success:** `200 {}` (`application/json`, no-cache).
- **Errors:** `503` on allocation/parse failure.
- **Side effects:** clears saved TLS session tickets; updates config; restarts cfg-mode
  deactivation timer; may update Ethernet IP / Wi-Fi AP config.
- **Source:** [`http_server_cb_on_post_ruuvi()` L44](../main/http_server_cb_on_post.c#L44).

### POST /bluetooth_scanning.json

- **Purpose:** update BLE scan/filter configuration.
- **Body:** JSON with scan/filter fields (`gw_cfg_json_parse_scan/_filter`).
- **Success:** `200 {}` (`application/json`, no-cache).
- **Errors:** `503` on parse/allocation failure.
- **Side effects:** `adv_post_nrf52_cfg_update()`; emits `EVENT_MGR_EV_CFG_BLE_SCAN_CHANGED`.
- **Source:** [`http_server_cb_on_post_ble_scanning()` L98](../main/http_server_cb_on_post.c#L98).

### POST /fw_update.json

- **Purpose:** start a firmware update from a validated set of binaries.
- **Body:** JSON parsed by `json_fw_update_parse_http_body()`.
- **HTTP transport status:** `200` for the handler's application results, or `500`
  if response construction fails. The shared POST guards can return `409`/`403`
  before this handler runs.
- **Application status:** JSON `{"status": 200, "message": "OK"}` on success;
  failures use the same JSON shape with `status: 400` (parse / bad URL),
  `status: 503` (failed to start), or the download-check status from
  `http_server_check_fw_update_binary_files()`, plus an explanatory `message`.
  These codes are JSON fields, not HTTP response codes
  ([`http_server_cb_gen_resp()`](../main/http_server_cb.c#L644)).
- **Side effects:** suspends relaying; sets update reason (LAN vs hotspot); starts update.
- **Source:** [`http_server_cb_on_post_fw_update()` L129](../main/http_server_cb_on_post.c#L129).

### POST /fw_update_url.json

- **Purpose:** set the persistent firmware-update URL.
- **Body:** JSON parsed by `json_fw_update_url_parse_http_body_get_url()`.
- **HTTP transport status:** `200` for the handler's application results, or `500`
  if response construction fails; the shared POST guards can return `409`/`403`.
- **Application status:** JSON `{"status": 200, "message": "OK"}` on success, or
  `{"status": 400, "message": "Failed to parse HTTP body"}` on parse failure;
  both are sent with HTTP `200`
  ([`http_server_cb_gen_resp()`](../main/http_server_cb.c#L644)).
- **Source:** [`http_server_cb_on_post_fw_update_url()` L167](../main/http_server_cb_on_post.c#L167).

### POST /fw_update_reset

- **Purpose:** clear firmware-update extra-info in `status.json`. Handled **before** the
  "cfg updating prohibited" guard, so it works during updating mode.
- **Body:** none required.
- **Success:** `200 {}` (`application/json`, no-cache).
- **Source:** [`http_server_cb_on_post_fw_update_reset()` L184](../main/http_server_cb_on_post.c#L184).

### POST /gw_cfg_download

- **Purpose:** download and apply gateway config from the configured remote server.
- **Body:** none required.
- **HTTP transport status:** `200` for the handler's application results, or `500`
  if response construction fails; the shared POST guards can return `409`/`403`.
- **Application status:** JSON `{"status": <code>, "message": <string>}`, where
  `status` is the result of `http_server_gw_cfg_download_and_update()`. Success is
  `status: 200`, with an empty message or a notice that network configuration changes
  require a reboot. Failures carry the operation's error code and message in JSON,
  while the outer HTTP status remains `200`
  ([`http_server_cb_gen_resp()`](../main/http_server_cb.c#L644)).
- **Source:** [`http_server_cb_on_post_gw_cfg_download()` L200](../main/http_server_cb_on_post.c#L200).

### POST /ssl_cert  and  POST /extra_cfg

- **Purpose:** write a stored config file (string or blob, e.g. certificates / HTTP
  headers). `ssl_cert` and `extra_cfg` both dispatch to the same handler.
- **Query params:** `file=<name>` (URL-decoded, required).
- **Body:** raw file content (string or blob). For the HTTP-headers blob file the
  content must end with CRLF.
- **Success:** `200 {}`.
- **Errors:** `400` (missing/unknown `file`, or HTTP-headers not CRLF-terminated),
  `500` (write failure).
- **Source:** [`http_server_cb_on_post_extra_cfg()` L232](../main/http_server_cb_on_post.c#L232)
  (dispatch [L358-365](../main/http_server_cb_on_post.c#L358)).

### POST /init_storage

- **Purpose:** check/initialize the gw_cfg storage partition; reboot if it was updated.
- **Query params:** ignored (logged only).
- **Success:** `200 {}`.
- **Side effects:** may schedule a reboot (`reset_task_reboot_after_timeout()`).
- **Source:** [`http_server_cb_on_post_init_storage()` L296](../main/http_server_cb_on_post.c#L296).

### POST /&lt;other&gt;

- Unknown POST path -> `404`
  ([L370](../main/http_server_cb_on_post.c#L370)).

---

## DELETE endpoints

DELETE routes **except `/auth`** require LAN auth with **RW bearer** for API-key auth;
hotspot access is allowed. `/auth` has additional logout-session requirements below,
even from the hotspot. The application-level DELETE handler also returns `409` if a firmware update
is in progress ([cb.c L117](../main/http_server_cb.c#L117)).

### DELETE /auth

- **Purpose:** log out the current interactive session (`RUUVI`/`DEFAULT`).
- **Auth:** first passes the shared LAN check, requiring RW scope when a bearer
  header is supplied; rejected bearer tokens (including RO keys) return `401`
  before logout dispatch. Without a bearer header, the configured scheme applies.
  Once that check succeeds (or is bypassed for hotspot access), the logout handler
  requires `RUUVI`/`DEFAULT`; otherwise it returns `503`. It also requires an
  authorized session cookie bound to the remote IP; missing/invalid session -> `401`.
  Thus an RW bearer key alone is insufficient, and non-`RUUVI`/`DEFAULT` requests
  can be rejected by the shared check before reaching the handler's `503`.
- **Success:** `200 {}` (clears the authorized session).
- **Source:** [handle_req_delete L236](../components/esp32-wifi-manager/src/http_server_handle_req.c#L236)
  -> [`http_server_handle_req_delete_auth()` L11](../components/esp32-wifi-manager/src/http_server_handle_req_delete_auth.c#L11).

### DELETE /connect.json

- **Purpose:** disconnect from Wi-Fi/Ethernet (user-initiated).
- **Auth:** enforced; **LAN forbidden** -> `403`
  ([L243-247](../components/esp32-wifi-manager/src/http_server_handle_req.c#L243)).
- **Success:** `200 {}`.
- **Side effects:** stops DNS server, disables WPS, marks user-disconnect, disconnects
  Wi-Fi/Ethernet.
- **Source:** [handle_req_delete L240-263](../components/esp32-wifi-manager/src/http_server_handle_req.c#L240).

### DELETE /ssl_cert  and  DELETE /extra_cfg

- **Purpose:** delete a stored config file. `ssl_cert` and `extra_cfg` both dispatch to
  the same handler.
- **Query params:** `file=<name>` (URL-decoded, required).
- **Success:** `200 {}`.
- **Errors:** `400` (missing/unknown `file`), `500` (delete failure).
- **Source:** [`http_server_cb_on_delete_extra_cfg()` L76](../main/http_server_cb.c#L76)
  (dispatch [L125-132](../main/http_server_cb.c#L125)).

### DELETE /&lt;other&gt;

- Unknown DELETE path -> `404`
  ([cb.c L133](../main/http_server_cb.c#L133)).

---

## Static web assets and fallback

After the earlier authentication and route checks, any remaining GET path is treated
as a static file request served from the GWUI FATFS partition. Paths ending in
case-sensitive `.json` have already been dispatched to the JSON handler, which
returns `404` for unknown JSON routes; they do not reach static fallback. All other
unmatched paths can reach it, including paths with no dot
([`http_server_resp_file()` L433](../main/http_server_cb_on_get.c#L433), dispatch
at [cb_on_get L587](../main/http_server_cb_on_get.c#L587)):

- Empty path -> `ruuvi.html`; wifi-manager also maps empty path to `index.html` earlier.
- `.js`, `.html`, `.css` are served gzip-encoded (`<file>.gz`) when available.
- Content-Type is derived by extension
  ([`http_get_content_type_by_ext()` L402](../main/http_server_cb_on_get.c#L402)):
  `.html`->text/html, `.css`/`.scss`->text/css, `.js`->text/javascript,
  `.png`->image/png, `.svg`->image/svg+xml, `.ttf`/other->application/octet-stream.
- **Success:** `200` (no-cache) with file bytes.
- **Errors:** `404` (file not found), `503` (partition not mounted / open failure /
  path too long).

This is a single wildcard/static-file handler, **not** a set of distinct API routes:
the served filenames are whatever exists on the GWUI partition (built from
[`../ruuvi.gwui.html/`](../ruuvi.gwui.html/)). It is documented once here rather than
enumerated. The usual dotted asset paths skip authentication, but dot-free paths
require the earlier LAN auth check even when they ultimately reach this file handler
(see [Authentication model](#authentication-model)).

---

## Endpoint index

Real API routes (excludes static-file fallback, captive-portal redirect, and parse
fallbacks):

| Method | Path                           | Auth (LAN)                    | Bearer                     | Source                                                                                         |
|--------|--------------------------------|-------------------------------|----------------------------|------------------------------------------------------------------------------------------------|
| GET    | `/auth`                        | scheme                        | ignored                    | [handle_req_get L115](../components/esp32-wifi-manager/src/http_server_handle_req.c#L115)      |
| GET    | `/ap.json`                     | yes                           | **RW**                     | [L175](../components/esp32-wifi-manager/src/http_server_handle_req.c#L175)                     |
| GET    | `/status.json`                 | yes                           | RO                         | [L187](../components/esp32-wifi-manager/src/http_server_handle_req.c#L187)                     |
| GET    | `/ruuvi.json`                  | yes                           | RO                         | [cb L47](../main/http_server_cb_on_get.c#L47)                                                  |
| GET    | `/firmware_update.json`        | yes                           | RO                         | [cb L69](../main/http_server_cb_on_get.c#L69)                                                  |
| GET    | `/info.json` (hotspot-only)    | yes                           | RO                         | [cb L211](../main/http_server_cb_on_get.c#L211)                                                |
| GET    | `/metrics`                     | yes                           | RO                         | [cb L247](../main/http_server_cb_on_get.c#L247)                                                |
| GET    | `/history`                     | yes                           | RO                         | [cb L322](../main/http_server_cb_on_get.c#L322)                                                |
| GET    | `/validate_url`                | yes                           | RO                         | [cb L568](../main/http_server_cb_on_get.c#L568)                                                |
| GET    | `/extra_cfg`                   | yes                           | RO                         | [cb L500](../main/http_server_cb_on_get.c#L500)                                                |
| POST   | `/auth`                        | interactive login             | ignored                    | [post_auth L248](../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c#L248) |
| POST   | `/connect.json` (hotspot-only) | yes                           | **RW**                     | [L554](../components/esp32-wifi-manager/src/http_server_handle_req.c#L554)                     |
| POST   | `/connect_wps` (hotspot-only)  | yes                           | **RW**                     | [L563](../components/esp32-wifi-manager/src/http_server_handle_req.c#L563)                     |
| POST   | `/ruuvi.json`                  | yes                           | **RW**                     | [cb L44](../main/http_server_cb_on_post.c#L44)                                                 |
| POST   | `/bluetooth_scanning.json`     | yes                           | **RW**                     | [cb L98](../main/http_server_cb_on_post.c#L98)                                                 |
| POST   | `/fw_update.json`              | yes                           | **RW**                     | [cb L129](../main/http_server_cb_on_post.c#L129)                                               |
| POST   | `/fw_update_url.json`          | yes                           | **RW**                     | [cb L167](../main/http_server_cb_on_post.c#L167)                                               |
| POST   | `/fw_update_reset`             | yes                           | **RW**                     | [cb L184](../main/http_server_cb_on_post.c#L184)                                               |
| POST   | `/gw_cfg_download`             | yes                           | **RW**                     | [cb L200](../main/http_server_cb_on_post.c#L200)                                               |
| POST   | `/ssl_cert`                    | yes                           | **RW**                     | [cb L358](../main/http_server_cb_on_post.c#L358)                                               |
| POST   | `/extra_cfg`                   | yes                           | **RW**                     | [cb L362](../main/http_server_cb_on_post.c#L362)                                               |
| POST   | `/init_storage`                | yes                           | **RW**                     | [cb L366](../main/http_server_cb_on_post.c#L366)                                               |
| DELETE | `/auth`                        | shared check + logout session | RW check + cookie required | [logout details](#delete-auth)                                                                 |
| DELETE | `/connect.json` (hotspot-only) | yes                           | **RW**                     | [L240](../components/esp32-wifi-manager/src/http_server_handle_req.c#L240)                     |
| DELETE | `/ssl_cert`                    | yes                           | **RW**                     | [cb L125](../main/http_server_cb.c#L125)                                                       |
| DELETE | `/extra_cfg`                   | yes                           | **RW**                     | [cb L129](../main/http_server_cb.c#L129)                                                       |

Wildcard / fallback handlers (not distinct API routes):

- **Static file fallback** — unmatched GET paths other than `.json` paths, including
  dot-free paths, are looked up in the GWUI FATFS partition after applicable auth
  ([cb_on_get L587](../main/http_server_cb_on_get.c#L587)).
- **Captive-portal redirect** — `302` for hotspot requests whose `Host:` is not the AP
  IP ([serve_handle_req L169](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c#L169)).
- **Auth-cookie redirect** — `302` under `RUUVI`/`DEFAULT` when the shared GET auth
  check returns `401` without a recognized bearer scheme. The check covers paths
  with no dot anywhere or ending in case-sensitive `.json`; `/auth` is handled
  separately. A previous-URL cookie is included except for `/ap.json` and `/status.json`
  ([handle_req_get L153](../components/esp32-wifi-manager/src/http_server_handle_req.c#L153)).
- **Unknown route** — `GET *.json` / `POST *` / `DELETE *` unmatched -> `404`.

### Source files audited

- [`components/esp32-wifi-manager/src/http_server_accept_and_handle_conn.c`](../components/esp32-wifi-manager/src/http_server_accept_and_handle_conn.c)
- [`components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c`](../components/esp32-wifi-manager/src/http_server_netconn_serve_handle_req.c)
- [`components/esp32-wifi-manager/src/http_server_handle_req.c`](../components/esp32-wifi-manager/src/http_server_handle_req.c)
- [`components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c`](../components/esp32-wifi-manager/src/http_server_handle_req_get_auth.c)
- [`components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c`](../components/esp32-wifi-manager/src/http_server_handle_req_post_auth.c)
- [`components/esp32-wifi-manager/src/http_server_handle_req_delete_auth.c`](../components/esp32-wifi-manager/src/http_server_handle_req_delete_auth.c)
- [`components/esp32-wifi-manager/src/http_server_ecdh.c`](../components/esp32-wifi-manager/src/http_server_ecdh.c)
- [`components/esp32-wifi-manager/src/wifi_manager_internal.c`](../components/esp32-wifi-manager/src/wifi_manager_internal.c)
- [`components/esp32-wifi-manager/src/wifi_manager.c`](../components/esp32-wifi-manager/src/wifi_manager.c)
- [`main/http_server_cb_on_get.c`](../main/http_server_cb_on_get.c)
- [`main/http_server_cb_on_post.c`](../main/http_server_cb_on_post.c)
- [`main/http_server_cb.c`](../main/http_server_cb.c)
- [`main/validate_url.c`](../main/validate_url.c)
- [`main/network_subsystem.c`](../main/network_subsystem.c) (callback registration)
