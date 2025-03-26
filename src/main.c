#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FIELDS 16

typedef struct {
  char *id;
  char *name;
  char *cohort;
  double days_inactive;
  double attendance_rate;
  double engagement_score;
  double gpa;
  double last_contact_days;
  double survey_score;
  int open_flags;
  double risk_score;
} Scholar;

typedef struct {
  char *name;
  int total;
  int high;
  int medium;
  int low;
  double avg_risk;
} CohortSummary;

typedef struct {
  const char *label;
  double value;
} Driver;

static char *trim(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  if (*s == 0) return s;
  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) end--;
  end[1] = '\0';
  return s;
}

static double clamp(double v, double min, double max) {
  if (v < min) return min;
  if (v > max) return max;
  return v;
}

static double parse_double(const char *s) {
  if (!s || !*s) return 0.0;
  return atof(s);
}

static int parse_int(const char *s) {
  if (!s || !*s) return 0;
  return atoi(s);
}

static double compute_risk(const Scholar *s) {
  double gpa_gap = clamp(4.0 - s->gpa, 0.0, 4.0);
  double attendance_gap = clamp(100.0 - s->attendance_rate, 0.0, 100.0);
  double engagement_gap = clamp(100.0 - s->engagement_score, 0.0, 100.0);
  double survey_gap = clamp(100.0 - s->survey_score, 0.0, 100.0);

  double score = 0.0;
  score += s->days_inactive * 0.6;
  score += s->last_contact_days * 0.4;
  score += attendance_gap * 0.35;
  score += engagement_gap * 0.25;
  score += gpa_gap * 12.5;
  score += survey_gap * 0.15;
  score += s->open_flags * 6.0;
  return clamp(score, 0.0, 100.0);
}

static int compare_driver_desc(const void *a, const void *b) {
  const Driver *da = (const Driver *)a;
  const Driver *db = (const Driver *)b;
  if (da->value < db->value) return 1;
  if (da->value > db->value) return -1;
  return 0;
}

static void format_drivers(const Scholar *s, char *buffer, size_t size) {
  Driver drivers[8];
  int count = 0;

  double gpa_gap = clamp(4.0 - s->gpa, 0.0, 4.0);
  double attendance_gap = clamp(100.0 - s->attendance_rate, 0.0, 100.0);
  double engagement_gap = clamp(100.0 - s->engagement_score, 0.0, 100.0);
  double survey_gap = clamp(100.0 - s->survey_score, 0.0, 100.0);

  double inactivity = s->days_inactive * 0.6;
  double contact_gap = s->last_contact_days * 0.4;
  double attendance = attendance_gap * 0.35;
  double engagement = engagement_gap * 0.25;
  double gpa = gpa_gap * 12.5;
  double survey = survey_gap * 0.15;
  double flags = s->open_flags * 6.0;

  if (inactivity > 0.1) drivers[count++] = (Driver){"inactivity", inactivity};
  if (contact_gap > 0.1) drivers[count++] = (Driver){"contact gap", contact_gap};
  if (attendance > 0.1) drivers[count++] = (Driver){"attendance", attendance};
  if (engagement > 0.1) drivers[count++] = (Driver){"engagement", engagement};
  if (gpa > 0.1) drivers[count++] = (Driver){"gpa", gpa};
  if (survey > 0.1) drivers[count++] = (Driver){"survey", survey};
  if (flags > 0.1) drivers[count++] = (Driver){"open flags", flags};

  if (count == 0) {
    snprintf(buffer, size, "stable");
    return;
  }

  qsort(drivers, count, sizeof(Driver), compare_driver_desc);

  buffer[0] = '\0';
  int max = count < 3 ? count : 3;
  for (int i = 0; i < max; i++) {
    char chunk[64];
    snprintf(chunk, sizeof(chunk), "%s %.1f", drivers[i].label, drivers[i].value);
    if (i > 0) {
      strncat(buffer, "; ", size - strlen(buffer) - 1);
    }
    strncat(buffer, chunk, size - strlen(buffer) - 1);
  }
}

static const char *risk_tier(double score) {
  if (score >= 75.0) return "high";
  if (score >= 50.0) return "medium";
  return "low";
}

static const char *action_hint(const Scholar *s) {
  if (s->days_inactive >= 30.0) return "re-engage outreach";
  if (s->attendance_rate < 70.0) return "attendance support";
  if (s->gpa < 2.5) return "academic support";
  if (s->open_flags > 0) return "resolve open flags";
  if (s->engagement_score < 60.0) return "engagement nudge";
  return "lightweight check-in";
}

static int compare_risk_desc(const void *a, const void *b) {
  const Scholar *sa = (const Scholar *)a;
  const Scholar *sb = (const Scholar *)b;
  if (sa->risk_score < sb->risk_score) return 1;
  if (sa->risk_score > sb->risk_score) return -1;
  return 0;
}

static int compare_cohort_avg_desc(const void *a, const void *b) {
  const CohortSummary *ca = *(const CohortSummary **)a;
  const CohortSummary *cb = *(const CohortSummary **)b;
  double avg_a = ca->avg_risk / (double)ca->total;
  double avg_b = cb->avg_risk / (double)cb->total;
  if (avg_a < avg_b) return 1;
  if (avg_a > avg_b) return -1;
  return 0;
}

static CohortSummary *find_or_create_cohort(CohortSummary **cohorts, int *count, const char *name) {
  for (int i = 0; i < *count; i++) {
    if (strcmp((*cohorts)[i].name, name) == 0) {
      return &(*cohorts)[i];
    }
  }
  *cohorts = realloc(*cohorts, sizeof(CohortSummary) * (*count + 1));
  CohortSummary *cs = &(*cohorts)[*count];
  cs->name = strdup(name);
  cs->total = 0;
  cs->high = 0;
  cs->medium = 0;
  cs->low = 0;
  cs->avg_risk = 0.0;
  (*count)++;
  return cs;
}

static void print_usage(const char *prog) {
  printf("Group Scholar Retention Watch\n\n");
  printf("Usage: %s <csv-file> [-limit N] [-min-risk SCORE] [-cohort NAME] [-export PATH] [-summary PATH] [-json] [-json-full] [-drivers]\n\n", prog);
  printf("CSV columns:\n");
  printf("  scholar_id,name,cohort,days_inactive,attendance_rate,engagement_score,gpa,last_contact_days,survey_score,open_flags\n\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char *path = NULL;
  int limit = 10;
  double min_risk = 0.0;
  int json = 0;
  int json_full = 0;
  int drivers = 0;
  const char *cohort_filter = NULL;
  const char *export_path = NULL;
  const char *summary_path = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-limit") == 0 && i + 1 < argc) {
      limit = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-min-risk") == 0 && i + 1 < argc) {
      min_risk = parse_double(argv[++i]);
    } else if (strcmp(argv[i], "-cohort") == 0 && i + 1 < argc) {
      cohort_filter = argv[++i];
    } else if (strcmp(argv[i], "-export") == 0 && i + 1 < argc) {
      export_path = argv[++i];
    } else if (strcmp(argv[i], "-summary") == 0 && i + 1 < argc) {
      summary_path = argv[++i];
    } else if (strcmp(argv[i], "-json") == 0) {
      json = 1;
    } else if (strcmp(argv[i], "-json-full") == 0) {
      json = 1;
      json_full = 1;
    } else if (strcmp(argv[i], "-drivers") == 0) {
      drivers = 1;
    } else if (argv[i][0] != '-') {
      path = argv[i];
    }
  }

  if (!path) {
    print_usage(argv[0]);
    return 1;
  }

  FILE *fp = fopen(path, "r");
  if (!fp) {
    perror("Failed to open CSV");
    return 1;
  }

  Scholar *scholars = NULL;
  int count = 0;
  int capacity = 0;
  int skipped = 0;

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  int line_no = 0;

  while ((read = getline(&line, &len, fp)) != -1) {
    line_no++;
    if (line_no == 1 && strstr(line, "scholar_id") != NULL) {
      continue;
    }

    char *fields[MAX_FIELDS];
    int field_count = 0;

    char *cursor = line;
    while (field_count < MAX_FIELDS) {
      char *token = strsep(&cursor, ",");
      if (!token) break;
      fields[field_count++] = trim(token);
    }

    if (field_count < 10) {
      skipped++;
      continue;
    }

    Scholar s;
    s.id = strdup(fields[0]);
    s.name = strdup(fields[1]);
    s.cohort = strdup(fields[2]);
    s.days_inactive = parse_double(fields[3]);
    s.attendance_rate = parse_double(fields[4]);
    s.engagement_score = parse_double(fields[5]);
    s.gpa = parse_double(fields[6]);
    s.last_contact_days = parse_double(fields[7]);
    s.survey_score = parse_double(fields[8]);
    s.open_flags = parse_int(fields[9]);
    s.risk_score = compute_risk(&s);

    if (cohort_filter && strcmp(s.cohort, cohort_filter) != 0) {
      free(s.id);
      free(s.name);
      free(s.cohort);
      continue;
    }

    if (count >= capacity) {
      capacity = capacity == 0 ? 32 : capacity * 2;
      scholars = realloc(scholars, sizeof(Scholar) * capacity);
    }
    scholars[count++] = s;
  }

  free(line);
  fclose(fp);

  if (count == 0) {
    fprintf(stderr, "No records loaded.\n");
    return 1;
  }

  qsort(scholars, count, sizeof(Scholar), compare_risk_desc);

  if (export_path) {
    FILE *out = fopen(export_path, "w");
    if (!out) {
      perror("Failed to write export");
      return 1;
    }
    if (drivers) {
      fprintf(out, "scholar_id,name,cohort,risk_score,tier,action,drivers,days_inactive,attendance_rate,engagement_score,gpa,last_contact_days,survey_score,open_flags\n");
    } else {
      fprintf(out, "scholar_id,name,cohort,risk_score,tier,action,days_inactive,attendance_rate,engagement_score,gpa,last_contact_days,survey_score,open_flags\n");
    }
    for (int i = 0; i < count; i++) {
      Scholar *s = &scholars[i];
      if (s->risk_score < min_risk) {
        continue;
      }
      if (drivers) {
        char driver_text[256];
        format_drivers(s, driver_text, sizeof(driver_text));
        fprintf(out,
                "%s,%s,%s,%.1f,%s,%s,%s,%.1f,%.1f,%.1f,%.2f,%.1f,%.1f,%d\n",
                s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score),
                action_hint(s), driver_text, s->days_inactive, s->attendance_rate, s->engagement_score,
                s->gpa, s->last_contact_days, s->survey_score, s->open_flags);
      } else {
        fprintf(out,
                "%s,%s,%s,%.1f,%s,%s,%.1f,%.1f,%.1f,%.2f,%.1f,%.1f,%d\n",
                s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score),
                action_hint(s), s->days_inactive, s->attendance_rate, s->engagement_score,
                s->gpa, s->last_contact_days, s->survey_score, s->open_flags);
      }
    }
    fclose(out);
  }

  int high = 0;
  int medium = 0;
  int low = 0;
  double total_risk = 0.0;
  double avg_risk = 0.0;

  CohortSummary *cohorts = NULL;
  int cohort_count = 0;

  for (int i = 0; i < count; i++) {
    total_risk += scholars[i].risk_score;
    const char *tier = risk_tier(scholars[i].risk_score);
    if (strcmp(tier, "high") == 0) high++;
    else if (strcmp(tier, "medium") == 0) medium++;
    else low++;

    CohortSummary *cs = find_or_create_cohort(&cohorts, &cohort_count, scholars[i].cohort);
    cs->total++;
    cs->avg_risk += scholars[i].risk_score;
    if (strcmp(tier, "high") == 0) cs->high++;
    else if (strcmp(tier, "medium") == 0) cs->medium++;
    else cs->low++;
  }

  avg_risk = total_risk / (double)count;

  CohortSummary **focus = NULL;
  int focus_count = cohort_count;
  if (cohort_count > 0) {
    focus = malloc(sizeof(CohortSummary *) * cohort_count);
    for (int i = 0; i < cohort_count; i++) {
      focus[i] = &cohorts[i];
    }
    qsort(focus, cohort_count, sizeof(CohortSummary *), compare_cohort_avg_desc);
  }

  if (summary_path) {
    FILE *summary = fopen(summary_path, "w");
    if (!summary) {
      perror("Failed to write summary");
      return 1;
    }
    fprintf(summary, "cohort,total,avg_risk,high,medium,low\n");
    for (int i = 0; i < cohort_count; i++) {
      CohortSummary *cs = &cohorts[i];
      double avg = cs->avg_risk / (double)cs->total;
      fprintf(summary, "%s,%d,%.1f,%d,%d,%d\n",
              cs->name, cs->total, avg, cs->high, cs->medium, cs->low);
    }
    fclose(summary);
  }

  if (json) {
    printf("{\n");
    printf("  \"total\": %d,\n", count);
    printf("  \"average_risk\": %.1f,\n", avg_risk);
    printf("  \"tiers\": {\n");
    printf("    \"high\": %d,\n", high);
    printf("    \"medium\": %d,\n", medium);
    printf("    \"low\": %d\n", low);
    printf("  },\n");
    printf("  \"action_queue_min_risk\": %.1f,\n", min_risk);
    printf("  \"cohorts\": [\n");
    for (int i = 0; i < cohort_count; i++) {
      CohortSummary *cs = &cohorts[i];
      double avg = cs->avg_risk / (double)cs->total;
      printf("    {\"cohort\": \"%s\", \"total\": %d, \"avg_risk\": %.1f, \"high\": %d, \"medium\": %d, \"low\": %d}%s\n",
             cs->name, cs->total, avg, cs->high, cs->medium, cs->low,
             (i + 1 == cohort_count) ? "" : ",");
    }
    printf("  ],\n");
    printf("  \"cohort_focus\": [\n");
    int focus_max = focus_count < 3 ? focus_count : 3;
    for (int i = 0; i < focus_max; i++) {
      CohortSummary *cs = focus[i];
      double avg = cs->avg_risk / (double)cs->total;
      printf("    {\"cohort\": \"%s\", \"avg_risk\": %.1f, \"total\": %d, \"high\": %d, \"medium\": %d, \"low\": %d}%s\n",
             cs->name, avg, cs->total, cs->high, cs->medium, cs->low,
             (i + 1 == focus_max) ? "" : ",");
    }
    printf("  ],\n");
    printf("  \"action_queue\": [\n");
    int printed = 0;
    for (int i = 0; i < count && printed < limit; i++) {
      Scholar *s = &scholars[i];
      if (s->risk_score < min_risk) {
        continue;
      }
      if (printed > 0) {
        printf(",\n");
      }
      if (drivers) {
        char driver_text[256];
        format_drivers(s, driver_text, sizeof(driver_text));
        printf("    {\"scholar_id\": \"%s\", \"name\": \"%s\", \"cohort\": \"%s\", \"risk\": %.1f, \"tier\": \"%s\", \"action\": \"%s\", \"drivers\": \"%s\"}",
               s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score), action_hint(s), driver_text);
      } else {
        printf("    {\"scholar_id\": \"%s\", \"name\": \"%s\", \"cohort\": \"%s\", \"risk\": %.1f, \"tier\": \"%s\", \"action\": \"%s\"}",
               s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score), action_hint(s));
      }
      printed++;
    }
    if (printed > 0) {
      printf("\n");
    }
    printf("  ]");
    if (json_full) {
      printf(",\n  \"records\": [\n");
      for (int i = 0; i < count; i++) {
        Scholar *s = &scholars[i];
        if (drivers) {
          char driver_text[256];
          format_drivers(s, driver_text, sizeof(driver_text));
          printf("    {\"scholar_id\": \"%s\", \"name\": \"%s\", \"cohort\": \"%s\", \"days_inactive\": %.1f, \"attendance_rate\": %.1f, \"engagement_score\": %.1f, \"gpa\": %.2f, \"last_contact_days\": %.1f, \"survey_score\": %.1f, \"open_flags\": %d, \"risk\": %.1f, \"tier\": \"%s\", \"action\": \"%s\", \"drivers\": \"%s\"}%s\n",
                 s->id, s->name, s->cohort, s->days_inactive, s->attendance_rate, s->engagement_score,
                 s->gpa, s->last_contact_days, s->survey_score, s->open_flags, s->risk_score,
                 risk_tier(s->risk_score), action_hint(s), driver_text, (i + 1 == count) ? "" : ",");
        } else {
          printf("    {\"scholar_id\": \"%s\", \"name\": \"%s\", \"cohort\": \"%s\", \"days_inactive\": %.1f, \"attendance_rate\": %.1f, \"engagement_score\": %.1f, \"gpa\": %.2f, \"last_contact_days\": %.1f, \"survey_score\": %.1f, \"open_flags\": %d, \"risk\": %.1f, \"tier\": \"%s\", \"action\": \"%s\"}%s\n",
                 s->id, s->name, s->cohort, s->days_inactive, s->attendance_rate, s->engagement_score,
                 s->gpa, s->last_contact_days, s->survey_score, s->open_flags, s->risk_score,
                 risk_tier(s->risk_score), action_hint(s), (i + 1 == count) ? "" : ",");
        }
      }
      printf("  ]\n");
    } else {
      printf("\n");
    }
    printf("}\n");
  } else {
    printf("Group Scholar Retention Watch\n\n");
    printf("Records: %d  Average risk: %.1f  Skipped rows: %d\n", count, avg_risk, skipped);
    printf("Risk tiers: high %d | medium %d | low %d\n\n", high, medium, low);

    printf("Cohort summary:\n");
    for (int i = 0; i < cohort_count; i++) {
      CohortSummary *cs = &cohorts[i];
      double avg = cs->avg_risk / (double)cs->total;
      printf("- %s: total %d, avg risk %.1f, high %d, medium %d, low %d\n",
             cs->name, cs->total, avg, cs->high, cs->medium, cs->low);
    }

    if (focus_count > 0) {
      printf("\nCohort focus (top %d by avg risk):\n", focus_count < 3 ? focus_count : 3);
      int focus_max = focus_count < 3 ? focus_count : 3;
      for (int i = 0; i < focus_max; i++) {
        CohortSummary *cs = focus[i];
        double avg = cs->avg_risk / (double)cs->total;
        printf("- %s: avg risk %.1f (high %d, medium %d, low %d)\n",
               cs->name, avg, cs->high, cs->medium, cs->low);
      }
    }

    printf("\nAction queue (top %d, min risk %.1f):\n", limit, min_risk);
    int printed = 0;
    for (int i = 0; i < count && printed < limit; i++) {
      Scholar *s = &scholars[i];
      if (s->risk_score < min_risk) {
        continue;
      }
      if (drivers) {
        char driver_text[256];
        format_drivers(s, driver_text, sizeof(driver_text));
        printf("%2d. %-14s %-18s cohort %-10s risk %.1f (%s) -> %s | drivers: %s\n",
               printed + 1, s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score), action_hint(s),
               driver_text);
      } else {
        printf("%2d. %-14s %-18s cohort %-10s risk %.1f (%s) -> %s\n",
               printed + 1, s->id, s->name, s->cohort, s->risk_score, risk_tier(s->risk_score), action_hint(s));
      }
      printed++;
    }
    if (printed == 0) {
      printf("No scholars met the minimum risk threshold.\n");
    }
  }

  for (int i = 0; i < count; i++) {
    free(scholars[i].id);
    free(scholars[i].name);
    free(scholars[i].cohort);
  }
  free(focus);
  for (int i = 0; i < cohort_count; i++) {
    free(cohorts[i].name);
  }
  free(cohorts);
  free(scholars);

  return 0;
}
