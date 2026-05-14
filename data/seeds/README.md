# KBO Seed Data

`seed_manifest.json` is the source of truth for seed classification and
payload routing.

Most domain files live under a source folder such as `captain/` or
`economy_and_fa/`. The manifest keeps a separate `path` for the runtime install
location because existing native readers still load many files from the
`%LOCALAPPDATA%\OOTP-KBO` root. When `source` is omitted, it defaults to
`path`.

Seed groups:

- `allstar`
- `amateur`
- `asian_games`
- `captain`
- `competitive_balance_tax`
- `custom_events`
- `economy_and_fa`
- `foreign_players`
- `military_service`
- `player_team_history`
- `runtime`
- `news_templates`
- `ui_text`
- `manifest`

When adding a new seed file, place it in the matching domain folder and add it
to `seed_manifest.json`. Use `path` for the installed runtime path and `source`
for the repository/bundled path. Release validation will pick it up
automatically.
