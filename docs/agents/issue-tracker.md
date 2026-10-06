# Issue tracker: GitHub

Issues and specs live in GitHub Issues for `awangs1986/DOOM-DLSS-5`. Use the `gh` CLI from this clone, or pass `--repo awangs1986/DOOM-DLSS-5` explicitly.

## Conventions

- Create an issue with `gh issue create --title "..." --body-file <file> --label <label>`; use a UTF-8 file for multiline bodies.
- Read an issue with `gh issue view <number> --comments`; fetch labels as needed using `--json`.
- List issues with `gh issue list --state open --json number,title,body,labels,assignees`, using the relevant label and state filters.
- Comment with `gh issue comment <number> --body-file <file>` when the invoked task authorizes posting.
- Apply or remove labels with `gh issue edit <number> --add-label <label>` or `--remove-label <label>`.
- Close with `gh issue close <number>` when the invoked task authorizes closing it.
- State labels use the mapping in `docs/agents/triage-labels.md`. A fully specified issue published by `to-spec` receives `ready-for-agent`.

## Pull requests as a triage surface

**PRs as a request surface: no.**

GitHub issues and pull requests share a number space. When an explicitly named item is ambiguous, try `gh pr view <number>`, then `gh issue view <number>`.

## Tracker instructions for engineering skills

When a skill says "publish to the issue tracker", create a GitHub issue. When it says "fetch the relevant ticket", read the issue and its comments.

## Wayfinding operations

- A map is one issue labelled `wayfinder:map`; each child is a separate issue labelled `wayfinder:<type>` and linked through GitHub sub-issues. If sub-issues are unavailable, use a task list in the map and a `Part of #<map>` reference in each child.
- Prefer native issue dependencies. Add a blocker through the issue dependencies endpoint using the blocker's numeric database ID. If dependencies are unavailable, record `Blocked by: #<number>` in the child.
- The frontier contains open, unassigned children whose blockers are closed; choose the first in map order.
- Claim by assigning the ticket to the driving developer. Resolve by posting the result, closing the ticket, and adding a result pointer to the map's decisions.
