#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

int help (char* binName) {
    std::cerr << "Usage: " << binName << " <source_file>" << std::endl;
    return 1;
}

const std::string WHITESPACE = " \n\r\t\f\v";

enum instruction_type {
    TYPE_NO_OPER, // ___________
    TYPE_REG_MEM, // R1MMMMMMMMM
    TYPE_ONE_REG, // _________R1
    TYPE_TWO_REG, // R1_______R2
    TYPE_MEM_ADR, // __MMMMMMMMM
    TYPE_REG_IMM  // R1IIIIIIIII
};

typedef struct instruction {
    unsigned char opcode;
    enum instruction_type type;
} instruction_t;

typedef struct mem_data {
    size_t size;
    int value;
} mem_data_t;

std::map<std::string, instruction_t> const opcodes = {
    { "stop",     { 0x00, TYPE_NO_OPER } },
    { "load",     { 0x01, TYPE_REG_MEM } },
    { "store",    { 0x02, TYPE_REG_MEM } },
    { "read",     { 0x03, TYPE_ONE_REG } },
    { "write",    { 0x04, TYPE_ONE_REG } },
    { "add",      { 0x05, TYPE_TWO_REG } },
    { "subtract", { 0x06, TYPE_TWO_REG } },
    { "multiply", { 0x07, TYPE_TWO_REG } },
    { "divide",   { 0x08, TYPE_TWO_REG } },
    { "jump",     { 0x09, TYPE_MEM_ADR } },
    { "jmpz",     { 0x0a, TYPE_REG_MEM } },
    { "jmpn",     { 0x0b, TYPE_REG_MEM } },
    { "move",     { 0x0c, TYPE_TWO_REG } },
    { "push",     { 0x0d, TYPE_ONE_REG } },
    { "pop",      { 0x0e, TYPE_ONE_REG } },
    { "call",     { 0x0f, TYPE_MEM_ADR } },
    { "return",   { 0x10, TYPE_NO_OPER } },
    { "load_s",   { 0x11, TYPE_REG_MEM } },
    { "store_s",  { 0x12, TYPE_REG_MEM } },
    { "loadc",    { 0x13, TYPE_REG_IMM } },
    { "loadi",    { 0x14, TYPE_TWO_REG } },
    { "storei",   { 0x15, TYPE_TWO_REG } },
};

std::string parseLabel(std::string tok) {
    std::size_t endPos = tok.find(":");
    if (tok[0] == '_' && endPos != std::string::npos) {
        return tok.substr(0, endPos);
    }
    return "";
}

/**
 * Faz a etapa 1 do montador para uma linha.
 * @param symbolMap tabela de símbolos
 * @param dataMap tabela de constantes .data
 * @param line linha do programa não vazia e sem comentários
 * @param ilc Instruction Location Counter
 * @returns novo ilc
 **/
size_t parseLineStep1 (
    std::map<std::string, size_t> &symbolMap,
    std::map<std::string, mem_data_t> &dataMap,
    std::string line,
    size_t ilc
) {
    std::istringstream sourceLine(line);
    std::string tok;
    if (sourceLine >> tok) {
        std::string label = parseLabel(tok);

        if (!label.empty()) { // label
            symbolMap[label] = ilc;
            sourceLine >> tok;
        }

        if (tok == ".data") { // pseudo
            size_t size;
            int value;
            sourceLine >> size >> value;
            symbolMap[label] = -1;
            dataMap[label] = { size, value };
            return ilc; // não muda ilc
        }

        const int instructionSize = 2;
        return ilc + instructionSize; // incrementa ilc
    } else {
        return ilc; // linha vazia
    }
}

void parseLineStep2 () {}

int main (int argc, char** argv) {
    if (argc < 2) return help(argv[0]);

    std::map<std::string, size_t> symbolMap;
    std::map<std::string, mem_data_t> dataMap;

    std::ifstream sourceFile;
    sourceFile.open(argv[1]);
    std::string line;

    size_t ilc = 0;
    // 1a passada
    while (std::getline(sourceFile, line)) {
        std::string noCommentLine = line.substr(0, line.find(";"));
        
        size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

        if (firstNonWhiteSpaceIdx != std::string::npos) {
            ilc = parseLineStep1(symbolMap, dataMap, line, ilc);
        }
    }

    // nesse ponto, ilc é o próximo endereço livre da memória

    for (auto data : dataMap) {
        symbolMap[data.first] = ilc;
        ilc += data.second.size;
    }

    std::cout << "Symbol table:\n";
    for (auto a : symbolMap) {
        std::cout << a.first << ": " << a.second << "\n";
    }

    std::cout << "\nData table:\n";
    for (auto a : dataMap) {
        std::cout << a.first << ": " << a.second.size << " " << a.second.value << "\n";
    }

    sourceFile.clear();
    sourceFile.seekg(0);
    
    // 2a passada
    while (std::getline(sourceFile, line)) {
        std::string noCommentLine = line.substr(0, line.find(";"));
        
        size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

        // TODO: parse line (instruction)
    }


    std::cout << "\nFilename: " << argv[1] << std::endl;
    return 0;
}