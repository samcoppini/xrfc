#include "parser.hpp"

using namespace std::string_literals;

namespace xrf {

namespace {

void parseChunk(FileReader &file, char chunkChar, std::vector<Chunk> &chunks, ParserErrorList &errors) {
    Chunk chunk;
    chunk.line = file.curLine();
    chunk.col = file.curColumn();

    for (std::optional<char> c = chunkChar; c && !std::isspace(*c); c = file.read()) {
        switch (*c) {
            case '0': chunk.commands.push_back(CommandType::Input); break;
            case '1': chunk.commands.push_back(CommandType::Output); break;
            case '2': chunk.commands.push_back(CommandType::Pop); break;
            case '3': chunk.commands.push_back(CommandType::Dup); break;
            case '4': chunk.commands.push_back(CommandType::Swap); break;
            case '5': chunk.commands.push_back(CommandType::Inc); break;
            case '6': chunk.commands.push_back(CommandType::Dec); break;
            case '7': chunk.commands.push_back(CommandType::Add); break;
            case '8': chunk.commands.push_back(CommandType::IgnoreFirst); break;
            case '9': chunk.commands.push_back(CommandType::Bottom); break;
            case 'A': chunk.commands.push_back(CommandType::Jump); break;
            case 'B': chunk.commands.push_back(CommandType::Exit); break;
            case 'C': chunk.commands.push_back(CommandType::IgnoreVisited); break;
            case 'D': chunk.commands.push_back(CommandType::Randomize); break;
            case 'E': chunk.commands.push_back(CommandType::Sub); break;
            case 'F': chunk.commands.push_back(CommandType::Nop); break;
            default:
                errors.emplace_back("Invalid command character: "s + *c, file.curLine(), file.curColumn());
                break;
        }
    }

    if (chunk.commands.size() < COMMANDS_PER_CHUNK) {
        errors.emplace_back("Chunk doesn't have enough commands.", chunk.line, chunk.col);
    }
    else if (chunk.commands.size() > COMMANDS_PER_CHUNK) {
        errors.emplace_back("Chunk has too many commands.", chunk.line, chunk.col);
    }
    else {
        chunks.push_back(std::move(chunk));
    }
}

} // anonymous namespace

std::variant<ParserErrorList, std::vector<Chunk>> parseXrf(FileReader &file) {
    std::vector<Chunk> chunks;
    ParserErrorList errors;

    for (auto c = file.read(); c; c = file.read()) {
        if (!std::isspace(*c)) {
            parseChunk(file, *c, chunks, errors);
        }
    }

    if (errors.empty()) {
        return chunks;
    }
    else {
        return errors;
    }
}

} // namespace xrf
