# KBO FA Compensation TODO

## FA signing AI must price compensation before offering

Current implementation records compensation obligations after a compensated FA signs, then selects player-plus-cash or cash-only after the protected list is available.

The next research task is earlier in the pipeline: when OOTP AI evaluates whether to sign a FA, the KBO mod must add the expected compensation cost to the acquisition price.

Implementation target:
- Find the FA target/offer valuation hook where AI clubs decide whether a FA is worth signing.
- Estimate compensation burden from the player's KBO FA grade and previous salary.
- For Grade A/B-style cases, include both cash and expected protected-list player loss.
- Penalize marginal signings hard enough that AI clubs avoid giving up compensation for replacement-level players.
- Keep this separate from the existing post-signing compensation decision code.

Important distinction:
- `kbo_fa_compensation_ai_prefers_cash_only()` is too late for this problem. It only chooses the compensation package after the signing already happened.
- The real fix must happen before the offer/signing decision, inside or near the FA acquisition valuation path.
