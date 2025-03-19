# Group Scholar Retention Watch

Retention Watch is a local-first CLI that turns a cohort CSV into a prioritized retention action queue. It computes a transparent risk score, summarizes risk tiers by cohort, and produces a ready-to-share action list for outreach planning.

## Features
- Risk scoring across inactivity, attendance, engagement, GPA, survey sentiment, and open flags
- Cohort summaries with tier counts and average risk
- Action queue with suggested outreach focus
- JSON output for downstream tooling
- Cohort filter and CSV export for ops handoffs

## Getting Started

Build the CLI:

```bash
make
```

Run against the sample data:

```bash
./retention-watch sample-data.csv
```

JSON output:

```bash
./retention-watch sample-data.csv -limit 5 -json
```

Export to CSV for downstream planning:

```bash
./retention-watch sample-data.csv -cohort "Fall 2024" -export retention-report.csv
```

Full JSON output (includes all records):

```bash
./retention-watch sample-data.csv -json-full
```

## Database Sync (Production)

Retention Watch can persist run history to the Group Scholar Postgres database.
This is intended for deployed/production usage only.

Prereqs:
- Python 3.11+
- `psycopg` installed (`pip install psycopg[binary]`)
- Environment variables set for the production database connection

Schema + ingest:

```bash
python3 db_sync.py init
python3 db_sync.py ingest sample-data.csv --notes "Seeded sample run"
```

Connection options (do not hardcode credentials):
- `RETENTION_WATCH_DATABASE_URL` (preferred)
- or `PGHOST`, `PGPORT`, `PGUSER`, `PGPASSWORD`, `PGDATABASE`

## CSV Format

Required columns:

```
scholar_id,name,cohort,days_inactive,attendance_rate,engagement_score,gpa,last_contact_days,survey_score,open_flags
```

## Output

The CLI prints:
- Total records, average risk, and tier counts
- Cohort-level summaries
- A ranked action queue with suggested next steps

## Notes
- CSV parsing assumes no quoted commas.
- Scores are capped to 0-100 for easy tiering.

## Tech
- C (C11)
- Standard library only
