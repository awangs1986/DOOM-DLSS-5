# Domain Docs

This repo uses a single-context domain documentation layout.

## Before exploring

- Read root `CONTEXT.md` for relevant domain vocabulary and context.
- Read ADRs under `docs/adr/` that concern the area being changed.
- If these documents do not exist, proceed silently. Domain documents are created lazily through `domain-modeling` when terminology or architectural decisions are resolved.

## Layout

- `CONTEXT.md`: project context and domain glossary.
- `docs/adr/`: architectural decisions for the whole project.

## Vocabulary and decisions

Use the terms defined in `CONTEXT.md` in specs, issues, designs, code reviews, and tests. When a needed term is missing, reconsider whether it belongs to this domain and record a real terminology gap through `domain-modeling`.

If a proposal conflicts with an ADR, identify that ADR and explain the conflict before changing the decision.
