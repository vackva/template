# Speech-to-text (`tpl_stt`)

Files: [`include/tpl/stt/`](../include/tpl/stt), [`src/stt/`](../src/stt),
[`cmake/stt.cmake`](../cmake/stt.cmake), [`cmake/anira.cmake`](../cmake/anira.cmake),
[`scripts/export-parakeet/`](../scripts/export-parakeet), [`models/`](../models),
[`test/stt/`](../test/stt), [`test/data/stt/`](../test/data/stt),
[`examples/transcribe_file/`](../examples/transcribe_file).

Offline transcription with [nvidia/parakeet-tdt-0.6b-v3](https://huggingface.co/nvidia/parakeet-tdt-0.6b-v3)
(600M parameters, 25 European languages detected automatically, punctuation and
capitalisation, CC BY 4.0) running as int8 ONNX on the CPU. This is the first step towards a
real-time plugin: the next step feeds it from an audio thread through anira, segmented at
pauses by a voice-activity detector (at most 30 s per segment).

```cpp
#include "tpl/stt/Transcriber.h"

tpl::stt::Transcriber transcriber;                   // loads default_model_dir()
auto transcript = transcriber.transcribe(samples);   // 16 kHz mono float
std::puts(transcript.m_text.c_str());                // "mister Quilter is the apostle of ..."
for (const auto& token : transcript.m_tokens) {}     // token id + encoder frame (80 ms each)
```

`Transcriber` loads for about a second and allocates while it transcribes: keep it off the
audio thread. `tpl-transcribe file.wav [--model dir] [--threads n] [--repeat n]` prints the
text and the real-time factor (RTF: compute time / audio duration).

## Pipeline

| Stage | File | What it does |
|---|---|---|
| Frontend | `nemo128.onnx` | 16 kHz audio -> 128-bin log-mel, normalised per feature (from onnx-asr, MIT; matches NeMo's features within 6e-5) |
| Encoder | `encoder-model.int8.onnx` | FastConformer, 8x subsampling: one 1024-dim frame per 80 ms |
| Prediction net + joint | `decoder_joint-model.int8.onnx` | 2-layer LSTM (640) + joint: 8193 token logits (blank = 8192) and 5 duration logits (0-4 frames) |
| Greedy TDT search | `TdtGreedyDecoder.cpp` | argmax token and duration per step, at most 10 tokens per frame |
| Detokenisation | `Vocabulary.cpp` | SentencePiece "▁" -> space, no space before punctuation |

ONNX Runtime comes from anira (`anira::onnxruntime`, 1.26.0), so the offline library and the
streaming path share one runtime per process. anira is a submodule at `third_party/anira`
(v2.3.0): run `git submodule update --init` after cloning.

Measured on an Apple M-series CPU (int8, one thread): RTF 0.063, load 0.8 s. No numbers yet
for low-end Windows machines.

## The model files

`models/parakeet-tdt-0.6b-v3-int8/` holds the export; the `.onnx` files (670 MB) are in Git
LFS, `vocab.txt`, `config.json` and `manifest.json` (sizes, SHA-256, tool versions) are plain
files.

```sh
git lfs install                     # once per machine; installs the LFS pre-push hook
just model                          # git lfs pull for models/ (after a GIT_LFS_SKIP_SMUDGE=1 clone)
```

A model file that is still an LFS pointer makes `Transcriber` throw "... is a Git LFS pointer,
run `git lfs pull`".

Re-export (needs about 8 GB RAM and 4 minutes; downloads the 2.5 GB NeMo checkpoint):

```sh
cd scripts/export-parakeet
uv run export_parakeet.py export ../../models/parakeet-tdt-0.6b-v3-int8
uv run export_parakeet.py golden ../../models/parakeet-tdt-0.6b-v3-int8 ../../test/data/stt
```

The script pins NeMo 2.7.3 and ONNX Runtime 1.26.0 (anira's), exports the encoder and
decoder_joint with `model.export()`, quantises both with `quantize_dynamic` (unsigned 8-bit
weights: no saturation on AVX2 CPUs without VNNI) and copies onnx-asr's `nemo128.onnx`.

## Where the model is installed

`default_model_dir()` returns the system-wide location an installer uses; `cmake --install`
puts it there only on request (it needs admin rights, so the release packages never do):

| OS | Directory |
|---|---|
| macOS | `/Library/Application Support/tpl/models/parakeet-tdt-0.6b-v3-int8` |
| Windows | `C:/ProgramData/tpl/models/parakeet-tdt-0.6b-v3-int8` |
| Linux | `/usr/local/share/tpl/models/parakeet-tdt-0.6b-v3-int8` |

```sh
just install-model                  # sudo cmake --install build/desktop/Debug --component stt_model
```

`TPL_STT_MODEL_DIR` (source) and `TPL_STT_MODEL_INSTALL_DIR` (destination) are CMake cache
variables.

## Tests

`test_stt` (`just test-filter 'Vocabulary|TdtGreedyDecoder|Transcriber'`):

- `test_Vocabulary`, `test_TdtGreedyDecoder`: no model needed; the decoder runs against a
  scripted joint (durations, the 10-symbol limit, state only advanced on emission).
- `test_Transcriber`: the real model on three 16 kHz clips in `test/data/stt/` (LibriSpeech
  and Multilingual LibriSpeech German, CC BY 4.0, see `ATTRIBUTION.md`):
  - word error rate against the human transcript: <= 5 % English, <= 15 % German (the model
    really says "seine Weltschiff" and "Dinsten");
  - against `golden.json` (onnx-asr over the same graphs, ORT 1.26, CPU): at most 5 % of the
    tokens and 10 % of the words may differ. int8 kernels differ between CPU architectures and
    flip borderline tokens, so exact equality would fail across the CI platforms;
  - empty input, digital silence (all zeros -> empty text, as in NeMo: the normalised
    features of a constant signal are all zero and the model hallucinates on them), very short
    input, more than 30 s.

Low-level noise also makes the model hallucinate ("Whoa.", "Yes." at -60/-50 dBFS): the
streaming step gates the model with a voice-activity detector.

## Sanitizers

The suite, inference included, runs in about 10 s under ASan+UBSan and TSan and passes RTSan.
Use upstream LLVM for them: on macOS 26.6 Apple clang 17's ASan and TSan runtimes hang during
start-up in every test binary, `test_dsp` included (`__asan::InitializeShadowMemory` ->
`get_dyld_hdr` -> `dyld_shared_cache_iterate_text_swift`), and
Apple clang has no RTSan. CI uses LLVM 20 on Ubuntu.

```sh
cmake -S . -B build/sanitizers/asan-llvm -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ \
  -DTPL_SANITIZERS="asan;ubsan"
```

## CI and Git LFS bandwidth

`actions/checkout` leaves LFS files as pointers. [`.github/actions/models-cache`](../.github/actions/models-cache/action.yml)
restores `models/*/*.onnx` from the Actions cache, keyed by the hash of
`models/*/manifest.json`. On a miss, the plan job of build_test and sanitizers and the
coverage job run `git lfs pull` and save the cache; the matrix legs only restore it.

GitHub's free plan includes 10 GB of LFS bandwidth a month. A new model version costs about
3 x 670 MB on its pull request (one pull per workflow) and the same again on `main` after the
merge (main cannot read a pull request's caches). Every clone that runs LFS smudge downloads
it too; `GIT_LFS_SKIP_SMUDGE=1 git clone ...` skips that.

## Streaming: `StreamingTranscriber`

`tpl::stt::StreamingTranscriber` implements the `tpl::stt::SegmentSource` hand-off
([`Segment.h`](../include/tpl/stt/Segment.h)) that the transcription app consumes:

| Thread | Work | Real-time safe |
|---|---|---|
| audio | `push_audio()`: host rate -> 16 kHz ([`Resampler`](../include/tpl/stt/Resampler.h), windowed sinc, cut-off 7.6 kHz), write a lock-free [`SpscRing`](../include/tpl/stt/SpscRing.h) (drops when full, never blocks) | yes, `TPL_NONBLOCKING`, checked by RTSan |
| worker (owned) | Silero VAD v6.2.3 per 512 samples -> [`VadSegmenter`](../include/tpl/stt/VadSegmenter.h) (0.5 s pause ends a segment, 30 s maximum, 0.4 s padding) -> Parakeet -> words -> output queue | no |
| any other | `pop_segment()`, `reset()` (flush the open segment, positions restart at 0) | no |

The inference thread does nothing but VAD and inference: resampling happens on the audio
thread. All instances in one process share one Parakeet model (a registry keyed by model
folder), inference serialised on a mutex; the 700 MB load happens once. Models load on the
worker; `state()` reports `Loading` / `Ready` / `Failed`.

Silero VAD (MIT, 2.3 MB) lives in `models/silero-vad-v6.2.3/` (Git LFS) and installs next to
Parakeet (`default_vad_model()`).

Measured on the test clips, 48 kHz host rate, pushed faster than real time: every clip becomes
one segment at the right place and stays within the offline WER bounds. Cutting segments close
to the speech onset and a 7.2 kHz resampler cut-off each cost German words on
`de_mls_4705_13109_000003`; the 0.4 s padding and 7.6 kHz cut-off recover them.

## How the tokens and text are tested

| Test | What it proves |
|---|---|
| `Vocabulary.DecodeMatchesOnnxAsrForTheWholeVocabulary` | `decode()` equals onnx-asr's detokeniser byte for byte on 2721 cases covering every token of the vocabulary (`test/data/stt/detokenize_golden.json`, written by `export_parakeet.py detokenize`), random mixes of the hard ones (bare `▁`, `▁-`, punctuation, special tokens, Cyrillic, Greek) and the real transcripts |
| `Words.InvariantsHoldForEveryReferenceCase` | on the same cases, `words_from_tokens()` gives the text split at its spaces, in order, timed inside the segment |
| `StreamingTranscriberTest.RealTimePathMatchesOfflineExactlyAt16k` / `...At48k` | the samples each segment was transcribed from are bit for bit the input at the reported positions (ring, VAD chunking, trimming, resampling), and text and words equal the offline `Transcriber` on those samples; host blocks of random size (1 - 1023) at 16 kHz, 480 at 48 kHz. Dropping one sample in the worker fails both. |
| `TranscriberTest.MatchesGoldenOnnxAsrOutput` | the offline C++ path against onnx-asr's (at most 5 % of tokens may differ across CPU architectures) |

## Known gaps

- anira provides ONNX Runtime only: resampling, the ring and the worker are this library's own.
  Silero VAD (fixed 512-sample blocks, recurrent state) is the part that maps onto anira's
  `InferenceHandler`; Parakeet's variable-length segments do not without a custom backend.
- The model detects the language itself; the app's EN/DE setting is stored with the session but
  cannot steer Parakeet v3.
- `tpl_stt` is not part of the installed CMake package yet (`cmake/install.cmake` exports
  `tpl::dsp` only).
- anira and the tanh-lib it fetches carry tanh-tooling 0.1.5 CMake modules against this repo's
  0.2.8. `cmake/anira.cmake` adds anira before the repo's modules so each side runs its own
  version; configure warns until anira pins the same tag.
- onnx-asr 0.12.0 excludes ONNX Runtime 1.26.0 without a stated reason; the export script
  overrides that pin because 1.26.0 is what anira ships.
