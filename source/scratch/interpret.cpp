#include "interpret.hpp"
#include "render.hpp"

std::vector<Sprite*> sprites;
std::vector<Sprite> spritePool;
std::vector<std::string> broadcastQueue;
//std::unordered_map<std::string,Conditional> conditionals;
std::unordered_map<std::string, Block*> blockLookup;
std::string answer;
bool toExit = false;
ProjectType projectType;

BlockExecutor executor;

int Scratch::projectWidth = 480;
int Scratch::projectHeight = 360;
int Scratch::FPS = 30;

std::string generateRandomString(int length) {
    std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-=[];',./_+{}|:<>?~`";
    std::string result;

    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);

    for (int i = 0; i < length; i++) {
        result += chars[distribution(generator)];
    }

    return result;


}

std::string removeQuotations(std::string value) {
    value.erase(std::remove_if(value.begin(),value.end(),[](char c){return c == '"';}),value.end());
    return value;
}

void initializeSpritePool(int poolSize) {
    for (int i = 0; i < poolSize; i++) {
        Sprite newSprite;
        newSprite.id = generateRandomString(15);
        newSprite.isClone = true;
        newSprite.toDelete = true;
        newSprite.isDeleted = true;
        spritePool.push_back(newSprite);
    }
}

Sprite* getAvailableSprite() {
    for (Sprite& sprite : spritePool) {
        if (sprite.isDeleted) {
            sprite.isDeleted = false;
            sprite.toDelete = false;
            return &sprite;
        }
    }
    return nullptr;  // No available sprites
}

void cleanupSprites() {
    for (Sprite* sprite : sprites) {
        delete sprite;
    }

    spritePool.clear();
    sprites.clear();
}

std::vector<std::pair<double, double>> getCollisionPoints(Sprite* currentSprite) {
    std::vector<std::pair<double, double>> collisionPoints;

    // Get sprite dimensions, scaled by size
    double halfWidth = (currentSprite->spriteWidth * currentSprite->size / 100.0) / 2.0;
    double halfHeight = (currentSprite->spriteHeight * currentSprite->size / 100.0) / 2.0;

    // Calculate rotation in radians
    double rotationRadians = (currentSprite->rotation - 90) * M_PI / 180.0;

    // Define the four corners relative to the sprite's center
    std::vector<std::pair<double, double>> corners = {
        { -halfWidth, -halfHeight }, // Top-left
        { halfWidth, -halfHeight },  // Top-right
        { halfWidth, halfHeight },   // Bottom-right
        { -halfWidth, halfHeight }   // Bottom-left
    };

    // Rotate and translate each corner
    for (const auto& corner : corners) {
        double rotatedX = corner.first * cos(rotationRadians) - corner.second * sin(rotationRadians);
        double rotatedY = corner.first * sin(rotationRadians) + corner.second * cos(rotationRadians);

        collisionPoints.emplace_back(
            currentSprite->xPosition + rotatedX,
            currentSprite->yPosition + rotatedY
        );
    }

    return collisionPoints;
}

void loadSprites(const nlohmann::json& json){
    std::cout<<"Beginning to load sprites..."<< std::endl;
    sprites.reserve(400);
    int count = 0;
    for (const auto& target : json["targets"]){ // "target" is sprite in Scratch speak, so for every sprite in sprites
    
        Sprite* newSprite = new Sprite();
        if(target.contains("name")){
        newSprite->name = target["name"].get<std::string>();}
        newSprite->id = generateRandomString(15);
        if(target.contains("isStage")){
        newSprite->isStage = target["isStage"].get<bool>();}
        if(target.contains("draggable")){
        newSprite->draggable = target["draggable"].get<bool>();}
        if(target.contains("visible")){
        newSprite->visible = target["visible"].get<bool>();}
        else newSprite->visible = true;
        if(target.contains("currentCostume")){
        newSprite->currentCostume = target["currentCostume"].get<int>();}
        if(target.contains("volume")){
        newSprite->volume = target["volume"].get<int>();}
        if(target.contains("x")){
        newSprite->xPosition = target["x"].get<int>();}
        if(target.contains("y")){
        newSprite->yPosition = target["y"].get<int>();}
        if(target.contains("size")){
        newSprite->size = target["size"].get<int>();}
        else newSprite->size = 100;
        if(target.contains("direction")){
        newSprite->rotation = target["direction"].get<int>();}
        else newSprite->rotation = 90;
        if(target.contains("layerOrder")){
        newSprite->layer = target["layerOrder"].get<int>();}
        else newSprite->layer = 0;
        if(target.contains("rotationStyle")){
            if(target["rotationStyle"].get<std::string>() == "all around")
            newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if(target["rotationStyle"].get<std::string>() == "left-right")
            newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
            newSprite->rotationStyle = newSprite->NONE;
    }
        newSprite->toDelete = false;
        newSprite->isClone = false;
       // std::cout<<"name = "<< newSprite.name << std::endl;


        // set variables
        for (const auto& [id,data] : target["variables"].items()){
            
            Variable newVariable;
            newVariable.id = id;
            newVariable.name = data[0];
            newVariable.value = Value::fromJson(data[1]);
            newSprite->variables[newVariable.id] = newVariable; // add variable to sprite
        }

        // set Blocks
        for(const auto& [id,data] : target["blocks"].items()){

            Block newBlock;
            newBlock.id = id;
            if (data.contains("opcode")){
            newBlock.opcode = newBlock.stringToOpcode(data["opcode"].get<std::string>());}
            if (data.contains("next") && !data["next"].is_null()){
            newBlock.next = data["next"].get<std::string>();}
            if (data.contains("parent") && !data["parent"].is_null()){
            newBlock.parent = data["parent"].get<std::string>();}
            else newBlock.parent = "null";
            if (data.contains("fields")){
            newBlock.fields = data["fields"];}
            if (data.contains("inputs")){

                for(const auto& [inputName,inputData] : data["inputs"].items()){
                    ParsedInput parsedInput;
                    parsedInput.originalJson = inputData;

                    int type = inputData[0];
                    auto& inputValue = inputData[1];


                    if(type == 1){
                        parsedInput.inputType = ParsedInput::LITERAL;
                        //std::cout << "doing it! " << inputValue.dump() << std::endl;
                        parsedInput.literalValue = Value::fromJson(inputValue);
                        //std::cout << "literal value = " << parsedInput.literalValue.asString();

                    } else if(type == 3){
                        if(inputValue.is_array()){
                            parsedInput.inputType = ParsedInput::VARIABLE;
                            parsedInput.variableId = inputValue[2].get<std::string>();
                        } else{
                            parsedInput.inputType = ParsedInput::BLOCK;
                            parsedInput.blockId = inputValue.get<std::string>();
                        }
                    } else if(type == 2){
                        parsedInput.inputType = ParsedInput::BOOLEAN;
                        parsedInput.blockId = inputValue.get<std::string>();
                    }
                    newBlock.parsedInputs[inputName] = parsedInput;
                    //std::cout << "input: " << inputName << ". type = " << parsedInput.inputType << std::endl;
                }

        }
            if (data.contains("topLevel")){
            newBlock.topLevel = data["topLevel"].get<bool>();}
            if (data.contains("shadow")){
            newBlock.shadow = data["shadow"].get<bool>();}
            if (data.contains("mutation")){
                newBlock.mutation = data["mutation"];
            }
            newSprite->blocks[newBlock.id] = newBlock; // add block


            
            // add custom function blocks
            if(newBlock.opcode == newBlock.PROCEDURES_PROTOTYPE){
                CustomBlock newCustomBlock;
                newCustomBlock.name = data["mutation"]["proccode"];
                newCustomBlock.blockId = newBlock.id;

                // custom blocks uses a different json structure for some reason?? have to parse them.
                std::string rawArgumentNames = data["mutation"]["argumentnames"];
                nlohmann::json parsedAN = nlohmann::json::parse(rawArgumentNames);
                newCustomBlock.argumentNames = parsedAN.get<std::vector<std::string>>();

                std::string rawArgumentDefaults = data["mutation"]["argumentdefaults"];
                nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
                //newCustomBlock.argumentDefaults = parsedAD.get<std::vector<std::string>>();

                for (const auto& item : parsedAD) {
                    if (item.is_string()) {
                        newCustomBlock.argumentDefaults.push_back(item.get<std::string>());
                    } else if (item.is_number_integer()) {
                        newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<int>()));
                    } else if (item.is_number_float()) {
                        newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<double>()));
                    } else {
                        newCustomBlock.argumentDefaults.push_back(item.dump());
                    }
                }

                std::string rawArgumentIds = data["mutation"]["argumentids"];
                nlohmann::json parsedAID = nlohmann::json::parse(rawArgumentIds);
                newCustomBlock.argumentIds = parsedAID.get<std::vector<std::string>>();

                if(data["mutation"]["warp"] == "true"){
                newCustomBlock.runWithoutScreenRefresh = true;}
                else newCustomBlock.runWithoutScreenRefresh = false;

                newSprite->customBlocks[newCustomBlock.name] = newCustomBlock; // add custom block
            }

        }



        // set Lists
        for(const auto &[id,data] : target["lists"].items()){
            List newList;
            newList.id = id;
            newList.name = data[0];
            for(const auto &listItem : data[1]){
            newList.items.push_back(Value::fromJson(listItem));
            }
            newSprite->lists[newList.id] = newList; // add list
        }

        // set Sounds
        for(const auto &[id,data] : target["sounds"].items()){
            Sound newSound;
            newSound.id = data["assetId"];
            newSound.name = data["name"];
            newSound.fullName = data["md5ext"];
            newSound.dataFormat = data["dataFormat"];
            newSound.sampleRate = data["rate"];
            newSound.sampleCount = data["sampleCount"];
            newSprite->sounds[newSound.id] = newSound;
        }

        // set Costumes
        for(const auto &[id,data] : target["costumes"].items()){
            Costume newCostume;
            newCostume.id = data["assetId"];
            if(data.contains("name")){
            newCostume.name = data["name"];}
            if(data.contains("bitmapResolution")){
            newCostume.bitmapResolution = data["bitmapResolution"];}
            if(data.contains("dataFormat")){
            newCostume.dataFormat = data["dataFormat"];}
            if(data.contains("md5ext")){
            newCostume.fullName = data["md5ext"];}
            if(data.contains("rotationCenterX")){
            newCostume.rotationCenterX = data["rotationCenterX"];}
            if(data.contains("rotationCenterY")){
            newCostume.rotationCenterY = data["rotationCenterY"];}
            newSprite->costumes.push_back(newCostume);
        }

       // set comments
        for(const auto &[id,data] : target["comments"].items()){
            Comment newComment;
            newComment.id = id;
            if(data.contains("blockId") && !data["blockId"].is_null()){
            newComment.blockId = data["blockId"];}
            newComment.width = data["width"];
            newComment.height = data["height"];
            newComment.minimized = data["minimized"];
            newComment.x = data["x"];
            newComment.y = data["y"];
            newComment.text = data["text"];
            newSprite->comments[newComment.id] = newComment;
        }

        // set Broadcasts
        for(const auto &[id,data] : target["broadcasts"].items()){
            Broadcast newBroadcast;
            newBroadcast.id = id;
            newBroadcast.name = data;
            newSprite->broadcasts[newBroadcast.id] = newBroadcast;
           // std::cout<<"broadcast name = "<< newBroadcast.name << std::endl;
        }

        sprites.push_back(newSprite);
        count++;


    }

    // load block lookup table
    blockLookup.clear();
    for (Sprite* sprite : sprites) {
        for (auto& [id, block] : sprite->blocks) {
            blockLookup[id] = &block;
        }
    }
    // setup top level blocks
    for (Sprite* currentSprite : sprites) {
    for(auto& [id,block]: currentSprite->blocks){
        if(block.topLevel) continue; // skip top level blocks
        block.topLevelParentBlock = getBlockParent(&block)->id; // get parent block id
        //std::cout<<"block id = "<< block.topLevelParentBlock << std::endl;
    }
}

    // try to find the advanced project settings comment
    nlohmann::json config;
    for (Sprite* currentSprite : sprites) {
        if(!currentSprite->isStage) continue;
        for(auto& [id,comment] : currentSprite->comments){
        std::size_t json_start = comment.text.find('{');
        if (json_start == std::string::npos) continue;

        std::string json_str = comment.text.substr(json_start);
        std::size_t json_end = json_str.find("}");
        if (json_end != std::string::npos) {
            json_str = json_str.substr(0, json_end + 1);}
        else continue;

        try {
            config = nlohmann::json::parse(json_str);
            //std::cout << "Parsed JSON:\n" << config.dump(4) << "\n";
            break;
    } catch (nlohmann::json::parse_error& e) {
        //std::cerr << "Failed to parse JSON: " << e.what() << "\n";
        continue;
    }
        }
    }
    // set advanced project settings properties
    int wdth = 0;
    int hght = 0;
    int framerate = 0;

    try{
       framerate = config["framerate"].get<int>();
       Scratch::FPS = framerate;
    }
    catch(...){
        //std::cerr << "no framerate property." << std::endl;
    }
        try{
       wdth = config["width"].get<int>();
       Scratch::projectWidth = wdth;
    }
    catch(...){
        //std::cerr << "no width property." << std::endl;
    }
        try{
       hght = config["height"].get<int>();
       Scratch::projectHeight = hght;
    }
    catch(...){
        //std::cerr << "no height property." << std::endl;
    }

    if(wdth == 400 && hght == 480)
    Render::renderMode = Render::BOTH_SCREENS;
    else if(wdth == 320 && hght == 240)
    Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
    Render::renderMode = Render::TOP_SCREEN_ONLY;
    
    // if unzipped, load initial sprites
    if(projectType == UNZIPPED){
        for(auto& currentSprite : sprites){
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].id);
        }
    }


    initializeSpritePool(300);

    // get block chains for every block
    for (Sprite* currentSprite : sprites) {
    for(auto& [id,block]: currentSprite->blocks){
        if(!block.topLevel) continue;
        std::string outID;
        BlockChain chain;
        chain.blockChain = getBlockChain(block.id,&outID);
        currentSprite->blockChains[outID] = chain;
        //std::cout << "ok = " << outID << std::endl;
        block.blockChainID = outID;

        for(auto& chainBlock : chain.blockChain) {
            if(currentSprite->blocks.find(chainBlock->id) != currentSprite->blocks.end()) {
                currentSprite->blocks[chainBlock->id].blockChainID = outID;
            }
        }

    }
}

    std::cout<<"Loaded " << sprites.size() << " sprites."<< std::endl;
}



Block* findBlock(std::string blockId){


    auto block = blockLookup.find(blockId);
    if(block != blockLookup.end()) {
        return block->second;
    }


    return nullptr;
}

std::vector<Block*> getBlockChain(std::string blockId,std::string* outID){
    std::vector<Block*> blockChain;
    Block* currentBlock = findBlock(blockId);
    while(currentBlock != nullptr){
        blockChain.push_back(currentBlock);
        if(outID)
        *outID += currentBlock->id;
        if(!currentBlock->parsedInputs["SUBSTACK"].originalJson[1].is_null()){
            std::vector<Block*> subBlockChain;
            subBlockChain = getBlockChain(currentBlock->parsedInputs["SUBSTACK"].originalJson[1],outID);
            for(auto& block : subBlockChain){
                blockChain.push_back(block);
                if(outID)
                *outID += block->id;
            }
        }
        if(!currentBlock->parsedInputs["SUBSTACK2"].originalJson[1].is_null()){
            std::vector<Block*> subBlockChain;
            subBlockChain = getBlockChain(currentBlock->parsedInputs["SUBSTACK2"].originalJson[1],outID);
            for(auto& block : subBlockChain){
                blockChain.push_back(block);
                if(outID)
                *outID += block->id;
            }
        }
        currentBlock = findBlock(currentBlock->next);
    }
    return blockChain;
}

Block* getBlockParent(const Block* block){
    Block* parentBlock;
    const Block* currentBlock = block;
    while(currentBlock->parent != "null"){
        parentBlock = findBlock(currentBlock->parent);
        if(parentBlock != nullptr){
            currentBlock = parentBlock;
        }
        else{
            break;
        }
    }
    return const_cast<Block*>(currentBlock);
}




Value findCustomValue(std::string valueName, Sprite* sprite, Block block) {
    for (auto& [custId, custBlock] : sprite->customBlocks) {

        auto it = std::find(custBlock.argumentNames.begin(), custBlock.argumentNames.end(), valueName);
        
        if (it != custBlock.argumentNames.end()) {
            size_t index = std::distance(custBlock.argumentNames.begin(), it);

            if (index < custBlock.argumentIds.size()) {
                std::string argumentId = custBlock.argumentIds[index];

                auto valueIt = custBlock.argumentValues.find(argumentId);
                if (valueIt != custBlock.argumentValues.end()) {
                   //std::cout << "FOUND that shit BAAANG: " << valueIt->second.asString() << std::endl;
                    return valueIt->second;
                } else {
                   std::cout << "Argument ID found, but no value exists for it." << std::endl;
                }
            } else {
              std::cout << "Index out of bounds for argumentIds!" << std::endl;
            }
        }
    }
    return Value();
}

void runCustomBlock(Sprite* sprite,Block& block, Block* callerBlock,bool* withoutScreenRefresh){
    for(auto &[id, data] : sprite->customBlocks){
        if(id == block.mutation.at("proccode").get<std::string>()){
            // Set up argument values
            for(std::string arg : data.argumentIds){
                if(block.parsedInputs.find(arg) != block.parsedInputs.end()){
                    data.argumentValues[arg] = Scratch::getInputValue(block, arg,sprite);
                }
            }
            
            //std::cout << "running custom block " << data.blockId << std::endl;
            
            // Get the parent of the prototype block (the definition containing all blocks)
            Block* customBlockDefinition = &sprite->blocks[sprite->blocks[data.blockId].parent];
            
            callerBlock->customBlockPtr = customBlockDefinition;

            bool localWithoutRefresh = data.runWithoutScreenRefresh;

            
            // If the parent chain is running without refresh, force this one to also run without refresh
            if(!localWithoutRefresh && withoutScreenRefresh != nullptr) {
                localWithoutRefresh = *withoutScreenRefresh;
            }
            
            //std::cout << "RWSR = " << localWithoutRefresh << std::endl;
            
            // Execute the custom block definition
            executor.runBlock(*customBlockDefinition, sprite, nullptr, &localWithoutRefresh);

            if(localWithoutRefresh){
                BlockExecutor::runRepeatsWithoutRefresh(sprite,customBlockDefinition->blockChainID);
            }
            
            break;
        }
    }
}




void setVariableValue(const std::string& variableId, const Value& newValue, Sprite* sprite) {
    // Set sprite variable
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        it->second.value = newValue;
        return;
    }
    
    // Set global variable
    for (auto& currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) {
                globalIt->second.value = newValue;
                return;
            }
        }
    }
}

Value getVariableValue(std::string variableId, Sprite* sprite) {
    // Check sprite variables
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        return it->second.value;  // Fast conversion
    }
    
    // Check lists
    auto listIt = sprite->lists.find(variableId);
    if (listIt != sprite->lists.end()) {
        std::string result;
        for (const auto& item : listIt->second.items) {
            result += item.asString() + " ";
        }
        if (!result.empty()) result.pop_back();
        Value val(result);
        return val;
    }
    
    // Check global variables
    for (const auto& currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) {
                return globalIt->second.value;
            }
        }
    }
    
    return Value(0);
}

Value Scratch::getInputValue(Block& block, const std::string& inputName, Sprite* sprite){
        auto parsedFind = block.parsedInputs.find(inputName);

        if (parsedFind == block.parsedInputs.end()) {
            return Value(0);
        }
        
        const ParsedInput& input = parsedFind->second;
        switch(input.inputType) {

            case ParsedInput::LITERAL:
                return input.literalValue;
                
            case ParsedInput::VARIABLE:
                return getVariableValue(input.variableId, sprite);
                
            case ParsedInput::BLOCK:
                return executor.getBlockValue(*findBlock(input.blockId), sprite);
                
            case ParsedInput::BOOLEAN:
                return executor.getBlockValue(*findBlock(input.blockId), sprite);
                
        }
        return Value(0);
    }




std::vector<Sprite*> findSprite(std::string spriteName){
    std::vector<Sprite*> sprts;
    for(Sprite* currentSprite : sprites){
        if(currentSprite->name == spriteName){
            sprts.push_back(currentSprite);
        }
    }
    return sprts;
}



