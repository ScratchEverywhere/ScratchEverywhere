#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <iostream>
#include <list>
#include <cmath>
#include <vector>
#include <chrono>
#include <random>
#include <time.hpp>
#include "blockExecutor.hpp"
#include "image.hpp"
#include "math.hpp"
#include "sprite.hpp"



enum ProjectType{
	UNZIPPED,
	EMBEDDED,
	UNEMBEDDED
};

class BlockExecutor;
extern BlockExecutor executor;

extern ProjectType projectType;

extern std::vector<Sprite*> sprites;
extern std::vector<Sprite> spritePool;
extern std::vector<std::string> broadcastQueue;
//extern std::unordered_map<std::string,Conditional> conditionals;
extern std::unordered_map<std::string, Block*> blockLookup;
extern bool toExit;
extern std::string answer;

class Scratch{
public:
    static Value getInputValue(Block& block, const std::string& inputName, Sprite* sprite);

    static int projectWidth;
    static int projectHeight;
    static int FPS;
};


std::vector<std::pair<double, double>> getCollisionPoints(Sprite* currentSprite);
void loadSprites(const nlohmann::json& json);
void cleanupSprites();
Block* getBlockParent(const Block* block);
void initializeSpritePool(int poolSize);
Sprite* getAvailableSprite();
void initializeSpritePool(int poolSize);
Block* findBlock(std::string blockId);

std::vector<Block*> getBlockChain(std::string blockId,std::string* outID = nullptr);
