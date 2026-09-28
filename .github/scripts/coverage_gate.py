"""Per-file line-coverage gate over an lcov report.

Every changed source file inside SCOPE must reach MIN percent line coverage.
A changed .cpp that is missing from the report fails as well: it is not built
into the coverage leg, so nothing measures it. A changed header missing from the
report is skipped (no executable lines, e.g. declarations and macros only).

Environment:
  MIN    minimum line coverage in percent, e.g. "80"
  SCOPE  extended regex over repo-relative paths, e.g. "^(src|include)/"
  FILES  newline-separated changed paths (from the changed-files action)
  SWEEP  "true" -> gate every in-scope file of the report instead of FILES
Usage: coverage_gate.py <coverage.lcov>
"""

import os
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}


def read_lcov(path: str, root: str) -> dict[str, tuple[int, int]]:
    """Returns {repo-relative path: (lines found, lines hit)}."""
    report: dict[str, list[int]] = {}
    current = None
    for raw in Path(path).read_text().splitlines():
        line = raw.strip()
        if line.startswith("SF:"):
            source = line[3:]
            current = source[len(root) :] if source.startswith(root) else source
            report.setdefault(current, [0, 0])
        elif current and line.startswith("LF:"):
            report[current][0] += int(line[3:])
        elif current and line.startswith("LH:"):
            report[current][1] += int(line[3:])
        elif line == "end_of_record":
            current = None
    return {k: (v[0], v[1]) for k, v in report.items()}


def main() -> int:
    minimum = float(os.environ["MIN"])
    scope = re.compile(os.environ.get("SCOPE", "^(src|include)/"))
    root = os.environ.get("GITHUB_WORKSPACE", os.getcwd()).rstrip("/") + "/"
    report = read_lcov(sys.argv[1], root)

    if os.environ.get("SWEEP") == "true":
        candidates = sorted(f for f in report if scope.search(f))
        print("changed files unavailable - gating every in-scope file of the report")
    else:
        candidates = sorted(
            f
            for f in os.environ.get("FILES", "").split()
            if scope.search(f) and Path(f).suffix in SOURCE_SUFFIXES and Path(f).is_file()
        )

    rows, failures = [], []
    for f in candidates:
        if f not in report:
            if Path(f).suffix in {".h", ".hpp", ".hh"}:
                rows.append((f, "-", "skipped (no executable lines)"))
            else:
                rows.append((f, "-", "FAIL: not in the coverage report (not built?)"))
                failures.append(f)
            continue
        found, hit = report[f]
        if found == 0:
            rows.append((f, "-", "skipped (no executable lines)"))
            continue
        percent = 100.0 * hit / found
        ok = percent >= minimum
        rows.append((f, f"{percent:.1f}% ({hit}/{found})", "ok" if ok else f"FAIL: below {minimum:g}%"))
        if not ok:
            failures.append(f)

    lines = [
        f"### Per-file coverage gate (min {minimum:g}%)",
        "",
        "| file | line coverage | result |",
        "|---|---|---|",
        *[f"| `{f}` | {c} | {r} |" for f, c, r in rows],
    ]
    if not rows:
        lines.append("| - | - | no changed source files in scope |")
    summary = "\n".join(lines)
    print(summary)
    if "GITHUB_STEP_SUMMARY" in os.environ:
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as out:
            out.write(summary + "\n")
    for f in failures:
        print(f"::error file={f}::line coverage below {minimum:g}% (or not measured)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
