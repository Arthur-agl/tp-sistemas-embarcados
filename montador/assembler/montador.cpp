#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <bitset>

int help (char* binName) {
    std::cerr << "Usage: " << binName << " <source_file> [output_file]" << std::endl;
    return 1;
}

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string INTERNAL_KEY = "!internal";

// Cada caractere representa 1 bit da instrução
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

typedef struct sym_data {
    size_t address;
    bool external;
} sym_data_t;

//Representa dados fixos, definidos pelas labels .data
typedef struct mem_data {
    size_t size;
    long long value;
} mem_data_t;

// entrada da tabela de relocação (ou dados extras)
typedef struct module_meta {
    char type;   // (E)xternal, (G)lobal, (!)meta
    size_t address;
    std::vector<size_t> locations;
} module_meta_t;

std::map<std::string, unsigned int> const registers = {
    { "a0", 0 },
    { "a1", 1 },
    { "a2", 2 },
    { "a3", 3 }
};

std::map<std::string, instruction_t> const opcodes = {
    { "stop",      { 0x00, TYPE_NO_OPER } },
    { "load",      { 0x01, TYPE_REG_MEM } },
    { "store",     { 0x02, TYPE_REG_MEM } },
    { "read",      { 0x03, TYPE_ONE_REG } },
    { "write",     { 0x04, TYPE_ONE_REG } },
    { "add",       { 0x05, TYPE_TWO_REG } },
    { "subtract",  { 0x06, TYPE_TWO_REG } },
    { "multiply",  { 0x07, TYPE_TWO_REG } },
    { "divide",    { 0x08, TYPE_TWO_REG } },
    { "jump",      { 0x09, TYPE_MEM_ADR } },
    { "jmpz",      { 0x0a, TYPE_REG_MEM } },
    { "jmpn",      { 0x0b, TYPE_REG_MEM } },
    { "move",      { 0x0c, TYPE_TWO_REG } },
    { "push",      { 0x0d, TYPE_ONE_REG } },
    { "pop",       { 0x0e, TYPE_ONE_REG } },
    { "call",      { 0x0f, TYPE_MEM_ADR } },
    { "return",    { 0x10, TYPE_NO_OPER } },
    { "load_s",    { 0x11, TYPE_REG_MEM } },
    { "store_s",   { 0x12, TYPE_REG_MEM } },
    { "load_c",    { 0x13, TYPE_REG_IMM } },
    { "load_i",    { 0x14, TYPE_TWO_REG } },
    { "store_i",   { 0x15, TYPE_TWO_REG } },
    { "copytop",   { 0x16, TYPE_ONE_REG } },
};

std::string toLower (std::string str) {
    std::string lowerStr = "";
    for (char c : str) {
        lowerStr += tolower(c);
    }
    return lowerStr;
}

/**
 * Transforma um número sem sinal de 8 bits em uma string contendo sua representação em binário.
 * @param bin número sem sinal de 8 bits
 * @returns representação do número em binário
 **/
std::string byte2str (unsigned char bin) {
    return std::bitset<8>(bin).to_string();
}

/**
 * Extrai a label de um token, caso ela exista
 * @param tok token
 * @returns a label, se existir, ou uma string vazia
 **/
std::string parseLabel (std::string tok) {
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
 * @param referenceMap tabela de referências externas e globais
 * @param line linha do programa não vazia e sem comentários
 * @param ilc Instruction Location Counter
 * @returns novo ilc
 **/
size_t parseLineStep1 (
    std::map<std::string, sym_data_t> &symbolMap,
    std::map<std::string, mem_data_t> &dataMap,
    std::map<std::string, module_meta_t> &referenceMap,

    std::string line,
    size_t ilc
) {
    std::istringstream sourceLine(line);
    std::string tok;
    if (sourceLine >> tok) {
        std::string label = parseLabel(tok);

        if (!label.empty()) { // é definição de label
            symbolMap[label] = { ilc, false };
            sourceLine >> tok;
        }

        // tratamento de pseudo-instruções
        if (tok == ".data") {
            size_t size;
            int value;
            sourceLine >> size >> value;
            symbolMap[label].address = 0; // valor temporário
            dataMap[label] = { size, value };
        } else if (tok == ".externD"){
            std::string ref;
            sourceLine >> ref;
            referenceMap[ref] = {'E', 0};
            symbolMap[ref] = { 0, true };
        } else if (tok == ".externT"){
            std::string ref;
            sourceLine >> ref;
            referenceMap[ref] = {'E', 0};
            symbolMap[ref] = { 0, true };
        } else if (tok == ".globalD"){
            std::string globl;
            sourceLine >> globl;
            referenceMap[globl] = {'G', 0};
        } else if (tok == ".globalT"){
            std::string globl;
            sourceLine >> globl;
            referenceMap[globl] = {'G', 0};
        } else {
            // como os externals são definidos no começo do arquivo é seguro assumir que se existe,
            // ele já está na tabela de relocação (referenceMap).
            while (sourceLine >> tok) { // procura labels na linha
                if (tok[0] == '_') {
                    bool found = referenceMap.find(tok) != referenceMap.end();
                    if (found && referenceMap[tok].type == 'E') { // externa
                        referenceMap[tok].locations.push_back(ilc);
                    } else { // Global ou interna
                        referenceMap[INTERNAL_KEY].locations.push_back(ilc);
                    }
                }
            }

            const int instructionSize = 2;
            return ilc + instructionSize; // incrementa ilc
        }

        return ilc; // não muda o ilc se for uma pseudo-instrução
    } else {
        return ilc; //não incrementa o ilc, a linha é vazia
    }
}

/**
 * Faz a etapa 2 do montador para uma linha.
 * @param symbolMap tabela de símbolos
 * @param referenceMap tabela de referências externas e globais
 * @param line linha do programa não vazia e sem comentários
 * @param output saída já codificada da instrução da linha, caso exista
 * @returns se `output` deve ser considerado ou não como uma codificação de instrução
 **/
bool parseLineStep2 (
    std::map<std::string, sym_data_t> &symbolMap,
    std::map<std::string, module_meta_t> &referenceMap,
    std::string line,
    unsigned int &output
) {
    output = 0;

    std::istringstream sourceLine(line);
    std::string tok;
    if (sourceLine >> tok) {
        std::string label = parseLabel(tok);

        if (!label.empty()) { // ignora label
            sourceLine >> tok;
        }

        if (tok == ".data" || tok == ".externD" || tok == ".externT") { // pseudo
            return false;
        } else if (tok == ".globalD" || tok == ".globalT") {
            sourceLine >> tok;
            referenceMap[tok].address = symbolMap[tok].address;
            return false;
        }

        instruction_t inst = opcodes.at(toLower(tok));

        output |= (inst.opcode << 11); // 11 = instruction_size (16) - opcode_size (5)

        switch (inst.type) {
            case TYPE_ONE_REG:
                sourceLine >> tok;
                output |= registers.at(toLower(tok));
                break;
            
            case TYPE_TWO_REG:
                sourceLine >> tok;
                output |= (registers.at(toLower(tok)) << 9); // 9 = remain_length (11) - register_size (2)
                sourceLine >> tok;
                output |= registers.at(toLower(tok));
                break;

            case TYPE_MEM_ADR:
                sourceLine >> tok;
                if (tok[0] == '_') { // label
                    output |= (symbolMap.at(tok).address);
                } else {
                    output |= (std::stoi(tok) & 0b111111111);
                }
                break;

            case TYPE_REG_MEM:
                sourceLine >> tok;
                output |= (registers.at(toLower(tok)) << 9); // 9 = remain_length (11) - register_size (2)
                sourceLine >> tok;
                if (tok[0] == '_') { // label
                    output |= (symbolMap.at(tok).address);
                } else {
                    output |= (std::stoi(tok) & 0b111111111);
                }
                break;

            case TYPE_REG_IMM:
                sourceLine >> tok;
                output |= (registers.at(toLower(tok)) << 9); // 9 = remain_length (11) - register_size (2)
                sourceLine >> tok;
                output |= (std::stoi(tok) & 0b111111111);
                break;
            case TYPE_NO_OPER:
            default:
                break;
        }

        return true;
    }
    return false;
}

int main (int argc, char** argv) {
    if (argc < 2 || argc > 3) return help(argv[0]);

    std::map<std::string, sym_data_t> symbolMap;
    std::map<std::string, mem_data_t> dataMap;
    std::map<std::string, module_meta_t> referenceMap;

    referenceMap[INTERNAL_KEY] = { '!', 0 };

    std::ifstream sourceFile;
    sourceFile.open(argv[1]);

    if (!sourceFile.is_open()) {
        std::cerr << "Unable to open file for reading " << argv[1] << "\n";
        return 2;
    }

    std::ofstream outputFile;
    std::streambuf *coutBuf = std::cout.rdbuf(); // salva o buffer do cout
    if (argc == 3) {
        outputFile.open(argv[2]);
        if (!outputFile.is_open()) {
            std::cerr << "Unable to open file for writing " << argv[2] << "\n";
            return 2;
        }
        std::cout.rdbuf(outputFile.rdbuf());
    }

    std::ios coutOldState(nullptr);
    coutOldState.copyfmt(std::cout);

    std::string line;

    size_t ilc = 0;
    // 1o passo
    while (std::getline(sourceFile, line)) {
        std::string noCommentLine = line.substr(0, line.find(";"));
        size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

        if (firstNonWhiteSpaceIdx != std::string::npos) {
            ilc = parseLineStep1(symbolMap, dataMap, referenceMap, noCommentLine, ilc);
        }
    }

    // nesse ponto, ilc é o próximo endereço livre da memória

    for (auto &data : dataMap) {
        symbolMap[data.first].address = ilc;
        ilc += data.second.size;
    }

    // volta para começo do arquivo
    sourceFile.clear();
    sourceFile.seekg(0);

    // imprime boilerplate do formato de arquivo de saída
    std::cout << "DEPTH = 128;\nWIDTH = 8;\nADDRESS_RADIX = HEX;\nDATA_RADIX = BIN;\nCONTENT\nBEGIN\n\n";

    size_t byteCount = 0;

    // 2o passo
    while (std::getline(sourceFile, line)) {
        std::string noCommentLine = line.substr(0, line.find(";"));
        
        size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

        if (firstNonWhiteSpaceIdx != std::string::npos) {
            line = line.substr(firstNonWhiteSpaceIdx, std::string::npos); // remove o whitespace no começo da linha
            unsigned int result;
            if (parseLineStep2(symbolMap, referenceMap, line, result)) {
                unsigned char b1, b2;
                b1 = result >> 8;
                b2 = result;
                std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
                std::cout << "        :  " << byte2str(b1) << ";              -- " << line << "\n";
                byteCount++;
                std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
                std::cout << "        :  " << byte2str(b2) << "; \n";
                byteCount++;
            }
        }
    }

    for (auto &data : dataMap) {
        for (int i = data.second.size - 1; i >= 0; i--) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
            std::cout << "        :  " << byte2str((unsigned char) (data.second.value  >> (i*8)));
            if ((size_t) i == data.second.size - 1) std::cout << std::dec << ";              -- " << data.first << ": .data " << data.second.size << " " << data.second.value << "\n";
            else std::cout << "; \n";
            byteCount++;
        }
    }

    // imprime boilerplate do formato de arquivo de saída
    std::cout << "[" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    std::cout << "..7F]  :  00000000; \nEND; \n";

    std::cout.copyfmt(coutOldState);

    std::cout << "\n---- START OF THE RELOCATION TABLE ----\n";
    std::cout << "-- !end: " << byteCount << "\n-- !internal: ";
    for (size_t loc : referenceMap[INTERNAL_KEY].locations) std::cout << loc << " ";
    std::cout << "\n--\n";
    referenceMap.erase(INTERNAL_KEY);
    for (auto &ref : referenceMap) {
        std::cout << "-- " << ref.first << " " << ref.second.type << " ";
        if (ref.second.type == 'G') std::cout << ref.second.address << " ";
        else for (size_t loc : ref.second.locations) std::cout << loc << " ";
        std::cout << "\n";
    }
    std::cout << "---- END OF THE RELOCATION TABLE ----\n";

    sourceFile.close();
    std::cout.rdbuf(coutBuf);
    if (outputFile.is_open()) outputFile.close();
    return 0;
}
