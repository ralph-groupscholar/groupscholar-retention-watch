#!/usr/bin/env python3
import argparse
import csv
import json
from dataclasses import dataclass
from datetime import datetime, timedelta, date
from typing import Dict, List, Tuple

DATE_FMT = "%Y-%m-%d"

@dataclass
class ScholarRecord:
    scholar_id: str
    name: str
    cohort: str
    attendance_rate: float
    engagement_score: float
    last_checkin_date: date
    milestone_count: int
    risk_notes: str

    def risk_score(self, today: date) -> float:
        score = 0.0
        if self.attendance_rate < 0.70:
            score += 3.0
        elif self.attendance_rate < 0.85:
            score += 1.5

        if self.engagement_score < 3.0:
            score += 3.0
        elif self.engagement_score < 4.0:
            score += 1.5

        days_since = (today - self.last_checkin_date).days
        if days_since > 30:
            score += 2.0
        elif days_since > 14:
            score += 1.0

        if self.milestone_count < 1:
            score += 1.0

        return score

    def risk_tier(self, today: date) -> str:
        score = self.risk_score(today)
        if score >= 6.0:
            return "high"
        if score >= 3.5:
            return "medium"
        return "low"

    def risk_drivers(self, today: date) -> List[str]:
        drivers = []
        if self.attendance_rate < 0.70:
            drivers.append("low_attendance")
        elif self.attendance_rate < 0.85:
            drivers.append("soft_attendance")

        if self.engagement_score < 3.0:
            drivers.append("low_engagement")
        elif self.engagement_score < 4.0:
            drivers.append("soft_engagement")

        days_since = (today - self.last_checkin_date).days
        if days_since > 30:
            drivers.append("stale_checkin")
        elif days_since > 14:
            drivers.append("aging_checkin")

        if self.milestone_count < 1:
            drivers.append("no_milestones")

        return drivers

    def next_checkin_date(self, today: date) -> date:
        tier = self.risk_tier(today)
        if tier == "high":
            return self.last_checkin_date + timedelta(days=7)
        if tier == "medium":
            return self.last_checkin_date + timedelta(days=14)
        return self.last_checkin_date + timedelta(days=30)

    def due_status(self, today: date, due_soon_days: int) -> str:
        next_due = self.next_checkin_date(today)
        if today > next_due:
            return "overdue"
        if today >= next_due - timedelta(days=due_soon_days):
            return "due_soon"
        return "upcoming"


def parse_float(value: str, field: str, row_id: str) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        raise ValueError(f"Invalid {field} for {row_id}: {value}")


def parse_int(value: str, field: str, row_id: str) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        raise ValueError(f"Invalid {field} for {row_id}: {value}")


def parse_date(value: str, field: str, row_id: str) -> date:
    try:
        return datetime.strptime(value, DATE_FMT).date()
    except (TypeError, ValueError):
        raise ValueError(f"Invalid {field} for {row_id}: {value} (expected YYYY-MM-DD)")


def load_records(path: str) -> List[ScholarRecord]:
    records: List[ScholarRecord] = []
    with open(path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        required = {
            "scholar_id",
            "name",
            "cohort",
            "attendance_rate",
            "engagement_score",
            "last_checkin_date",
            "milestone_count",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Missing required columns: {', '.join(sorted(missing))}")

        for idx, row in enumerate(reader, start=1):
            row_id = row.get("scholar_id") or f"row {idx}"
            records.append(
                ScholarRecord(
                    scholar_id=row.get("scholar_id", "").strip(),
                    name=row.get("name", "").strip(),
                    cohort=row.get("cohort", "").strip(),
                    attendance_rate=parse_float(row.get("attendance_rate", ""), "attendance_rate", row_id),
                    engagement_score=parse_float(row.get("engagement_score", ""), "engagement_score", row_id),
                    last_checkin_date=parse_date(row.get("last_checkin_date", ""), "last_checkin_date", row_id),
                    milestone_count=parse_int(row.get("milestone_count", ""), "milestone_count", row_id),
                    risk_notes=(row.get("risk_notes") or "").strip(),
                )
            )
    return records


def summarize(
    records: List[ScholarRecord],
    today: date,
    due_soon_days: int,
    max_high_risk: int,
) -> Dict:
    tiers = {"high": [], "medium": [], "low": []}
    by_cohort: Dict[str, Dict[str, int]] = {}
    due_status = {"overdue": 0, "due_soon": 0, "upcoming": 0}
    driver_counts: Dict[str, int] = {}

    for record in records:
        tier = record.risk_tier(today)
        tiers[tier].append(record)

        cohort = record.cohort or "Unassigned"
        by_cohort.setdefault(cohort, {"high": 0, "medium": 0, "low": 0})
        by_cohort[cohort][tier] += 1

        status = record.due_status(today, due_soon_days)
        due_status[status] += 1

        for driver in record.risk_drivers(today):
            driver_counts[driver] = driver_counts.get(driver, 0) + 1

    def avg(field_fn):
        if not records:
            return 0.0
        return round(sum(field_fn(r) for r in records) / len(records), 2)

    high_risk_sorted = sorted(tiers["high"], key=lambda x: x.risk_score(today), reverse=True)
    high_risk_list = high_risk_sorted[:max_high_risk] if max_high_risk else high_risk_sorted

    summary = {
        "total_scholars": len(records),
        "average_attendance": avg(lambda r: r.attendance_rate),
        "average_engagement": avg(lambda r: r.engagement_score),
        "risk_counts": {k: len(v) for k, v in tiers.items()},
        "due_status": due_status,
        "by_cohort": by_cohort,
        "risk_driver_counts": dict(sorted(driver_counts.items(), key=lambda item: item[1], reverse=True)),
        "high_risk_total": len(tiers["high"]),
        "high_risk_limit": max_high_risk,
        "high_risk_truncated": max_high_risk > 0 and len(tiers["high"]) > max_high_risk,
        "high_risk": [
            {
                "scholar_id": r.scholar_id,
                "name": r.name,
                "cohort": r.cohort,
                "risk_score": round(r.risk_score(today), 2),
                "attendance_rate": r.attendance_rate,
                "engagement_score": r.engagement_score,
                "last_checkin_date": r.last_checkin_date.strftime(DATE_FMT),
                "next_checkin_date": r.next_checkin_date(today).strftime(DATE_FMT),
                "due_status": r.due_status(today, due_soon_days),
                "risk_drivers": r.risk_drivers(today),
                "risk_notes": r.risk_notes,
            }
            for r in high_risk_list
        ],
    }
    return summary


def render_console(summary: Dict, due_soon_days: int, today: date) -> str:
    lines = []
    lines.append("Group Scholar Retention Watch")
    lines.append(f"Date: {today.strftime(DATE_FMT)}")
    lines.append("")
    lines.append(f"Total scholars: {summary['total_scholars']}")
    lines.append(f"Average attendance rate: {summary['average_attendance']:.2f}")
    lines.append(f"Average engagement score: {summary['average_engagement']:.2f}")
    lines.append(
        "Risk mix: "
        + ", ".join(
            f"{tier} {summary['risk_counts'][tier]}" for tier in ("high", "medium", "low")
        )
    )
    lines.append(
        "Check-in status (due soon window "
        f"{due_soon_days}d): "
        + ", ".join(
            f"{status} {summary['due_status'][status]}"
            for status in ("overdue", "due_soon", "upcoming")
        )
    )
    lines.append("Top risk drivers:")
    if summary["risk_driver_counts"]:
        for driver, count in summary["risk_driver_counts"].items():
            lines.append(f"- {driver.replace('_', ' ')}: {count}")
    else:
        lines.append("- None")
    lines.append("")
    lines.append("Cohort risk distribution:")
    for cohort, counts in sorted(summary["by_cohort"].items()):
        lines.append(
            f"- {cohort}: high {counts['high']}, "
            f"medium {counts['medium']}, low {counts['low']}"
        )

    lines.append("")
    if summary["high_risk_truncated"]:
        lines.append(
            f"High risk roster (top {summary['high_risk_limit']} of {summary['high_risk_total']}):"
        )
    else:
        lines.append("High risk roster:")
    if not summary["high_risk"]:
        lines.append("- None")
    else:
        for record in summary["high_risk"]:
            drivers = (
                " | Drivers: " + ", ".join(d.replace("_", " ") for d in record["risk_drivers"])
                if record["risk_drivers"]
                else ""
            )
            notes = f" | Notes: {record['risk_notes']}" if record["risk_notes"] else ""
            lines.append(
                f"- {record['name']} ({record['scholar_id']}) "
                f"[{record['cohort']}] score {record['risk_score']:.2f} "
                f"attendance {record['attendance_rate']:.2f} engagement {record['engagement_score']:.2f} "
                f"last check-in {record['last_checkin_date']} next {record['next_checkin_date']} "
                f"status {record['due_status']}{drivers}{notes}"
            )

    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Assess scholar retention risk from a cohort check-in CSV."
    )
    parser.add_argument("csv", help="Path to the cohort check-in CSV")
    parser.add_argument(
        "--json",
        dest="json_path",
        help="Write JSON summary to file instead of console output",
    )
    parser.add_argument(
        "--today",
        help="Override today's date (YYYY-MM-DD)",
    )
    parser.add_argument(
        "--due-soon-days",
        type=int,
        default=7,
        help="Days ahead to flag upcoming check-ins as due soon",
    )
    parser.add_argument(
        "--max-high-risk",
        type=int,
        default=10,
        help="Limit high-risk roster output (0 for unlimited)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    today = datetime.strptime(args.today, DATE_FMT).date() if args.today else datetime.now().date()
    records = load_records(args.csv)
    summary = summarize(records, today, args.due_soon_days, args.max_high_risk)

    if args.json_path:
        with open(args.json_path, "w", encoding="utf-8") as handle:
            json.dump(summary, handle, indent=2)
    else:
        print(render_console(summary, args.due_soon_days, today))


if __name__ == "__main__":
    main()
