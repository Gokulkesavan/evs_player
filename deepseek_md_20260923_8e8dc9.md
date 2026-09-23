# EVS PCAP → WASM Decoder → WAV

## Files
- index.html          UI (drop a .pcap, get playable WAV)
- evs_dec.js          Emscripten glue  ← built by build.sh
- evs_dec.wasm        Compiled decoder ← built by build.sh
- wasm_main.c         C glue (added to lib_dec/)
- build.sh            One-shot build
- serve.sh            Local HTTP server

## Prerequisites
- emsdk ≥ 3.1 (with emcc on PATH or EMSDK env var)
- The unzipped 3GPP TS 26.443 c-code tree at ./c-code
  (you must obtain this yourself — it is licence-restricted)

## One-time build
1.  ./build.sh                  # produces evs_dec.js + evs_dec.wasm
2.  ./serve.sh                  # http://localhost:8080
3.  open http://localhost:8080/index.html
    → header badge should read "WASM Ready"

## If your TS 26.443 release uses different decoder API names
Edit the four `EVS_DEC_*` lines at the top of wasm_main.c, then re-run
./build.sh. Everything else is release-agnostic.

## If the runtime refuses a second callMain()
Already handled — wasm_main.c calls emscripten_exit_with_live_runtime()
and index.html prefers ccall('evs_mime_decode', ...) over callMain().

## Troubleshooting
- "WASM Missing" persists → check browser console for the wasm fetch 404.
  Ensure you serve over http://, not file://.
- Empty / silent WAV → confirm your SDP advertised `bw=` (else the page
  guesses 16 kHz from observed frame sizes).
- Garbled audio → the RTP payload is likely not RFC 8130 framed; check the
  "framing" column in the RTP stream card.
- Symbol errors during build → add another `-I` for the header directory,
  or delete `-Wno-everything` to see which symbol is missing.