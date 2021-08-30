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

const std::string WHITESPACE = " \n\r\t\f\v";

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
    char locationType;
    char referenceType;
    size_t address;
    std::vector<size_t> locations;
} reloc_data_t;

typedef struct module_meta {
    size_t dataStart;
    size_t dataEnd;
    std::vector<reloc_data_t> relocTable;
} module_meta_t;

typedef std::vector<unsigned long> module_t;

int main (int argc, char** argv) {
    //if (argc < 3) return help(argv[0]);

    std::map<std::string, size_t> symbolMap;

    std::vector<module_t> modules;
    std::vector<module_meta_t> symTables;

    for (int i=1; i<argc; i++){
        std::ifstream sourceFile;
        module_t module;
        sourceFile.open(argv[i]);

        if (!sourceFile.is_open()) {
            std::cerr << "Unable to open file for reading " << argv[i] << "\n";
            return 2;
        }

        std::string line;
        // Pular cabeçalho
        while(std::getline(sourceFile,line))
            if (line == "BEGIN") break;

        std::getline(sourceFile,line);
        while(std::getline(sourceFile,line)){
            if(line[0] == '[') break;
            
            // get 8 bits
            std::string str2 = line.substr(13,21);
            module.push_back(std::stoul(str2, nullptr, 2));
        }
        sourceFile.close();

        std::ifstream symFile;
        symFile.open(std::string(argv[i]) + ".sym");
        if (!symFile.is_open()) {
            std::cerr << "Unable to open sym file for reading " << argv[i] << ".sym\n";
            return 2;
        }

        module_meta_t metaTable;
        symFile >> line >> line >> line >> metaTable.dataStart >> line >> metaTable.dataEnd;

        // get tabela de relocação
        while(std::getline(symFile, line)){
            std::istringstream iss(line);
            reloc_data_t relocData;
            iss >> relocData.referenceName >> relocData.locationType >> relocData.referenceType;
            if(relocData.locationType == 'G') {
                iss >> relocData.address;
            }else{
                while(iss >> relocData.address){
                    relocData.locations.push_back(relocData.address);
                }
            }
        }

        modules.push_back(module);
        symTables.push_back(metaTable);
        
    }

    // std::ofstream outputFile;
    // std::streambuf *coutBuf = std::cout.rdbuf(); // salva o buffer do cout
    // if (argc == 3) {
    //     outputFile.open(argv[2]);
    //     if (!outputFile.is_open()) {
    //         std::cerr << "Unable to open file for writing " << argv[2] << "\n";
    //         return 2;
    //     }
    //     std::cout.rdbuf(outputFile.rdbuf());
    // }

    // std::string line;

    // size_t ilc = 0;
    // // 1o passo
    // while (std::getline(sourceFile, line)) {
    //     std::string noCommentLine = line.substr(0, line.find(";"));
    //     size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

    //     if (firstNonWhiteSpaceIdx != std::string::npos) {
    //         ilc = parseLineStep1(symbolMap, dataMap, line, ilc);
    //     }
    // }

    // // nesse ponto, ilc é o próximo endereço livre da memória

    // for (auto data : dataMap) {
    //     symbolMap[data.first] = ilc;
    //     ilc += data.second.size;
    // }

    // // volta para começo do arquivo
    // sourceFile.clear();
    // sourceFile.seekg(0);

    // // imprime boilerplate do formato de arquivo de saída
    // std::cout << "DEPTH = 128;\nWIDTH = 8;\nADDRESS_RADIX = HEX;\nDATA_RADIX = BIN;\nCONTENT\nBEGIN\n\n";

    // size_t byteCount = 0;

    // // 2o passo
    // while (std::getline(sourceFile, line)) {
    //     std::string noCommentLine = line.substr(0, line.find(";"));
        
    //     size_t firstNonWhiteSpaceIdx = noCommentLine.find_first_not_of(WHITESPACE);

    //     if (firstNonWhiteSpaceIdx != std::string::npos) {
    //         line = line.substr(firstNonWhiteSpaceIdx, std::string::npos); // remove o whitespace no começo da linha
    //         unsigned int result;
    //         if (parseLineStep2(symbolMap, line, result)) {
    //             unsigned char b1, b2;
    //             b1 = result >> 8;
    //             b2 = result;
    //             std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    //             std::cout << "        :  " << byte2str(b1) << ";              -- " << line << "\n";
    //             byteCount++;
    //             std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    //             std::cout << "        :  " << byte2str(b2) << "; \n";
    //             byteCount++;
    //         }
    //     }
    // }

    // for (auto data : dataMap) {
    //     for (int i = data.second.size - 1; i >= 0; i--) {
    //         std::cout << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    //         std::cout << "        :  " << byte2str((unsigned char) (data.second.value  >> (i*8)));
    //         if ((size_t) i == data.second.size - 1) std::cout << std::dec << ";              -- " << data.first << ": .data " << data.second.size << " " << data.second.value << "\n";
    //         else std::cout << "; \n";
    //         byteCount++;
    //     }
    // }

    // // imprime boilerplate do formato de arquivo de saída
    // std::cout << "[" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << byteCount;
    // std::cout << "..7F]  :  00000000; \nEND; \n";

    // sourceFile.close();
    // std::cout.rdbuf(coutBuf);
    // if (outputFile.is_open()) outputFile.close();
    return 0;
}
