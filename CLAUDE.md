# CommandStation-EX, rails49 fork

Read `GLOSSARY.md` for the vocabulary and `docs/adr/` for decisions before
touching track power or current protection.

gate: ./scripts/check.sh

## Agent skills

The rules every rails49 repository shares are the organisation's
[`docs/agents/`](https://github.com/rails49/.github/tree/main/docs/agents):
`issue-tracker.md`, `triage-labels.md` and `domain.md`.

- Issue tracker: GitHub Issues on rails49/CommandStation-EX, via the `gh` CLI.
- Triage labels: `needs-triage`, `needs-info`, `ready-for-agent`,
  `ready-for-human`, `wontfix`.
- Domain docs: `GLOSSARY.md` and `docs/adr/` at the repo root, and the command
  reference `docs/commands.md`, which most glossary entries link to. What each
  kind of doc holds: org
  [ADR-0025](https://github.com/rails49/.github/blob/main/docs/adr/0025-each-fact-has-one-owner-and-an-adr-is-written-after-its-decision-has-run.md).
