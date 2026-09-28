---
name: explain-pr
description: Explain what the current branch or PR changes, in plain language and in execution order, with code references.
disable-model-invocation: true
argument-hint: "[PR number or base branch, optional]"
---

Explain the changes on this branch (or PR $ARGUMENTS if given) so I fully understand what they do.

## Gather context

- If a PR number is given: `gh pr view $ARGUMENTS` and `gh pr diff $ARGUMENTS`.
- Otherwise find the base (`gh pr view` for the current branch, else main/master) and use `git log --oneline <base>..HEAD` and `git diff <base>...HEAD`.
- Read the full changed files and the callers/callees you need, not only the diff hunks.

## Output, in this order

1. **What it brings, in simple terms.** Two or three short paragraphs: the problem, what now works differently, and why it matters. No jargon unless defined. If it makes the change easier to grasp, include a small example of the new behaviour (e.g. a command, a config snippet, or what the user now sees).
2. **Execution-order walkthrough.** Start at the real entry point (main, callback, CLI command, request handler, audio callback, etc.) and follow the code path as it actually runs, not file by file. For each step:
   - reference the code as `path/to/file.cpp:123` (or a line range)
   - say what this step does and what changed compared to before
   - point out threading, lifetime, real-time or error-handling implications
   - where it helps, add a concrete example: sample input and the resulting output or state, a before/after of the behaviour, or a short usage snippet of a new API. Skip it when the step is already obvious.
3. **Things outside the main path.** Config, build system, tests, docs, and refactors that don't change behaviour.
4. **Risks and open questions.** Anything that looks suspicious, untested, or inconsistent with the PR description or commit messages.

Be detailed. Read the code before explaining it and never guess what a function does.
