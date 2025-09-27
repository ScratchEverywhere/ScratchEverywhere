#include <algorithm>
#include <fstream>
#include <iomanip>
#include <os.hpp>
#include <sstream>
#include <string>
#include <vector>
#include "os.hpp"

#ifdef __3DS__
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#endif

class Linker {
  public:
    static std::string sanitizeString(const std::string &input, bool padWithNull);

    static bool isValidString(const std::string &str);

    static bool installLinker(std::string title, std::string author, std::string romPath, std::string projectName);

    static std::vector<unsigned char> stringToHexWith00(const std::string &input);

    static bool replaceInFile(const std::string &inputFile, const std::string &outputFile, const std::vector<unsigned char> &searchPattern, const std::vector<unsigned char> &replacePattern);
};