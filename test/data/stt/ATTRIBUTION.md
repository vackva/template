# Test clips

16 kHz mono 16-bit PCM excerpts used by the `tpl_stt` tests. Reference transcripts are in
`clips.json`; `golden.json` is the output of the exported model under onnx-asr
(`scripts/export-parakeet/export_parakeet.py golden`).

| File | Source | Licence |
|---|---|---|
| `en_librispeech_1272-128104-0000.wav` | LibriSpeech dev-clean, utterance `1272-128104-0000` (V. Panayotov, G. Chen, D. Povey, S. Khudanpur), via `hf-internal-testing/librispeech_asr_dummy`; FLAC converted to WAV, audio unchanged | CC BY 4.0 |
| `de_mls_4705_13109_000001.wav` | Multilingual LibriSpeech, German test split, utterance `4705_13109_000001` (V. Pratap et al.), via `facebook/multilingual_librispeech`; decoded from Opus and resampled to 16 kHz | CC BY 4.0 |
| `de_mls_4705_13109_000003.wav` | Multilingual LibriSpeech, German test split, utterance `4705_13109_000003`; same processing | CC BY 4.0 |

Licence text: <https://creativecommons.org/licenses/by/4.0/>. The underlying recordings are
public-domain LibriVox audiobooks.
