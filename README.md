# EVS RTP Capture Analyzer and Browser Decoder

A static, browser-based tool for inspecting EVS RTP streams in PCAP/PCAPNG captures, reconstructing EVS codec frames, decoding them with an Emscripten/WebAssembly build of the reference EVS decoder, and listening to individual streams or a combined conversation.

The application is intended for **EVS streams identified through SIP/SDP negotiation**. It is not a general-purpose multi-codec RTP player.

## Features

- Load a `.pcap` or `.pcapng` capture in the browser.
- Parse packet records and decode supported link/network/transport headers.
- Reassemble fragmented IP traffic and TCP streams where supported, then extract SIP messages and SDP.
- Identify RTP streams whose payload type and media endpoint match an SDP-negotiated EVS codec.
- Exclude non-EVS and unknown/unmatched RTP streams from EVS payload parsing.
- Inspect SSRC, payload type, endpoints, packet/loss estimates, sequence and RTP timestamp ranges, payload sizes, and capture-time packet intervals.
- Parse EVS RTP payloads in Compact and Header-Full formats, reconstruct frame records, and infer missing frames for sequence/timestamp gaps.
- Decode per-stream EVS input to PCM/WAV with the WebAssembly decoder.
- Select streams by SSRC, mix selected streams on a capture-time-aligned timeline, view audio activity, and download WAV output.

## How packet parsing works

The parsing path is intentionally staged. Codec identity is established before EVS frame parsing.

1. **Capture container**
   - Classic PCAP: the parser reads the file magic, byte order, timestamp precision, link type, and packet records.
   - PCAPNG: the parser walks blocks, tracks interface metadata and byte order, and extracts enhanced/simple packet records supported by the implementation.
2. **Network and transport decoding**
   - Captured packet bytes are decoded into packet metadata, including timestamps and transport endpoints.
   - IP fragments are reassembled where supported. TCP byte streams are reassembled for SIP-over-TCP message extraction.
3. **SIP and SDP extraction**
   - SIP messages are collected from UDP and reassembled TCP streams.
   - SDP media descriptions provide media addresses/ports and RTP payload-type-to-codec mappings.
4. **EVS-only RTP admission**
   - RTP packets are matched to SDP media endpoints and payload types.
   - Only packets mapped to EVS by SDP are admitted to RTP stream grouping and subsequent EVS analysis.
   - AMR, AMR-WB, G.711, unknown payload types, and streams without a usable SDP match are not guessed to be EVS from payload length alone.
   - If SIP/SDP is missing or incomplete, an RTP stream may therefore be omitted rather than incorrectly decoded.
5. **RTP stream analysis**
   - Admitted packets are grouped into streams using RTP stream metadata, including SSRC and endpoints.
   - Sequence numbers are used to identify duplicate packets and order packets, accounting for 16-bit sequence-number wraparound.
   - RTP timestamps help infer frame progression. The current implementation uses a 20 ms frame-timestamp step (320 ticks at a 16 kHz RTP clock) when inserting `SPEECH_LOST` filler records. This is an implementation assumption; review it when working with captures whose negotiated RTP clock or packetization differs.
6. **EVS payload parsing**
   - Payloads are parsed as Compact or Header-Full EVS framing, subject to SDP's EVS Header-Full-only indication when present.
   - Frame type tables distinguish EVS Primary rates, EVS AMR-WB IO rates, SID, `SPEECH_LOST`, `NO_DATA`, and reserved frame types.
   - Malformed, truncated, or unrecognized payloads are recorded as parse errors/empty frame results instead of being treated as another codec.
7. **EVS decoder input**
   - Reconstructed frame records are serialized into the EVS storage format expected by the bundled reference decoder: the `#!EVS_MC1.0\n` magic/header, channel count, then ToC and frame data records.
   - This intermediate `.evs` data is built in memory. It is not the original RTP payload concatenated or renamed.
8. **Decode and playback**
   - The Emscripten-generated JavaScript glue loads the WebAssembly decoder in the browser.
   - The decoder produces PCM; the app wraps PCM in a WAV container for playback and download.
   - Selected decoded streams can be aligned by capture timestamps and mixed into a conversation WAV. Audio activity statistics are calculated from the mixed PCM.

## Understanding the displayed EVS modes

The mode list is derived from reconstructed frame records, not directly from packet count.

- **EVS Primary**: EVS-native coding modes. The rate shown is the codec's nominal source bit rate, not the RTP packet size or the full network bit rate.
- **EVS AMR-WB IO**: EVS interoperable-operation modes intended for interoperability with AMR-WB. These remain EVS modes when the SDP mapping identifies the payload as EVS.
- **SID**: a silence descriptor used with discontinuous transmission (DTX).
- **SPEECH_LOST**: a concealment/filler record inserted by the tool when it infers a missing media frame from packet/timestamp progression. It is not a received speech frame.
- **NO_DATA**: a frame record with no speech data.
- **Reserved**: a frame type reserved for future use; it is not a normal speech mode.

A single RTP packet can contain multiple codec frames, so frame counts and packet counts are different measurements.

## Running the static site

The site does not require a server-side application or backend API. It runs in a modern browser and fetches the decoder assets from the same hosting location.

1. Keep these files together in the published site directory:
   - `index.html`
   - `EVS_dec.js` — Emscripten JavaScript glue generated for the decoder
   - `EVS_dec.wasm` — compiled WebAssembly module
2. Publish the directory with GitHub Pages or another static web host.
3. Open the published page over HTTP(S), select a PCAP/PCAPNG file, and wait for parsing to complete.
4. Select one or more EVS streams and use the stream conversion or conversation-mix controls.

The app fetches `EVS_dec.js` by that filename. If the file is absent, renamed, or blocked by hosting/path configuration, decoder initialization will fail. The JavaScript glue must reference the matching `EVS_dec.wasm` file.

## Building the WebAssembly decoder

The HTML application and the codec decoder are separate parts:

- `index.html` contains the UI, capture parsing, SDP/RTP correlation, EVS frame reconstruction, WAV generation, and browser-side decoder integration.
- The EVS reference decoder C sources are compiled to WebAssembly with **Emscripten**.
- The Emscripten build emits `EVS_dec.js` and `EVS_dec.wasm`, which the static page loads at runtime.

Build outline:

1. Obtain the compatible EVS reference decoder source and its build files.
2. Install and activate the Emscripten SDK (`emcc`, `emcmake`, and `emmake` as needed).
3. Configure the decoder build for WebAssembly and ensure its command-line entry point can read an input file and write decoded PCM using the Emscripten virtual filesystem. The browser integration expects a decoder entry point compatible with the bundled `EVS_dec.js` interface.
4. Link the required decoder sources/libraries and runtime filesystem support; emit the JavaScript glue named `EVS_dec.js` and the module named `EVS_dec.wasm`.
5. Copy both generated artifacts beside `index.html`, deploy, then verify decoder initialization and a known-good EVS sample before testing PCAP-derived streams.

**Build reproducibility note:** Emscripten flags depend on the exact EVS source release, entry point, and build system. Use the compile/link settings from the compatible decoder build rather than copying generic flags blindly. The JavaScript wrapper and C/WASM binary must be built as a matching pair.

## Important limitations and interpretation notes

- EVS is accepted based on SDP codec mapping; captures without sufficient SIP/SDP may not expose streams for decoding.
- RTP payload parsing is not a substitute for a complete protocol analyzer. Unsupported encapsulations, malformed packets, unusual link types, or missing fragments may prevent stream identification.
- Packet loss is estimated from observed RTP sequence/timestamp progression. Capture drops, reordering, retransmission, timestamp behavior, and packetization can affect the estimate.
- The displayed mean packet interval uses capture timestamps; it is not the same as the codec frame duration or negotiated `ptime`.
- The inferred output sample rate is a convenience estimate when SDP bandwidth information is not available; verify it for unusual EVS configurations.
- The browser performs parsing and decoding locally. Large captures can consume significant memory and CPU.
- Test changes against representative captures before relying on the output for production or forensic conclusions.

## Standards and implementation references

- 3GPP TS 26.445 — EVS codec general overview and RTP payload framing references.
- 3GPP TS 26.443 — EVS codec ANSI-C reference implementation.
- RFC 3264 / RFC 4566 — SDP offer/answer and SDP format context.
- RFC 3550 — RTP: A Transport Protocol for Real-Time Applications.
- Emscripten documentation — compiling C/C++ programs to WebAssembly and JavaScript.

This README describes the current implementation and its intended workflow; it is not a claim of complete conformance to every PCAP, RTP, SDP, or EVS deployment variant.
