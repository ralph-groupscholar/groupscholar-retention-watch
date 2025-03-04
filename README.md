# Group Scholar Retention Watch

Retention Watch is a local-first CLI that scores scholar retention risk from cohort check-in data. It highlights high-risk scholars, summarizes cohort health, and flags check-ins that are overdue or due soon.

## Features
- Risk scoring based on attendance, engagement, recency, and milestone progress
- High-risk roster with next check-in dates and due status
- Risk driver summary (attendance, engagement, check-in recency, milestones)
- Cohort-level risk distribution summaries
- Optional JSON export for downstream reporting

## Requirements
- Python 3.9+

## Quickstart
```bash
python3 retention_watch.py data/sample.csv
```

Generate JSON output:
```bash
python3 retention_watch.py data/sample.csv --json retention_summary.json
```

Override the date and due-soon window:
```bash
python3 retention_watch.py data/sample.csv --today 2026-02-07 --due-soon-days 10
```

Limit the high-risk roster size:
```bash
python3 retention_watch.py data/sample.csv --max-high-risk 5
```

## CSV Format
Required columns:
- scholar_id
- name
- cohort
- attendance_rate (0-1)
- engagement_score (1-5)
- last_checkin_date (YYYY-MM-DD)
- milestone_count

Optional:
- risk_notes

## Output
The CLI prints a summary with total counts, average metrics, risk mix, check-in status, top risk drivers, cohort distribution, and a high-risk roster. JSON output mirrors the same data in structured form and includes driver counts plus roster limits.

## Tech
- Python 3
- Standard library only (csv, json, datetime)
