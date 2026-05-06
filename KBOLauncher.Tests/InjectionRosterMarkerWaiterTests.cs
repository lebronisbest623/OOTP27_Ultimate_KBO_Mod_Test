namespace KBOLauncher.Tests;

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
    public void IsTerminalRosterMarkerFailure_OnlyStopsWhenWaitingCannotHelp(string status, bool expected)
    {
        var info = global::RosterMarkerInfo.Fail(status, null, null, "test");

        var actual = global::InjectionRosterMarkerWaiter.IsTerminalRosterMarkerFailure(info);

        Assert.Equal(expected, actual);
    }
}
