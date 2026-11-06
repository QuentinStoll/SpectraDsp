#include "AppUpdate.h"
#include "TestRunner.h"

namespace profiler_tests {
namespace {

class AppUpdateTests : public juce::UnitTest {
   public:
    AppUpdateTests() : juce::UnitTest("AppUpdate", "Profiler") {}

    void runTest() override {
        runCase("app update tag order", [this] { testTagOrder(); });
        runCase("app update parse latest", [this] { testParseLatest(); });
    }

   private:
    template <typename Fn>
    void runCase(const juce::String& name, Fn&& fn) {
        if (requestedTestCase.isNotEmpty() && requestedTestCase != name) {
            return;
        }

        beginTest(name);
        fn();
    }

    void testTagOrder() {
        expectEquals(AppUpdate::runNumberFromTag("v2026.11.06-178"), 178);
        expectEquals(AppUpdate::runNumberFromTag("0.0.0-dev"), 0);
        expectEquals(AppUpdate::runNumberFromTag("v2026.11.06-100"), 100);
        expect(AppUpdate::isNewer("v2026.11.06-178", "v2026.11.06-100"));
        expect(!AppUpdate::isNewer("v2026.11.06-100", "v2026.11.06-178"));
        expect(!AppUpdate::isNewer("v2026.11.06-100", "v2026.11.06-100"));
        expect(!AppUpdate::isNewer("not-a-tag", "v2026.11.06-100"));
        expectEquals(AppUpdate::currentTag(), juce::String("v2026.11.06-100"));
    }

    void testParseLatest() {
        const auto json = juce::String(
            R"({"tag_name":"v2026.11.06-178","assets":[)"
            R"({"name":"Spectra-windows-x64.exe","browser_download_url":"https://example.com/win.exe"},)"
            R"({"name":"Spectra-linux-x64.run","browser_download_url":"https://example.com/linux.run"}]})");

        const auto update = AppUpdate::parseLatestJson(json, "Spectra-windows-x64.exe", "v2026.11.06-100");
        expect(update.available);
        expectEquals(update.latestTag, juce::String("v2026.11.06-178"));
        expectEquals(update.installerUrl, juce::String("https://example.com/win.exe"));

        const auto current = AppUpdate::parseLatestJson(json, "Spectra-windows-x64.exe", "v2026.11.06-178");
        expect(!current.available);
        expect(current.message.containsIgnoreCase("up to date"));

        const auto missing = AppUpdate::parseLatestJson(json, "Spectra-macos-arm64.pkg", "v2026.11.06-100");
        expect(!missing.available);
        expect(missing.message.containsIgnoreCase("No installer"));
    }
};

AppUpdateTests appUpdateTests;

}  // namespace
}  // namespace profiler_tests
