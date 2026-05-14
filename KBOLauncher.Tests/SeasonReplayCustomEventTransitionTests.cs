namespace KBOLauncher.Tests;

using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using FluentAssertions;
using Xunit;

public sealed class SeasonReplayCustomEventTransitionTests
{
    private const string CustomEventOffseasonTransitionKind = "custom_event.offseason_transition";

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    public static IEnumerable<object[]> OffseasonTransitionScenarios()
    {
        var root = FindRepoRoot();
        var scenarioDir = Path.Combine(root, "tests", "scenarios", "season_replay", "custom_event_transitions");
        foreach (var path in Directory.EnumerateFiles(scenarioDir, "*.json").OrderBy(path => path, StringComparer.OrdinalIgnoreCase))
        {
            yield return new object[] { ReadJson<SeasonReplayScenario>(path) };
        }
    }

    [Theory]
    [MemberData(nameof(OffseasonTransitionScenarios))]
    public void CustomEventOffseasonTransition_ReplaysExpectedSchedulingDecision(SeasonReplayScenario scenario)
    {
        scenario.Kind.Should().Be(CustomEventOffseasonTransitionKind);

        var actual = ReplayOffseasonTransition(scenario.Inputs);

        actual.Should().BeEquivalentTo(scenario.ExpectedTransition);
    }

    private static OffseasonTransitionResult ReplayOffseasonTransition(ReplayInputs inputs)
    {
        var today = ParseDate(inputs.Date);
        var pendingAnchor = ParseOptionalDate(inputs.PendingAnchorDate);

        if (!DateAllowsOffseasonTransition(today))
        {
            return SchedulePending(today, pendingAnchor, inputs.LastOffseasonTransitionAnchorDate);
        }

        if (!inputs.ReadPhaseAvailable)
        {
            return SchedulePending(today, pendingAnchor, inputs.LastOffseasonTransitionAnchorDate);
        }

        var transitionedToOffseason = inputs.HadPreviousPhase
            && PhaseCanEnterOffseason(inputs.PreviousPhase)
            && PhaseIsOffseason(inputs.Phase);
        if (!transitionedToOffseason)
        {
            return SchedulePending(today, pendingAnchor, inputs.LastOffseasonTransitionAnchorDate);
        }

        var anchor = ResolveOffseasonTransitionAnchor(inputs, today);
        if (SameDate(anchor, ParseOptionalDate(inputs.LastOffseasonTransitionAnchorDate)))
        {
            return new OffseasonTransitionResult
            {
                Action = "already_scheduled",
                AnchorDate = FormatDate(anchor),
            };
        }

        return SchedulePending(today, anchor, inputs.LastOffseasonTransitionAnchorDate);
    }

    private static OffseasonTransitionResult SchedulePending(
        DateOnly today,
        DateOnly? pendingAnchor,
        string lastOffseasonTransitionAnchorDate)
    {
        if (pendingAnchor is null)
        {
            return new OffseasonTransitionResult { Action = "none" };
        }

        var lastAnchor = ParseOptionalDate(lastOffseasonTransitionAnchorDate);
        if (SameDate(pendingAnchor.Value, lastAnchor))
        {
            return new OffseasonTransitionResult
            {
                Action = "already_scheduled",
                AnchorDate = FormatDate(pendingAnchor.Value),
            };
        }
        if (today < pendingAnchor.Value)
        {
            return new OffseasonTransitionResult
            {
                Action = "pending",
                AnchorDate = FormatDate(pendingAnchor.Value),
            };
        }

        return new OffseasonTransitionResult
        {
            Action = "schedule_foreign_priority",
            AnchorDate = FormatDate(pendingAnchor.Value),
        };
    }

    private static DateOnly ResolveOffseasonTransitionAnchor(ReplayInputs inputs, DateOnly today)
    {
        var stockOffseasonStart = ParseOptionalDate(inputs.StockOffseasonStartDate);
        if (stockOffseasonStart is not null)
        {
            return stockOffseasonStart.Value;
        }

        var previousPhaseDate = ParseOptionalDate(inputs.PreviousPhaseDate);
        if (previousPhaseDate is not null
                && previousPhaseDate.Value < today
                && DateAllowsOffseasonTransition(previousPhaseDate.Value)
                && today.DayNumber - previousPhaseDate.Value.DayNumber <= 1)
        {
            return previousPhaseDate.Value;
        }

        return today;
    }

    private static bool PhaseIsOffseason(int phase)
    {
        return phase is 0 or 1;
    }

    private static bool PhaseCanEnterOffseason(int phase)
    {
        return phase is 3 or 4;
    }

    private static bool DateAllowsOffseasonTransition(DateOnly date)
    {
        return MonthDay(date) >= 1001;
    }

    private static int MonthDay(DateOnly date)
    {
        return (date.Month * 100) + date.Day;
    }

    private static bool SameDate(DateOnly date, DateOnly? other)
    {
        return other is not null && date == other.Value;
    }

    private static T ReadJson<T>(string path)
    {
        return JsonSerializer.Deserialize<T>(File.ReadAllText(path), JsonOptions)
            ?? throw new InvalidOperationException($"Could not read JSON: {path}");
    }

    private static DateOnly ParseDate(string value)
    {
        return DateOnly.ParseExact(value, "yyyy-MM-dd", CultureInfo.InvariantCulture);
    }

    private static DateOnly? ParseOptionalDate(string value)
    {
        return string.IsNullOrWhiteSpace(value) ? null : ParseDate(value);
    }

    private static string FormatDate(DateOnly date)
    {
        return date.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture);
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "OOTP27-KBO-Launcher.sln")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        throw new InvalidOperationException("Could not find repository root.");
    }

    public sealed class SeasonReplayScenario
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("kind")]
        public string Kind { get; set; } = "";

        [JsonPropertyName("inputs")]
        public ReplayInputs Inputs { get; set; } = new();

        [JsonPropertyName("expected_transition")]
        public OffseasonTransitionResult ExpectedTransition { get; set; } = new();
    }

    public sealed class ReplayInputs
    {
        [JsonPropertyName("date")]
        public string Date { get; set; } = "";

        [JsonPropertyName("read_phase_available")]
        public bool ReadPhaseAvailable { get; set; } = true;

        [JsonPropertyName("had_previous_phase")]
        public bool HadPreviousPhase { get; set; }

        [JsonPropertyName("previous_phase")]
        public int PreviousPhase { get; set; } = 255;

        [JsonPropertyName("phase")]
        public int Phase { get; set; } = 255;

        [JsonPropertyName("previous_phase_date")]
        public string PreviousPhaseDate { get; set; } = "";

        [JsonPropertyName("stock_offseason_start_date")]
        public string StockOffseasonStartDate { get; set; } = "";

        [JsonPropertyName("pending_anchor_date")]
        public string PendingAnchorDate { get; set; } = "";

        [JsonPropertyName("last_offseason_transition_anchor_date")]
        public string LastOffseasonTransitionAnchorDate { get; set; } = "";
    }

    public sealed class OffseasonTransitionResult
    {
        [JsonPropertyName("action")]
        public string Action { get; set; } = "";

        [JsonPropertyName("anchor_date")]
        public string AnchorDate { get; set; } = "";
    }
}
