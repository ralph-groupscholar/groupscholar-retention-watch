#!/usr/bin/env python3
"""Retention Watch database sync utilities."""
import argparse
import csv
import os
from dataclasses import dataclass
from typing import List, Tuple, Optional

import psycopg

SCHEMA = "retention_watch"
REQUIRED_FIELDS = [
    "scholar_id",
    "name",
    "cohort",
    "days_inactive",
    "attendance_rate",
    "engagement_score",
    "gpa",
    "last_contact_days",
    "survey_score",
    "open_flags",
]


@dataclass
class Scholar:
    scholar_id: str
    name: str
    cohort: str
    days_inactive: float
    attendance_rate: float
    engagement_score: float
    gpa: float
    last_contact_days: float
    survey_score: float
    open_flags: int
    risk_score: float
    tier: str
    action_hint: str


def parse_float(value: str) -> float:
    return float(value.strip()) if value else 0.0


def parse_int(value: str) -> int:
    return int(value.strip()) if value else 0


def clamp(value: float, min_value: float, max_value: float) -> float:
    return max(min_value, min(value, max_value))


def compute_risk(scholar: Scholar) -> float:
    gpa_gap = clamp(4.0 - scholar.gpa, 0.0, 4.0)
    attendance_gap = clamp(100.0 - scholar.attendance_rate, 0.0, 100.0)
    engagement_gap = clamp(100.0 - scholar.engagement_score, 0.0, 100.0)
    survey_gap = clamp(100.0 - scholar.survey_score, 0.0, 100.0)

    score = 0.0
    score += scholar.days_inactive * 0.6
    score += scholar.last_contact_days * 0.4
    score += attendance_gap * 0.35
    score += engagement_gap * 0.25
    score += gpa_gap * 12.5
    score += survey_gap * 0.15
    score += scholar.open_flags * 6.0
    return clamp(score, 0.0, 100.0)


def risk_tier(score: float) -> str:
    if score >= 75.0:
        return "high"
    if score >= 50.0:
        return "medium"
    return "low"


def action_hint(scholar: Scholar) -> str:
    if scholar.days_inactive >= 30.0:
        return "re-engage outreach"
    if scholar.attendance_rate < 70.0:
        return "attendance support"
    if scholar.gpa < 2.5:
        return "academic support"
    if scholar.open_flags > 0:
        return "resolve open flags"
    if scholar.engagement_score < 60.0:
        return "engagement nudge"
    return "lightweight check-in"


def load_csv(path: str) -> Tuple[List[Scholar], int]:
    scholars: List[Scholar] = []
    skipped = 0
    with open(path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            raise ValueError("CSV missing header row")
        missing = [field for field in REQUIRED_FIELDS if field not in reader.fieldnames]
        if missing:
            raise ValueError(f"CSV missing required columns: {', '.join(missing)}")

        for row in reader:
            try:
                scholar = Scholar(
                    scholar_id=(row.get("scholar_id") or "").strip(),
                    name=(row.get("name") or "").strip(),
                    cohort=(row.get("cohort") or "").strip(),
                    days_inactive=parse_float(row.get("days_inactive", "")),
                    attendance_rate=parse_float(row.get("attendance_rate", "")),
                    engagement_score=parse_float(row.get("engagement_score", "")),
                    gpa=parse_float(row.get("gpa", "")),
                    last_contact_days=parse_float(row.get("last_contact_days", "")),
                    survey_score=parse_float(row.get("survey_score", "")),
                    open_flags=parse_int(row.get("open_flags", "")),
                    risk_score=0.0,
                    tier="",
                    action_hint="",
                )
                scholar.risk_score = compute_risk(scholar)
                scholar.tier = risk_tier(scholar.risk_score)
                scholar.action_hint = action_hint(scholar)
                scholars.append(scholar)
            except ValueError:
                skipped += 1
    return scholars, skipped


def connect_db() -> psycopg.Connection:
    dsn = os.environ.get("RETENTION_WATCH_DATABASE_URL")
    if dsn:
        return psycopg.connect(dsn)

    host = os.environ.get("PGHOST")
    user = os.environ.get("PGUSER")
    password = os.environ.get("PGPASSWORD")
    if not host or not user or not password:
        raise RuntimeError(
            "Set RETENTION_WATCH_DATABASE_URL or PGHOST/PGUSER/PGPASSWORD (and PGDATABASE/PGPORT if needed)."
        )
    return psycopg.connect(
        host=host,
        port=os.environ.get("PGPORT"),
        user=user,
        password=password,
        dbname=os.environ.get("PGDATABASE") or user,
    )


def init_db(conn: psycopg.Connection) -> None:
    with conn.cursor() as cur:
        cur.execute(f"CREATE SCHEMA IF NOT EXISTS {SCHEMA}")
        cur.execute(
            f"""
            CREATE TABLE IF NOT EXISTS {SCHEMA}.runs (
                run_id BIGSERIAL PRIMARY KEY,
                run_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                source_file TEXT NOT NULL,
                total INT NOT NULL,
                average_risk NUMERIC(5,1) NOT NULL,
                high INT NOT NULL,
                medium INT NOT NULL,
                low INT NOT NULL,
                skipped INT NOT NULL,
                notes TEXT
            )
            """
        )
        cur.execute(
            f"""
            CREATE TABLE IF NOT EXISTS {SCHEMA}.scholar_snapshots (
                snapshot_id BIGSERIAL PRIMARY KEY,
                run_id BIGINT NOT NULL REFERENCES {SCHEMA}.runs(run_id) ON DELETE CASCADE,
                scholar_id TEXT NOT NULL,
                name TEXT NOT NULL,
                cohort TEXT NOT NULL,
                days_inactive NUMERIC(6,1) NOT NULL,
                attendance_rate NUMERIC(6,1) NOT NULL,
                engagement_score NUMERIC(6,1) NOT NULL,
                gpa NUMERIC(4,2) NOT NULL,
                last_contact_days NUMERIC(6,1) NOT NULL,
                survey_score NUMERIC(6,1) NOT NULL,
                open_flags INT NOT NULL,
                risk_score NUMERIC(6,1) NOT NULL,
                tier TEXT NOT NULL,
                action_hint TEXT NOT NULL
            )
            """
        )
        cur.execute(
            f"CREATE INDEX IF NOT EXISTS idx_{SCHEMA}_snapshots_run ON {SCHEMA}.scholar_snapshots(run_id)"
        )
        cur.execute(
            f"CREATE INDEX IF NOT EXISTS idx_{SCHEMA}_snapshots_tier ON {SCHEMA}.scholar_snapshots(tier)"
        )
        cur.execute(
            f"CREATE INDEX IF NOT EXISTS idx_{SCHEMA}_snapshots_cohort ON {SCHEMA}.scholar_snapshots(cohort)"
        )
    conn.commit()


def ingest_csv(conn: psycopg.Connection, path: str, source_label: str, notes: Optional[str]) -> int:
    scholars, skipped = load_csv(path)
    if not scholars:
        raise RuntimeError("No records loaded from CSV")

    total = len(scholars)
    avg_risk = sum(s.risk_score for s in scholars) / total
    high = sum(1 for s in scholars if s.tier == "high")
    medium = sum(1 for s in scholars if s.tier == "medium")
    low = sum(1 for s in scholars if s.tier == "low")

    with conn.cursor() as cur:
        cur.execute(
            f"""
            INSERT INTO {SCHEMA}.runs
                (source_file, total, average_risk, high, medium, low, skipped, notes)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
            RETURNING run_id
            """,
            (source_label, total, round(avg_risk, 1), high, medium, low, skipped, notes),
        )
        run_id = cur.fetchone()[0]

        rows = [
            (
                run_id,
                s.scholar_id,
                s.name,
                s.cohort,
                s.days_inactive,
                s.attendance_rate,
                s.engagement_score,
                s.gpa,
                s.last_contact_days,
                s.survey_score,
                s.open_flags,
                round(s.risk_score, 1),
                s.tier,
                s.action_hint,
            )
            for s in scholars
        ]

        cur.executemany(
            f"""
            INSERT INTO {SCHEMA}.scholar_snapshots
                (run_id, scholar_id, name, cohort, days_inactive, attendance_rate,
                 engagement_score, gpa, last_contact_days, survey_score, open_flags,
                 risk_score, tier, action_hint)
            VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            """,
            rows,
        )

    conn.commit()
    return run_id


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Retention Watch Postgres sync")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("init", help="Create database schema and tables")

    ingest_parser = subparsers.add_parser("ingest", help="Ingest a CSV into Postgres")
    ingest_parser.add_argument("csv", help="Path to retention CSV")
    ingest_parser.add_argument(
        "--source-label",
        default=None,
        help="Label to store as the source file (defaults to CSV filename)",
    )
    ingest_parser.add_argument("--notes", default=None, help="Notes for this run")

    return parser.parse_args()


def main() -> None:
    args = parse_args()
    with connect_db() as conn:
        init_db(conn)
        if args.command == "init":
            print("Retention Watch schema ensured.")
            return

        source_label = args.source_label or os.path.basename(args.csv)
        run_id = ingest_csv(conn, args.csv, source_label, args.notes)
        print(f"Ingested run {run_id} from {source_label}.")


if __name__ == "__main__":
    main()
