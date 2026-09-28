# Agentic C++ template

An example C++20 repo set up for developing with Claude Code: CLAUDE.md files, hooks, a plugin,
CMake presets, GoogleTest, sanitizers, clang-format/clang-tidy, CI and Codecov. It follows the
setup of tanh-lab's projects ([anira](https://github.com/anira-project/anira), tanh-lib,
cosmos), with everything in this one repo: [tanh-tooling](https://github.com/tanh-lab/tanh-tooling)
is in `tooling/`, [ci-actions](https://github.com/tanh-lab/ci-actions) in `.github/`.

The library is small on purpose: `tpl::dsp::Gain` and `tpl::dsp::OnePoleLowpass`, both real-time safe.

## Quick start

```sh
brew install cmake ninja llvm just jq    # or the apt equivalents
just setup                               # arms the pre-push hook, configures build/desktop/Debug
just test
claude                                   # trust the folder, then: /plugin install tanh-tools@tanh-tooling
```

`main` only accepts pull requests with green CI and >= 80% line coverage on every changed file.

## Docs

| Topic | |
|---|---|
| [Claude Code: your setup](docs/claude-setup.md) | `~/.claude`: global CLAUDE.md, settings, statusline, personal skills |
| [Claude Code: this repo](docs/claude-project.md) | CLAUDE.md files, `.claude/settings.json`, plugin, skill |
| [Hooks](docs/hooks.md) | format/lint after edits, build+test on stop, protected files, pre-push |
| [CMake and presets](docs/cmake.md) | `CMakeLists.txt`, every preset, install package, `justfile` |
| [clang-format, clang-tidy, clangd](docs/clang-tools.md) | style, naming rules, the language server |
| [Tests](docs/testing.md) | GoogleTest, CTest, export check |
| [Sanitizers](docs/sanitizers.md) | ASan/UBSan, TSan, RTSan and `TPL_NONBLOCKING` |
| [Coverage and Codecov](docs/coverage.md) | llvm-cov, the per-file gate, `codecov.yml` |
| [CI](docs/ci.md) | workflows, matrices, branch protection, releases |
| [Shared tooling](docs/tooling.md) | `tooling/`, installed copies, CMake modules |
