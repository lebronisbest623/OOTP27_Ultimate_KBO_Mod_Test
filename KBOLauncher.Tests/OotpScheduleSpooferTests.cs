namespace KBOLauncher.Tests;

using Xunit;

public sealed class OotpScheduleSpooferTests
{
    [Fact]
    public void BuildSpoofScheduleText_AddsExplicitAllstarGameWhenOnlyHeaderDayExists()
    {
        var source = """
            <?xml version="1.0" encoding="ISO-8859-1"?>
            <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """;

        var actual = global::OotpScheduleSpoofer.BuildSpoofScheduleText(source, 2026);

        Assert.Contains("""<GAME day="106" time="1830" away="0" home="0" type="4" />""", actual);
        Assert.Contains("</SCHEDULE>", actual);
    }

    [Fact]
    public void BuildSpoofScheduleText_DoesNotDuplicateExplicitAllstarGame()
    {
        var source = """
            <SCHEDULE allstar_game_day="106">
            <GAMES>
            <GAME day="106" time="1830" away="0" home="0" type="4" />
            </GAMES>
            </SCHEDULE>
            """;

        var actual = global::OotpScheduleSpoofer.BuildSpoofScheduleText(source, 2026);

        Assert.Equal(source, actual);
    }

    [Theory]
    [InlineData("""<SCHEDULE allstar_game_day="106">""", true, 106)]
    [InlineData("""<SCHEDULE allstar_game_day="0">""", false, 0)]
    [InlineData("""<SCHEDULE>""", false, 0)]
    public void TryExtractAllstarGameDay_ParsesPositiveHeaderDay(string text, bool expectedResult, int expectedDay)
    {
        var result = global::OotpScheduleSpoofer.TryExtractAllstarGameDay(text, out var day);

        Assert.Equal(expectedResult, result);
        Assert.Equal(expectedDay, day);
    }
}
