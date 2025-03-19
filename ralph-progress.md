# Ralph Progress Log

## 2026-02-07
- Initialized Group Scholar Retention Watch CLI.
- Implemented CSV ingestion, risk scoring, cohort summaries, and high-risk roster output.
- Added JSON export option and sample dataset.
- Documented usage and CSV schema in README.

## 2026-02-08
- Added cohort filtering and CSV export for handoff-ready retention reports.
- Updated CLI usage and README examples for new flags.

## 2026-02-08
- Added -json-full option to export full record payload alongside summaries.
- Documented full JSON output mode and validated build.

## 2026-02-08
- Added Postgres sync script to persist retention runs and scholar snapshots.
- Documented production database sync workflow in README.
- Initialized retention_watch schema and seeded sample run data in production database.

## 2026-02-08
- Added optional risk-driver insights for each scholar via new -drivers flag.
- Extended CSV/JSON outputs and action queue to include top risk contributors when enabled.
