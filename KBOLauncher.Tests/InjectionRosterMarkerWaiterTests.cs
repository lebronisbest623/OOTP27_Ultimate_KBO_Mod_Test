namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class InjectionRosterMarkerWaiterTests
{
    [Theory]
    [InlineData("marker_missing", true)]
    [InlineData("current_save_not_lg", true)]
    [InlineData("process_unavailable", true)]
    [InlineData("current_save_unavailable", false)]
    [InlineData("description_missing", false)]
    [InlineData("description_unreadable", false)]
    [InlineData("save_not_completed", false)]
    [InlineData("save_completion_unreadable", false)]
    [InlineData("save_completion_stale", false)]
    public void IsTerminalRosterMarkerFailure_OnlyStopsWhenWaitingCannotHelp(string status, bool expected)
    {
        var info = global::RosterMarkerInfo.Fail(status, null, null, "test");

        var actual = global::InjectionRosterMarkerWaiter.IsTerminalRosterMarkerFailure(info);

        actual.Should().Be(expected);
    }
}
