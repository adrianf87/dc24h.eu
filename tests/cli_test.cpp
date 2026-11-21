#include "dc24h/cli.hpp"

#include <gtest/gtest.h>

#include <array>
#include <sstream>
#include <string>
#include <string_view>

namespace {

class CliTest : public testing::Test {
  protected:
    std::ostringstream output;
    std::ostringstream errors;
};

TEST_F(CliTest, ShowsProjectBannerOnStartup) {
    EXPECT_EQ(dc24h::run_cli({}, output, errors), 0);
    EXPECT_EQ(output.str(), "dc24h.eu — ADC/ADCS Hub — Alpha 0.1\n");
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(CliTest, ShowsHelp) {
    constexpr std::array<std::string_view, 1> arguments{"--help"};

    EXPECT_EQ(dc24h::run_cli(arguments, output, errors), 0);
    EXPECT_NE(output.str().find("Usage: dc24h-hub [--help | --version]"), std::string::npos);
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(CliTest, ShowsReleaseLabelAndVersion) {
    constexpr std::array<std::string_view, 1> arguments{"--version"};

    EXPECT_EQ(dc24h::run_cli(arguments, output, errors), 0);
    EXPECT_EQ(output.str(), "dc24h.eu Alpha 0.1 (0.1.0-alpha.1)\n");
    EXPECT_TRUE(errors.str().empty());
}

TEST_F(CliTest, RejectsUnknownArgument) {
    constexpr std::array<std::string_view, 1> arguments{"--unknown"};

    EXPECT_EQ(dc24h::run_cli(arguments, output, errors), 2);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(errors.str().find("unknown argument '--unknown'"), std::string::npos);
    EXPECT_NE(errors.str().find("dc24h-hub --help"), std::string::npos);
}

TEST_F(CliTest, RejectsMultipleArguments) {
    constexpr std::array<std::string_view, 2> arguments{"--help", "--version"};

    EXPECT_EQ(dc24h::run_cli(arguments, output, errors), 2);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(errors.str().find("expected at most one argument"), std::string::npos);
}

} // namespace
