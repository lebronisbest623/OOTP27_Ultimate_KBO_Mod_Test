# Season Replay Scenarios

These fixtures replay narrow season moments against the same seed and policy
files that the launcher ships. They are intentionally small: one scenario
should prove one rule, one date transition, or one selection edge case.

Supported scenario kinds:

- `custom_event.foreign_priority` validates the external custom event catalog
  and the foreign priority schedule derived from an offseason anchor.
- `custom_event.offseason_transition` validates the phase-transition anchor
  detection that queues the foreign-priority custom events.
- `captain.preseason_selection` validates preseason captain selection scoring,
  seed priority, and deterministic tie-breaking.
- `captain.maintenance_decision` validates the season-phase decisions that
  bootstrap captain CSVs or invoke in-season repair.
- `captain.inseason_repair` validates the merge behavior when an existing
  captain has left the team or remains valid.

Keep expected results explicit. The harness should fail when a policy seed,
catalog entry, or scenario expectation drifts.
