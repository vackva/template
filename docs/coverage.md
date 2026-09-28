# Coverage and Codecov

Files: preset `ci-tests-coverage`, [`codecov.yml`](../codecov.yml),
[`.github/workflows/coverage.yml`](../.github/workflows/coverage.yml),
[`.github/workflows/reusable-coverage.yml`](../.github/workflows/reusable-coverage.yml),
[`.github/scripts/coverage_gate.py`](../.github/scripts/coverage_gate.py).

## How it is measured

Clang source-based coverage: the `ci-tests-coverage` preset compiles with
`-fprofile-instr-generate -fcoverage-mapping`, every test process writes a `.profraw`, and
llvm-cov maps the merged profile back to source lines.

```sh
just coverage     # build, run the suite, print a per-file table
```

In CI (`reusable-coverage.yml`):

1. Build and run `ctest` with `LLVM_PROFILE_FILE=profraw/%p.profraw` (one file per process).
2. `llvm-profdata merge` the valid profiles (one broken file from a crashed process is tolerated).
3. `llvm-cov export -format=lcov` over `libtpl_dsp.so*` and the `test_*` binaries, ignoring
   `build/`, `test/` and fetched code.
4. Upload `coverage.lcov` to Codecov (tokenless OIDC, needs the Codecov GitHub App on the repo).
5. On pull requests: the per-file gate.

`llvm-profdata` and `llvm-cov` must be the same LLVM version as the compiler; the workflow
installs the matching `llvm-<N>`.

## The per-file gate (blocks the merge)

On a pull request, every changed file under `src/` or `include/` needs at least **80% line
coverage**:

```yaml
# coverage.yml
with:
  preset: ci-tests-coverage
  lib_pattern: "libtpl_dsp.so*"
  min_file_coverage: "80"
```

- The changed files come from git (`changed-files` action), no token.
- A changed `.cpp` that is not in the report fails: it is not built, so nothing measures it.
- A changed header without executable lines (declarations, macros) is skipped.
- The result table is in the job summary; failing files are annotated on the PR.

It fails the `coverage result` check, which `main` requires ([ci.md](ci.md#branch-protection)).

## Codecov (reports)

```yaml
coverage:
  status:
    project: off              # overall % is not a check
    patch:
      default:
        target: 80%           # all changed lines of the PR together
comment:
  layout: "diff, files"       # PR comment with the per-file numbers
github_checks:
  annotations: true           # uncovered changed lines marked in the diff
```

Codecov's patch status looks at all changed lines together; it cannot demand 80% per file.
That is why the hard rule is the CI gate above and Codecov is the view on top.
