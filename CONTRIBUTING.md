# Contributing to ParseX

Thanks for considering a contribution. This guide covers the workflow end to
end; environment details live in `docs/dev-setup.md`, style rules in
`docs/coding-conventions.md`, and the testing reference in `docs/testing.md`
rather than repeated here.

## Before you start

- Check open issues for existing discussion of the change.
- For large changes (new subcommand, new tool, new report kind, schema
  support change), open an issue first describing the approach before writing
  code.
- Read the [versioning policy](docs/versioning.md) if the change could affect
  the command line, the assistant tools, or the report shapes.

## Making a change

- Fork the repo and create a feature branch from `main`
  (`feat/short-topic` or `fix/short-topic`).
- Keep commits focused; write commit messages as a short imperative summary
  (`Add ...`, `Fix ...`, `Document ...`).
- Follow the style in `docs/coding-conventions.md` and run the relevant
  checks from `docs/testing.md` before pushing.

## Submitting a pull request

- Open a pull request against `main`. The pull-request template
  (`.github/PULL_REQUEST_TEMPLATE.md`) lists the required checks — fill it
  in rather than deleting it.
- Continuous integration must pass: plain build plus unit checks, shared
  output checks, sanitizer build and suite, and the coverage floor.
- Every user-facing pull request adds a `CHANGELOG.md` entry under
  `## [Unreleased]` (see the note at the bottom of `CHANGELOG.md`).
- Behavior changes update the relevant doc (`README.md`, `docs/cli.md`,
  `docs/mcp.md`, or the design docs) in the same pull request.

## Reviewing a pull request

- Confirm continuous integration is green before approving.
- Confirm the template's checkboxes are actually true, not just checked
  (tests added, changelog entry present, docs updated, coverage floor held).
- Confirm new public APIs follow `docs/coding-conventions.md`.

## Getting help

- Open an issue with what you tried, what you expected, and the full output.
- For build problems, include the CMake preset used and the compiler version.
