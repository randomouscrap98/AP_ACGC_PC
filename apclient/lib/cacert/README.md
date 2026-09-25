# cacert.pem

CA certificate bundle used by apclientpp/wswrap to verify `wss://` servers.

- Source: https://curl.se/ca/cacert.pem (extracted from Mozilla's CA list by curl)
- License: MPL 2.0 (see `LICENSE-MPL-2.0.txt`)
- Not stored in the repo: CMake downloads it at configure time, verifies the
  SHA256 published at https://curl.se/ca/cacert.pem.sha256, and places it at
  `bin/cacert.pem` next to the executable. Re-run cmake to refresh it.
