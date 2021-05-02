#include "parser.hpp"

#include "catch2/catch.hpp"

#include <sstream>

TEST_CASE("Parser parses chunks correctly", "[parser]") {
    std::stringstream code{"01234 56789\n\n ABCDE FFFFF"};
    xrf::FileReader reader{code};

    auto parseResult = xrf::parseXrf(reader);

    REQUIRE(std::holds_alternative<std::vector<xrf::Chunk>>(parseResult));

    auto chunks = std::get<std::vector<xrf::Chunk>>(parseResult);

    REQUIRE(chunks.size() == 4);

    auto chunk1 = chunks[0];
    CHECK(chunk1.line == 1);
    CHECK(chunk1.col == 1);
    REQUIRE(chunk1.commands.size() == 5);
    CHECK(chunk1.commands[0] == xrf::CommandType::Input);
    CHECK(chunk1.commands[1] == xrf::CommandType::Output);
    CHECK(chunk1.commands[2] == xrf::CommandType::Pop);
    CHECK(chunk1.commands[3] == xrf::CommandType::Dup);
    CHECK(chunk1.commands[4] == xrf::CommandType::Swap);

    auto chunk2 = chunks[1];
    CHECK(chunk2.line == 1);
    CHECK(chunk2.col == 7);
    REQUIRE(chunk2.commands.size() == 5);
    CHECK(chunk2.commands[0] == xrf::CommandType::Inc);
    CHECK(chunk2.commands[1] == xrf::CommandType::Dec);
    CHECK(chunk2.commands[2] == xrf::CommandType::Add);
    CHECK(chunk2.commands[3] == xrf::CommandType::IgnoreFirst);
    CHECK(chunk2.commands[4] == xrf::CommandType::Bottom);

    auto chunk3 = chunks[2];
    CHECK(chunk3.line == 3);
    CHECK(chunk3.col == 2);
    REQUIRE(chunk3.commands.size() == 5);
    CHECK(chunk3.commands[0] == xrf::CommandType::Jump);
    CHECK(chunk3.commands[1] == xrf::CommandType::Exit);
    CHECK(chunk3.commands[2] == xrf::CommandType::IgnoreVisited);
    CHECK(chunk3.commands[3] == xrf::CommandType::Randomize);
    CHECK(chunk3.commands[4] == xrf::CommandType::Sub);

    auto chunk4 = chunks[3];
    CHECK(chunk4.line == 3);
    CHECK(chunk4.col == 8);
    REQUIRE(chunk4.commands.size() == 5);
    CHECK(chunk4.commands[0] == xrf::CommandType::Nop);
    CHECK(chunk4.commands[1] == xrf::CommandType::Nop);
    CHECK(chunk4.commands[2] == xrf::CommandType::Nop);
    CHECK(chunk4.commands[3] == xrf::CommandType::Nop);
    CHECK(chunk4.commands[4] == xrf::CommandType::Nop);
}

TEST_CASE("Parser rejects chunk that is too long", "[parser]") {
    std::stringstream code{"000000"};
    xrf::FileReader reader{code};

    auto parseResult = xrf::parseXrf(reader);

    REQUIRE(std::holds_alternative<xrf::ParserErrorList>(parseResult));

    auto errors = std::get<xrf::ParserErrorList>(parseResult);

    REQUIRE(errors.size() == 1);

    auto error = errors[0];
    CHECK(error.line == 1);
    CHECK(error.col == 1);
    CHECK(error.msg.find("too many commands") != std::string::npos);
}

TEST_CASE("Parser rejects chunk that is too short", "[parser]") {
    std::stringstream code{"   FFFF   "};
    xrf::FileReader reader{code};

    auto parseResult = xrf::parseXrf(reader);

    REQUIRE(std::holds_alternative<xrf::ParserErrorList>(parseResult));

    auto errors = std::get<xrf::ParserErrorList>(parseResult);

    REQUIRE(errors.size() == 1);

    auto error = errors[0];
    CHECK(error.line == 1);
    CHECK(error.col == 4);
    CHECK(error.msg.find("doesn't have enough commands") != std::string::npos);
}

TEST_CASE("Parser rejects chunk with invalid commands", "[parser]") {
    std::stringstream code{"0G1234"};
    xrf::FileReader reader{code};

    auto parseResult = xrf::parseXrf(reader);

    REQUIRE(std::holds_alternative<xrf::ParserErrorList>(parseResult));

    auto errors = std::get<xrf::ParserErrorList>(parseResult);

    REQUIRE(errors.size() == 1);

    auto error = errors[0];
    CHECK(error.line == 1);
    CHECK(error.col == 2);
    CHECK(error.msg.find("invalid command character") != std::string::npos);

}
