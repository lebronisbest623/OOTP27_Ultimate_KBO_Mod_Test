namespace KBOLauncher.Tests;

using System.Text.Json.Serialization;

public sealed partial class SeasonReplayCaptainLifecycleTests
{
    public sealed class SeasonReplayScenario
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("kind")]
        public string Kind { get; set; } = "";

        [JsonPropertyName("inputs")]
        public ReplayInputs Inputs { get; set; } = new();

        [JsonPropertyName("teams")]
        public List<CaptainTeam> Teams { get; set; } = [];

        [JsonPropertyName("players")]
        public List<CaptainPlayer> Players { get; set; } = [];

        [JsonPropertyName("current_captains")]
        public List<CaptainCurrentRow> CurrentCaptains { get; set; } = [];

        [JsonPropertyName("expected_captains")]
        public List<CaptainSelection> ExpectedCaptains { get; set; } = [];

        [JsonPropertyName("expected_maintenance")]
        public List<CaptainMaintenanceAction> ExpectedMaintenance { get; set; } = [];
    }

    public sealed class ReplayInputs
    {
        [JsonPropertyName("season")]
        public int Season { get; set; }

        [JsonPropertyName("date")]
        public string Date { get; set; } = "";

        [JsonPropertyName("league_id")]
        public int LeagueId { get; set; }

        [JsonPropertyName("league_season")]
        public int LeagueSeason { get; set; }

        [JsonPropertyName("phase")]
        public int Phase { get; set; }

        [JsonPropertyName("league_ptr_available")]
        public bool LeaguePtrAvailable { get; set; } = true;

        [JsonPropertyName("csv_exists")]
        public bool CsvExists { get; set; }

        [JsonPropertyName("seed_available")]
        public bool SeedAvailable { get; set; }

        [JsonPropertyName("summary_rows_available")]
        public bool SummaryRowsAvailable { get; set; } = true;
    }

    public sealed class CaptainSelectionPolicy
    {
        [JsonPropertyName("eligible_age_min")]
        public int EligibleAgeMin { get; set; }

        [JsonPropertyName("eligible_age_max")]
        public int EligibleAgeMax { get; set; }

        [JsonPropertyName("salary_score_divisor")]
        public int SalaryScoreDivisor { get; set; }

        [JsonPropertyName("salary_score_max")]
        public int SalaryScoreMax { get; set; }

        [JsonPropertyName("age_core_min")]
        public int AgeCoreMin { get; set; }

        [JsonPropertyName("age_core_max")]
        public int AgeCoreMax { get; set; }

        [JsonPropertyName("age_core_score")]
        public int AgeCoreScore { get; set; }

        [JsonPropertyName("age_extended_min")]
        public int AgeExtendedMin { get; set; }

        [JsonPropertyName("age_extended_max")]
        public int AgeExtendedMax { get; set; }

        [JsonPropertyName("age_extended_score")]
        public int AgeExtendedScore { get; set; }

        [JsonPropertyName("age_depth_min")]
        public int AgeDepthMin { get; set; }

        [JsonPropertyName("age_depth_max")]
        public int AgeDepthMax { get; set; }

        [JsonPropertyName("age_depth_score")]
        public int AgeDepthScore { get; set; }

        [JsonPropertyName("domestic_bonus")]
        public int DomesticBonus { get; set; }

        [JsonPropertyName("foreign_penalty")]
        public int ForeignPenalty { get; set; }

        [JsonPropertyName("active_team_bonus")]
        public int ActiveTeamBonus { get; set; }

        [JsonPropertyName("current_team_bonus")]
        public int CurrentTeamBonus { get; set; }

        [JsonPropertyName("same_team_min_seasons")]
        public int SameTeamMinSeasons { get; set; }

        [JsonPropertyName("same_team_bonus_per_season")]
        public int SameTeamBonusPerSeason { get; set; }

        [JsonPropertyName("same_team_bonus_max")]
        public int SameTeamBonusMax { get; set; }

        [JsonPropertyName("same_team_short_penalty")]
        public int SameTeamShortPenalty { get; set; }

        [JsonPropertyName("same_team_unknown_penalty")]
        public int SameTeamUnknownPenalty { get; set; }

        [JsonPropertyName("dfa_penalty")]
        public int DfaPenalty { get; set; }

        [JsonPropertyName("restricted_penalty")]
        public int RestrictedPenalty { get; set; }

        [JsonPropertyName("injured_penalty")]
        public int InjuredPenalty { get; set; }
    }

    public sealed class CaptainTeam
    {
        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("code")]
        public string Code { get; set; } = "";

        [JsonPropertyName("name")]
        public string Name { get; set; } = "";
    }

    public sealed class CaptainPlayer
    {
        [JsonPropertyName("player_id")]
        public int PlayerId { get; set; }

        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("current_team_id")]
        public int CurrentTeamId { get; set; }

        [JsonPropertyName("active_team_id")]
        public int ActiveTeamId { get; set; }

        [JsonPropertyName("nation_id")]
        public int NationId { get; set; }

        [JsonPropertyName("age")]
        public int Age { get; set; }

        [JsonPropertyName("salary")]
        public int Salary { get; set; }

        [JsonPropertyName("value_score")]
        public int ValueScore { get; set; }

        [JsonPropertyName("same_team_seasons")]
        public int SameTeamSeasons { get; set; }

        [JsonPropertyName("retired")]
        public bool Retired { get; set; }

        [JsonPropertyName("dfa")]
        public bool Dfa { get; set; }

        [JsonPropertyName("restricted")]
        public bool Restricted { get; set; }

        [JsonPropertyName("injured")]
        public bool Injured { get; set; }

        [JsonPropertyName("seeded")]
        public bool Seeded { get; set; }

        [JsonPropertyName("seed_priority")]
        public int SeedPriority { get; set; }

        [JsonPropertyName("seed_key")]
        public string SeedKey { get; set; } = "";
    }

    public sealed class CaptainSelection
    {
        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("team_name")]
        public string TeamName { get; set; } = "";

        [JsonPropertyName("player_id")]
        public int PlayerId { get; set; }

        [JsonPropertyName("player_name")]
        public string PlayerName { get; set; } = "";

        [JsonPropertyName("score")]
        public int Score { get; set; }

        [JsonPropertyName("seeded")]
        public bool Seeded { get; set; }

        [JsonPropertyName("seed_priority")]
        public int SeedPriority { get; set; }

        [JsonPropertyName("reason")]
        public string Reason { get; set; } = "";
    }

    public sealed class CaptainCurrentRow
    {
        [JsonPropertyName("team_id")]
        public int TeamId { get; set; }

        [JsonPropertyName("team_name")]
        public string TeamName { get; set; } = "";

        [JsonPropertyName("player_id")]
        public int PlayerId { get; set; }

        [JsonPropertyName("player_name")]
        public string PlayerName { get; set; } = "";

        [JsonPropertyName("score")]
        public int Score { get; set; }

        [JsonPropertyName("seeded")]
        public bool Seeded { get; set; }

        [JsonPropertyName("seed_priority")]
        public int SeedPriority { get; set; }

        [JsonPropertyName("reason")]
        public string Reason { get; set; } = "";

        [JsonPropertyName("still_with_team")]
        public bool StillWithTeam { get; set; } = true;

        public CaptainSelection ToSelection()
        {
            return new CaptainSelection
            {
                TeamId = TeamId,
                TeamName = TeamName,
                PlayerId = PlayerId,
                PlayerName = PlayerName,
                Score = Score,
                Seeded = Seeded,
                SeedPriority = SeedPriority,
                Reason = Reason,
            };
        }
    }

    public sealed class CaptainMaintenanceAction
    {
        [JsonPropertyName("action")]
        public string Action { get; set; } = "";

        [JsonPropertyName("season")]
        public int Season { get; set; }

        [JsonPropertyName("reason")]
        public string Reason { get; set; } = "";
    }
}
