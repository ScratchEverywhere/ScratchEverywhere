#include <iostream>
#include <fstream>
#include <filesystem>
#include "interpret.hpp"

class Unzip{
public:
    static volatile int projectOpened;
    static volatile bool threadFinished;
    static std::string filePath;
    
    static void openScratchProject(void* arg){
        std::ifstream file;
        int isFileOpen = openFile(&file);
        if(isFileOpen == 0){
            std::cerr<<"Failed to open Scratch project."<<std::endl;
            Unzip::projectOpened = -1;
            Unzip::threadFinished = true;
            return;
        }
        else if(isFileOpen == -1){
            std::cout<<"Main Menu Activated."<<std::endl;
            Unzip::projectOpened = -3;
            Unzip::threadFinished = true;
            return;
        } 
        nlohmann::json project_json = unzipProject(&file);
        if(project_json.empty()){
            std::cerr<<"Project.json is empty."<<std::endl;
            Unzip::projectOpened = -2;
            Unzip::threadFinished = true;
            return;
        }
        loadSprites(project_json);
        Unzip::projectOpened = 1;
        Unzip::threadFinished = true;
        return;
    }

    static std::vector<std::string> getProjectFiles(const std::string directory){

        std::vector<std::string> projectFiles;

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".sb3") {
                std::string fileName = entry.path().filename().string();
                projectFiles.push_back(fileName);
            }
        }
    return projectFiles;

    }


    static nlohmann::json unzipProject(std::ifstream *file){

    nlohmann::json project_json;

    if(projectType != UNZIPPED){
        // read the file
        std::cout<<"Reading SB3..."<<std::endl;
        std::streamsize size = file->tellg(); // gets the size of the file
        file->seekg(0,std::ios::beg); // go to the beginning of the file
        std::vector<char> buffer(size);
        if (!file->read(buffer.data(), size)){
            return project_json;
        }

        // open ZIP file from the thing that we just did
        std::cout<<"Opening SB3 file..."<<std::endl;
        mz_zip_archive zip;
        memset(&zip,0,sizeof(zip));
        if (!mz_zip_reader_init_mem(&zip,buffer.data(),buffer.size(),0)){
            return project_json;
        }

        // extract project.json
        std::cout<<"Extracting project.json..."<<std::endl;
        int file_index = mz_zip_reader_locate_file(&zip,"project.json",NULL,0);
        if (file_index < 0){
            return project_json;
        }

        size_t json_size;
        const char* json_data = static_cast<const char*>(mz_zip_reader_extract_to_heap(&zip, file_index, &json_size, 0));

        // Parse JSON file
        std::cout<<"Parsing project.json..."<<std::endl;
        project_json = nlohmann::json::parse(std::string(json_data,json_size));
        mz_free((void*)json_data);

        Image::loadImages(&zip);
        mz_zip_reader_end(&zip);
    }
    else {
        // if project is unzipped
    file->clear(); // Clear any EOF flags
    file->seekg(0, std::ios::beg); // Go to the start of the file
    (*file) >> project_json;
}

    return project_json;
    }

    static int openFile(std::ifstream *file);

    static bool load();

};