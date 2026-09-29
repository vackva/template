# Transcriber app (plugin + standalone)

Files: [`apps/transcriber/`](../apps/transcriber), [`include/tpl/transcript/`](../include/tpl/transcript),
[`src/transcript/`](../src/transcript), [`test/transcript/`](../test/transcript),
[`cmake/transcript.cmake`](../cmake/transcript.cmake).

A JUCE 9 plugin (VST3, AU) and standalone app that transcribes what passes through it and
keeps every session: sessions on the left, the transcript as a document in the middle. The
speech-to-text backend is `tpl::stt::StreamingTranscriber` ([stt.md](stt.md)).

```sh
just build                      # desktop-debug builds the app (TPL_WITH_APP=ON)
open build/desktop/Debug/apps/transcriber/TplTranscriber_artefacts/Debug/Standalone/Transcriber.app
cmake --preset ci-app && cmake --build --preset ci-app   # release, static, no tests
```

## Threads and data flow

```
audio thread    processBlock: pass audio through (standalone: silence the output)
                  -> mono mix -> SegmentSource::push_audio (resample to 16 kHz, lock-free ring)
stt worker      Silero VAD -> Parakeet -> Segment (text + word times)
service worker  TranscriptionService: pop segments -> TranscriptStore (SQLite) -> listeners
message thread  TranscriberEditor timer (20 Hz): take_events() -> SegmentCache -> TranscriptView
```

The processor owns everything (source, store, service); the editor is a view that may be
closed and reopened. A storage move or model change stops recording, swaps the objects and
waits on an atomic busy count until the audio thread no longer touches the old service.

## Storage

`TranscriptStore`: one SQLite file (`transcripts.db`, WAL mode) per storage folder.

| Table | Holds |
|---|---|
| `sessions` | title, start/end (Unix ms), language, heartbeat |
| `segments` | per session: seq, backend id, start/end ms since session start, text, words (blob), final flag |
| `segments_fts` | FTS5 index of the text, `unicode61 remove_diacritics 2` ("kapitan" finds "Kapitän") |

- One transaction per segment: a crash loses at most the segment being written.
- A recording session writes a heartbeat every minute; at start-up, sessions without a
  heartbeat for 5 minutes are closed at their last segment (crash recovery). Other plugin
  instances' live sessions keep their heartbeat and stay open.
- A session is one Record -> Stop run; after `max_session_hours` (default 24) it continues in a
  new session at a segment boundary.
- A day of speech is about 11k segments and a few MB; the view pages 128 segments at a time
  (`SegmentCache`, 16 pages kept), so memory does not grow with the session.
- Plugin state stores only the viewed session id, never transcript text.

Settings (`settings.json` in the per-user data folder, gear panel): transcript folder (moving
it copies the database; the old file stays as a backup), retention (keep, or delete finished
sessions after N days), session length, model folder. `TPL_DATA_DIR` overrides the data folder
(portable installs, tests).

## UI

| Component | Job |
|---|---|
| `TranscriberEditor` | layout, header (breadcrumb, language, Record/Stop with elapsed time, gear), status banner, dialogs |
| `SessionSidebar` | search field over a list of sessions (live one marked) or search hits with highlighted snippets; right-click: Rename / Export / Delete |
| `TranscriptView` | the document: virtualised paragraphs, follow mode, word selection within a paragraph, context menu |
| `SettingsPanel` | the gear overlay |

`TranscriptView` never lays out more than what is on screen: `RowIndex` (Fenwick tree of
paragraph heights, estimates for paragraphs not yet measured) maps scroll offset <-> paragraph
in O(log n); `ParagraphLayout` wraps a paragraph word by word with the caller's font
measurement and answers which word is under the mouse. Both are JUCE-free and unit-tested.
While at the bottom of a live session the view follows new text; scrolling up stops that and
shows "Jump to live".

Every non-ASCII UI literal goes through `theme::text()` (`juce::String::fromUTF8`):
`juce::String(const char*)` accepts ASCII only and asserts on "…" or "·".

## Backend selection

The processor looks for the models in the Settings model folder, then the system-wide install
(`default_model_dir()`), then — development builds — the repository's `models/`. Without
them it runs `ReplaySegmentSource` (scripted sentences in step with the audio) and says so in
the banner.

## Checks

- `test_transcript`: store, search, export, settings, row index, paragraph layout, segment
  cache, replay source, service (including a simulated 24 h day).
- pluginval strictness 5 on the VST3 (its auval and VST3 validator steps included) passes
  locally; the AU is not installed by the build, so auval on its own is not run.
- CI builds the app on Linux, macOS and Windows (`App-*` rows of `build_test_matrix.json`,
  build only); Linux needs JUCE's ALSA/X11/freetype headers (`apt` in the matrix row).

## Known gaps

- The language setting labels sessions; Parakeet v3 detects the language itself.
- JUCE 9's bundled WebP code exports its symbols from the plugin binary (no `tpl`, ONNX
  Runtime or SQLite symbols are exported).
- The UI has no automated rendering test; the reference screenshots come from running the
  standalone app against a seeded `TPL_DATA_DIR`.
