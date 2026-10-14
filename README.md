# EVS RTP Capture Analyzer and Browser Decoder

A web based tool for inspecting EVS Primary RTP streams in PCAP/PCAPNG captures, reconstructing EVS codec frames, decoding them with an Emscripten/WebAssembly build of the reference EVS decoder, and listening to individual streams or a combined conversation.

The application is intended for **EVS streams only**

## How packet parsing works

1. **Capture container**
   - Classic PCAP: the parser reads the file magic, byte order, timestamp precision, link type, and packet records.
   - PCAPNG: the parser walks blocks, tracks interface metadata and byte order, and extracts enhanced/simple packet records supported by the implementation.
2. **EVS-only RTP admission**
   - Only packets mapped to EVS by SDP are admitted to RTP stream grouping and subsequent EVS analysis.
3. **RTP stream analysis**
   - Admitted packets are grouped into streams using RTP stream metadata, including SSRC and endpoints.
   - Sequence numbers are used to identify duplicate packets and order packets, accounting for 16-bit sequence-number wraparound.
   - RTP timestamps help infer frame progression. The current implementation uses a 20 ms frame-timestamp step (320 ticks at a 16 kHz RTP clock) when inserting `SPEECH_LOST` filler records. This is an implementation assumption; review it when working with captures whose negotiated RTP clock or packetization differs.
4. **EVS payload parsing**
   - Payloads are parsed as Compact or Header-Full EVS framing, subject to SDP's EVS Header-Full-only indication when present.
   - Frame type tables distinguish EVS Primary rates, EVS AMR-WB IO rates, SID, `SPEECH_LOST`, `NO_DATA`, and reserved frame types.
   - Malformed, truncated, or unrecognized payloads are recorded as parse errors/empty frame results instead of being treated as another codec.
5. **EVS decoder input**
   - Reconstructed frame records are serialized into the EVS storage format expected by the bundled reference decoder: the `#!EVS_MC1.0\n` magic/header, channel count, then ToC and frame data records.
   - This intermediate `.evs` data is built in memory. It is not the original RTP payload concatenated or renamed.
6. **Decode and playback**
   - The Emscripten-generated JavaScript glue loads the WebAssembly decoder in the browser.
   - The decoder produces PCM; the app wraps PCM in a WAV container for playback and download.
   - Selected decoded streams can be aligned by capture timestamps and mixed into a conversation WAV. Audio activity statistics are calculated from the mixed PCM.

## Understanding the displayed EVS modes

The mode list is derived from reconstructed frame records, not directly from packet count.

- **EVS Primary**: EVS-native coding modes. The rate shown is the codec's nominal source bit rate, not the RTP packet size or the full network bit rate.
- **SID**: a silence descriptor used with discontinuous transmission (DTX).
- **SPEECH_LOST**: a concealment/filler record inserted by the tool when it infers a missing media frame from packet/timestamp progression. It is not a received speech frame.
- **NO_DATA**: a frame record with no speech data.
- **Reserved**: a frame type reserved for future use; it is not a normal speech mode.

A single RTP packet can contain multiple codec frames, so frame counts and packet counts are different measurements.


## Building the WebAssembly decoder

The HTML application and the codec decoder are separate parts:

- `index.html` contains the UI, capture parsing, SDP/RTP correlation, EVS frame reconstruction, WAV generation, and browser-side decoder integration.
- The EVS reference decoder C sources are compiled to WebAssembly with **Emscripten**.
- The Emscripten build emits `EVS_dec.js` and `EVS_dec.wasm`, which the static page loads at runtime.

## Important limitations and interpretation notes

- RTP payload parsing is not a substitute for a complete protocol analyzer. Unsupported encapsulations, malformed packets, unusual link types, or missing fragments may prevent stream identification.
- Packet loss is estimated from observed RTP sequence/timestamp progression. Capture drops, reordering, retransmission, timestamp behavior, and packetization can affect the estimate.
- The displayed mean packet interval uses capture timestamps; it is not the same as the codec frame duration or negotiated `ptime`.
- The inferred output sample rate is a convenience estimate when SDP bandwidth information is not available; verify it for unusual EVS configurations.
- The browser performs parsing and decoding locally. Large captures can consume significant memory and CPU.
