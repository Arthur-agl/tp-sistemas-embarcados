#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <bitset>
#include <vector>

int help (char* binName) {
    std::cerr << "Usage: " << binName << " <object_files>... <output_file>" << std::endl;
    return 1;
}

/**
 * Transforma um número sem sinal de 8 bits em uma string contendo sua representação em binário.
 * @param bin número sem sinal de 8 bits
 * @returns representação do número em binário
 **/
std::string byte2str (unsigned char bin) {
    return std::bitset<8>(bin).to_string();
}

typedef struct reloc_data {
    std::string referenceName;
    char type;
    size_t address;
    std::vector<size_t> locations;
} reloc_data_t;

typedef struct module_meta {
    size_t start;
    size_t end;
    std::vector<reloc_data_t> relocTable;
    std::vector<size_t> internalRelocTable;
} module_meta_t;

typedef std::vector<unsigned long> module_t;

int main (int argc, char** argv) {
    if (argc < 3) return help(argv[0]);

    std::map<std::string, size_t> symbolMap;
    std::vector<module_t> modules;
    std::vector<module_meta_t> symTables;

    std::vector<module_t> outputModule;

    for (int i=1; i<argc - 1; i++){
        std::ifstream mifFile;
        std::string line;
        module_t module;

        mifFile.open(argv[i]);

        if (!mifFile.is_open()) {
            std::cerr << "Unable to open file for reading " << argv[i] << "\n";
            return 2;
        }

        // Pular cabeçalho
        while(std::getline(mifFile,line))
            if (line == "BEGIN") break;
        std::getline(mifFile,line);

        while(std::getline(mifFile,line)){
            if(line[0] == '[') break;
            
            // get 8 bits
            std::string str2 = line.substr(13,21);
            module.push_back(std::stoul(str2, nullptr, 2));
        }

        // Encontra tabela
        while(std::getline(mifFile,line))
            if (line == "---- START OF THE RELOCATION TABLE ----") break;

        module_meta_t symMetaData;
        std::string _discard, tok;

        // Lê a tabela de relocação
        while (std::getline(mifFile, line)) {
            if (line == "") continue;
            if (line == "---- END OF THE RELOCATION TABLE ----") break;
            std::istringstream iss(line);
            iss >> _discard >> tok;
            if (tok == "!end:") {
                iss >> symMetaData.end;
            } else if (tok == "!internal:") {
                size_t addr;
                while (iss >> addr) symMetaData.internalRelocTable.push_back(addr);
            } else if (tok[0] != '!') {
                reloc_data_t relocData;
                relocData.referenceName = tok;
                iss >> relocData.type;
                if (relocData.type == 'G') {
                    iss >> relocData.address;
                } else {
                    while(iss >> relocData.address){
                        relocData.locations.push_back(relocData.address);
                    }
                }
                symMetaData.relocTable.push_back(relocData);
            }
        }

        symTables.push_back(symMetaData);
        modules.push_back(module);
        mifFile.close();
    }

    // acerta o endereço de início e fim de cada módulo, e os endereços das labels globais
    for (size_t i = 1; i < modules.size(); i++) {
        symTables[i].start = symTables[i-1].end;
        symTables[i].end += symTables[i].start;
        for (reloc_data_t &relocData : symTables[i].relocTable) {
            if (relocData.type == 'G') {
                relocData.address += symTables[i].start;
            }
        }
    }

    // substituição das referências internas e externas
    for (size_t i = 0; i < modules.size(); i++) {
        module_t &thisModule = modules[i];
        module_meta_t &symTable = symTables[i];
        // reloca labels internas
        for (size_t addr : symTable.internalRelocTable) {
            // atualiza só os últimos 8 bits do endereço porque a memória tem apenas 128 bytes, o bit 9 é sempre 0
            thisModule[addr + 1] += symTable.start;
        }
        // insere labels externas
        for (reloc_data_t &relocData : symTable.relocTable) {
            if (relocData.type == 'E') { // se label externa
                size_t labelAddr = -1;
                for (module_meta_t &st : symTables) { // procura nas tabelas de símbolos
                    if (labelAddr != -1) break;
                    for (reloc_data_t &rd : st.relocTable) {
                        if (rd.type == 'G' && rd.referenceName == relocData.referenceName) { // achou
                            labelAddr = rd.address;
                            break;
                        }
                    }
                }
                if (labelAddr == -1) { // não achou
                    std::cerr << "Reference not found: " << relocData.referenceName << "\n\tat " << argv[i+1] << '\n';
                    exit(1);
                }

                for (size_t addr : relocData.locations) {
                    // atualiza só os últimos 8 bits do endereço porque a memória tem apenas 128 bytes, o bit 9 é sempre 0
                    thisModule[addr + 1] = labelAddr;
                }
            }
        }
    }

    std::ofstream outputFile;
    std::streambuf *coutBuf = std::cout.rdbuf(); // salva o buffer do cout
    if (std::string(argv[argc - 1]) != "-") {
        outputFile.open(argv[argc - 1]);
        if (!outputFile.is_open()) {
            std::cerr << "Unable to open file for writing " << argv[argc - 1] << "\n";
            return 2;
        }
        std::cout.rdbuf(outputFile.rdbuf());
    }

    // imprime boilerplate do formato de arquivo de saída
    std::cout << "DEPTH = 128;\nWIDTH = 8;\nADDRESS_RADIX = HEX;\nDATA_RADIX = BIN;\nCONTENT\nBEGIN\n\n";
    size_t byteCount = 0;

    for (size_t moduleIndex = 0; moduleIndex < modules.size(); moduleIndex++) {
        module_t &thisModule = modules[moduleIndex];
        for (size_t i = 0; i < thisModule.size(); i++) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
            std::cout << "        :  " << byte2str(thisModule[i]) << ';';
            if (i == 0) std::cout << "              -- start of module " << argv[moduleIndex + 1];
            std::cout << '\n';
            byteCount++;
        }
    }

    // imprime boilerplate do formato de arquivo de saída
    std::cout << "[" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    std::cout << "..7F]  :  00000000; \nEND; \n";

    std::cout.rdbuf(coutBuf);
    if (outputFile.is_open()) outputFile.close();
    return 0;
}
