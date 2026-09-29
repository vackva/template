# Silero VAD v6.2.3

`silero_vad.onnx` from <https://github.com/snakers4/silero-vad> at tag `v6.2.3`
(`src/silero_vad/data/silero_vad.onnx`, SHA-256
`1a153a22f4509e292a94e67d6f9b85e8deb25b4988682b7e174c65279d8788e3`), MIT licence (`LICENSE`).

Inputs: `input` float [1, 576] (64 samples of context + 512 new samples at 16 kHz),
`state` float [2, 1, 128], `sr` int64 scalar (16000). Outputs: `output` [1, 1] speech
probability, `stateN` [2, 1, 128].
