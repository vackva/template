set shell := ["bash", "-cu"]

preset := "desktop-debug"

default:
    @just --list

# Arm the pre-push hook and configure the dev build (compile DB for clangd + Claude hooks)
setup:
    ln -sf ../../hooks/tanh/pre-push .git/hooks/pre-push
    cmake --preset {{preset}}

# Configure + build the dev preset
build:
    cmake --preset {{preset}} > /dev/null
    cmake --build --preset {{preset}}

# Build + run every test
test: build
    ctest --preset {{preset}}

# Run tests whose name matches PATTERN (regex, e.g. 'Gain\.')
test-filter PATTERN: build
    ctest --preset {{preset}} -R '{{PATTERN}}'

# Build + test under a sanitizer: asan (with ubsan) | tsan | rtsan (needs Clang >= 20)
sanitize KIND="asan":
    cmake --preset desktop-tests-{{KIND}}
    cmake --build --preset desktop-tests-{{KIND}}
    ctest --preset desktop-tests-{{KIND}}

# Instrumented build, run the suite, print a per-file coverage report
coverage:
    cmake --preset ci-tests-coverage > /dev/null
    cmake --build --preset ci-tests-coverage
    rm -rf build/ci/coverage/profraw
    LLVM_PROFILE_FILE="$PWD/build/ci/coverage/profraw/%p.profraw" ctest --preset ci-tests-coverage
    xcrun_or() { if command -v xcrun > /dev/null; then xcrun "$@"; else "$@"; fi; }; \
      xcrun_or llvm-profdata merge -sparse build/ci/coverage/profraw/*.profraw -o build/ci/coverage/merged.profdata && \
      xcrun_or llvm-cov report -instr-profile build/ci/coverage/merged.profdata \
        -ignore-filename-regex='(^|/)(build|test|_deps)/' \
        $(find build/ci/coverage -maxdepth 1 -name 'libtpl_dsp.*' -type f | head -1) \
        -object build/ci/coverage/test/test_dsp

# clang-format in place
format:
    find src include test -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# clang-format check (what CI's lint job runs)
format-check:
    find src include test -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror

# clang-tidy, warnings as errors (what CI's clang_tidy job runs)
tidy: build
    find src test -name '*.cpp' | xargs -P "$(nproc 2>/dev/null || sysctl -n hw.ncpu)" -n 1 \
        clang-tidy -p build/desktop/Debug --warnings-as-errors='*'

# Fetch the speech-to-text model from Git LFS (after a GIT_LFS_SKIP_SMUDGE=1 clone)
model:
    git lfs pull --include "models/**/*.onnx"

# Install the model to the system-wide location default_model_dir() reads (needs sudo)
install-model: build
    sudo cmake --install build/desktop/Debug --component stt_model

# Reinstall the tooling copies (.clang-*, cmake/tanh/, hooks/tanh/) from tooling/
tooling-install:
    sh tooling/install.sh clang cmake hooks

# Fail if an installed tooling copy differs from tooling/
tooling-check:
    sh tooling/install.sh --check clang cmake hooks

# Run the tests of the CMake modules in tooling/cmake
tooling-test:
    cmake -S tooling/cmake/test -B build/tooling-test
    ctest --test-dir build/tooling-test -C Release --output-on-failure

# Remove every build directory
clean:
    rm -rf build

# Create or update the main-branch ruleset on GitHub from .github/rulesets/main.json (needs admin)
protect-main REPO:
    id=$(gh api "repos/{{REPO}}/rulesets" -q '.[] | select(.name=="main: pull requests only + required checks") | .id' | head -1); \
    if [ -n "$id" ]; then gh api -X PUT "repos/{{REPO}}/rulesets/$id" --input .github/rulesets/main.json -q '"updated ruleset " + (.id|tostring)'; \
    else gh api -X POST "repos/{{REPO}}/rulesets" --input .github/rulesets/main.json -q '"created ruleset " + (.id|tostring)'; fi
