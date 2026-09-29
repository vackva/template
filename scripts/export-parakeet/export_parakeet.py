"""Export nvidia/parakeet-tdt-0.6b-v3 to int8 ONNX for tpl_stt.

    uv run export_parakeet.py export <model_dir>          # NeMo -> ONNX -> int8, vocab, manifest
    uv run export_parakeet.py golden <model_dir> <clips>  # reference transcripts via onnx-asr

The model directory holds exactly what tpl::stt::Transcriber loads:

    nemo128.onnx                    log-mel frontend (onnx-asr, MIT)
    encoder-model.int8.onnx         FastConformer encoder
    decoder_joint-model.int8.onnx   prediction net + joint (TDT: 8193 token + 5 duration logits)
    vocab.txt                       "<token> <id>" per line, blank last
    config.json                     onnx-asr model config, so onnx-asr can load the same files
    manifest.json                   sizes, SHA-256 and the tool versions that produced them

`golden` transcribes every clip listed in <clips>/clips.json with onnx-asr — an independent
Python implementation of the TDT decoding over the same ONNX graphs, on the CPU provider and
the ONNX Runtime version anira ships — and writes <clips>/golden.json. The C++ integration
test compares its own output against that file with a small tolerance: int8 kernels differ
between CPU architectures, which can flip a borderline token.
"""

import argparse
import hashlib
import json
import shutil
import sys
import tempfile
from importlib import metadata, resources
from pathlib import Path

MODEL_ID = "nvidia/parakeet-tdt-0.6b-v3"
ONNX_ASR_MODEL = "nemo-parakeet-tdt-0.6b-v3"
MODEL_FILES = [
    "nemo128.onnx",
    "encoder-model.int8.onnx",
    "decoder_joint-model.int8.onnx",
    "vocab.txt",
    "config.json",
]
ENCODER_FRAME_SECONDS = 0.08  # 10 ms feature hop x subsampling factor 8
ONNX_ASR_CONFIG = {
    "model_type": "nemo-conformer-tdt",
    "features_size": 128,
    "subsampling_factor": 8,
    "max_tokens_per_step": 10,
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def export_fp32(work_dir: Path) -> None:
    import nemo.collections.asr as nemo_asr  # noqa: PLC0415 - heavy import, only for `export`

    model = nemo_asr.models.ASRModel.from_pretrained(MODEL_ID, map_location="cpu")
    model.eval()
    # Writes encoder-model.onnx (+ external weights) and decoder_joint-model.onnx.
    model.export(str(work_dir / "model.onnx"))

    with (work_dir / "vocab.txt").open("w", encoding="utf-8") as f:
        for token_id, token in enumerate([*model.tokenizer.vocab, "<blk>"]):
            f.write(f"{token} {token_id}\n")


def quantize(src: Path, dst: Path) -> None:
    from onnxruntime.quantization import QuantType, quantize_dynamic  # noqa: PLC0415

    # Unsigned 8-bit weights: no VPMADDUBSW saturation on AVX2 CPUs without VNNI.
    quantize_dynamic(src, dst, weight_type=QuantType.QUInt8)


def copy_frontend(model_dir: Path) -> None:
    source = resources.files("onnx_asr.preprocessors").joinpath("data", "nemo128.onnx")
    with resources.as_file(source) as path:
        shutil.copyfile(path, model_dir / "nemo128.onnx")


def write_manifest(model_dir: Path) -> None:
    manifest = {
        "source_model": MODEL_ID,
        "source_license": "CC-BY-4.0",
        "frontend_license": "MIT (onnx-asr, Ilya Stupakov)",
        "tools": {
            name: metadata.version(name) for name in ["nemo_toolkit", "onnx", "onnxruntime", "onnx-asr"]
        },
        "files": {
            name: {"bytes": (model_dir / name).stat().st_size, "sha256": sha256(model_dir / name)}
            for name in MODEL_FILES
        },
    }
    (model_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def cmd_export(args: argparse.Namespace) -> None:
    model_dir: Path = args.model_dir
    model_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="parakeet-export-") as tmp:
        work_dir = Path(tmp)
        export_fp32(work_dir)
        quantize(work_dir / "encoder-model.onnx", model_dir / "encoder-model.int8.onnx")
        quantize(work_dir / "decoder_joint-model.onnx", model_dir / "decoder_joint-model.int8.onnx")
        shutil.copyfile(work_dir / "vocab.txt", model_dir / "vocab.txt")

    copy_frontend(model_dir)
    (model_dir / "config.json").write_text(json.dumps(ONNX_ASR_CONFIG, indent=2) + "\n", encoding="utf-8")
    write_manifest(model_dir)
    print(f"exported {MODEL_ID} to {model_dir}")


def cmd_golden(args: argparse.Namespace) -> None:
    import onnx_asr  # noqa: PLC0415
    import soundfile  # noqa: PLC0415

    clips_dir: Path = args.clips_dir
    clips = json.loads((clips_dir / "clips.json").read_text(encoding="utf-8"))
    # CPU only (onnx-asr would pick CoreML on macOS) and the exported nemo128.onnx frontend
    # (onnx-asr defaults to a NumPy one on CPU) — the same graphs tpl::stt::Transcriber runs.
    model = onnx_asr.load_model(
        ONNX_ASR_MODEL,
        args.model_dir,
        quantization="int8",
        providers=["CPUExecutionProvider"],
        preprocessor_config={"use_numpy_preprocessors": False},
    ).with_timestamps()

    # onnx-asr reports token strings with "▁" already turned into spaces; map them back to ids.
    token_ids: dict[str, int] = {}
    for line in (args.model_dir / "vocab.txt").read_text(encoding="utf-8").splitlines():
        token, token_id = line.rsplit(" ", 1)
        token_ids.setdefault(token.replace("▁", " "), int(token_id))

    golden = []
    for clip in clips:
        samples, sample_rate = soundfile.read(clips_dir / clip["file"], dtype="float32")
        if sample_rate != 16_000 or samples.ndim != 1:
            sys.exit(f"{clip['file']}: expected 16 kHz mono, got {sample_rate} Hz, shape {samples.shape}")
        result = model.recognize(samples, sample_rate=sample_rate)
        golden.append(
            {
                "file": clip["file"],
                "text": result.text,
                "token_ids": [token_ids[token] for token in result.tokens],
                # Encoder frames (80 ms each) at which each token was emitted.
                "frames": [round(seconds / ENCODER_FRAME_SECONDS) for seconds in result.timestamps],
            }
        )
        print(f"{clip['file']}: {result.text}")

    (clips_dir / "golden.json").write_text(json.dumps(golden, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    export = sub.add_parser("export", help="export the int8 ONNX model directory")
    export.add_argument("model_dir", type=Path)
    export.set_defaults(func=cmd_export)

    golden = sub.add_parser("golden", help="write golden.json for the test clips")
    golden.add_argument("model_dir", type=Path)
    golden.add_argument("clips_dir", type=Path)
    golden.set_defaults(func=cmd_golden)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
