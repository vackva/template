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
Use upstream LLVM for them: Apple clang's ASan runtime deadlocks during start-up once the
ONNX Runtime dylib is mapped (`__asan::InitializeShadowMemory` spinning on its own lock), and
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

## Known gaps

- `tpl_stt` is not part of the installed CMake package yet (`cmake/install.cmake` exports
  `tpl::dsp` only).
- anira and the tanh-lib it fetches carry tanh-tooling 0.1.5 CMake modules against this repo's
  0.2.8. `cmake/anira.cmake` adds anira before the repo's modules so each side runs its own
  version; configure warns until anira pins the same tag.
- onnx-asr 0.12.0 excludes ONNX Runtime 1.26.0 without a stated reason; the export script
  overrides that pin because 1.26.0 is what anira ships.
