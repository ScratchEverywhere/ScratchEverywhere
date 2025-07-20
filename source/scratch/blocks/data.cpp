#include "data.hpp"

BlockResult DataBlocks::setVariable(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"VALUE",sprite);
    std::string varId = block.fields.at("VARIABLE")[1];
    BlockExecutor::setVariableValue(varId, val, sprite);
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::changeVariable(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"VALUE",sprite);
    std::string varId = block.fields.at("VARIABLE")[1];
    Value oldVariable = BlockExecutor::getVariableValue(varId,sprite);

    if(val.isNumeric() && oldVariable.isNumeric()){
        val = val + oldVariable;
    }

    BlockExecutor::setVariableValue(varId, val, sprite);
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::addToList(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"ITEM", sprite);
    std::string listId = block.fields.at("LIST")[1];
    for(Sprite* currentSprite : sprites){
    if (currentSprite->lists.find(listId) != currentSprite->lists.end()) {
        // std::cout << "Adding to list " << listId << std::endl;
        sprite->lists[listId].items.push_back(val);
        break;
    }
}
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteFromList(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"INDEX", sprite);
    std::string listId = block.fields.at("LIST")[1];

    for (Sprite* currentSprite : sprites) {
        if (currentSprite->lists.find(listId) != currentSprite->lists.end()) {
            //std::cout << "Deleting from list " << listId << std::endl;

            // Convert `val` to an integer index
            if (val.isNumeric()) {
                int index = val.asInt() - 1; // Convert to 0-based index
                auto& items = currentSprite->lists[listId].items;

                // Check if the index is within bounds
                if (index >= 0 && index < static_cast<int>(items.size())) {
                    items.erase(items.begin() + index); // Remove the item at the index
                } else {
                    // std::cerr << "Delete list Index out of bounds: " << index << std::endl;
                }
            } else {
                // std::cerr << "Invalid Delete list index: " << val << std::endl;
            }
            break;
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteAllOfList(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    std::string listId = block.fields.at("LIST")[1];
    for (Sprite* currentSprite : sprites) {
        if (currentSprite->lists.find(listId) != currentSprite->lists.end()) {
            //std::cout << "Deleting all from list " << listId << std::endl;
            currentSprite->lists[listId].items.clear(); // Clear the list
            break;
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::insertAtList(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"ITEM", sprite);
    std::string listId = block.fields.at("LIST")[1];
    Value index = Scratch::getInputValue(block,"INDEX", sprite);

    for (Sprite* currentSprite : sprites) {
        if(!currentSprite) continue;
        if (currentSprite->lists.find(listId) != currentSprite->lists.end()) {
            // std::cout << "Inserting into list " << listId << std::endl;

            // Convert `index` to an integer index
            if (index.isNumeric()) {
                int idx = index.asInt() - 1; // Convert to 0-based index
                auto& items = currentSprite->lists[listId].items;

                // Check if the index is within bounds
                if (idx >= 0 && idx <= static_cast<int>(items.size())) {
                    items.insert(items.begin() + idx, val); // Insert the item at the index
                } else {
                    //std::cerr << "Insert Index out of bounds: " << idx << std::endl;
                }
            } else {
                // std::cerr << "Invalid Insert index: " << index << std::endl;
            }
            break;
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::replaceItemOfList(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value val = Scratch::getInputValue(block,"ITEM", sprite);
    std::string listId = block.fields.at("LIST")[1];
    Value index = Scratch::getInputValue(block,"INDEX",sprite);

    for (Sprite* currentSprite : sprites) {
        if (currentSprite->lists.find(listId) != currentSprite->lists.end()) {
            //std::cout << "Replacing item in list " << listId << std::endl;

            // Convert `index` to an integer index
            if (index.isNumeric()) {
                int idx = index.asInt() - 1; // Convert to 0-based index
                auto& items = currentSprite->lists[listId].items;

                // Check if the index is within bounds
                if (idx >= 0 && idx < static_cast<int>(items.size())) {
                    items[idx] = val; // Replace the item at the index
                } else {
                    // std::cerr << "Replace item Index out of bounds: " << idx << std::endl;
                }
            } else {
                // std::cerr << "Invalid Replace index: " << index << std::endl;
            }
            break;
        }
    }
    return BlockResult::CONTINUE;
}

Value DataBlocks::itemOfList(Block& block, Sprite* sprite) {
    Value indexStr = Scratch::getInputValue(block,"INDEX", sprite);
    int index = indexStr.asInt() - 1;
    std::string listName = block.fields.at("LIST")[1];
    
    for (Sprite* currentSprite : sprites) {
        for (auto& [id, list] : currentSprite->lists) {
            if (id == listName) {
                if (index >= 0 && index < static_cast<int>(list.items.size())) {
                    return Value(Math::removeQuotations(list.items[index].asString()));
                }
                return Value();
            }
        }
    }
    return Value();
}

Value DataBlocks::itemNumOfList(Block& block, Sprite* sprite) {
    std::string listName = block.fields.at("LIST")[1];
    Value itemToFind = Scratch::getInputValue(block,"ITEM",sprite);
    
    for (Sprite* currentSprite : sprites) {
        for (auto& [id, list] : currentSprite->lists) {
            if (id == listName) {
                int index = 1;
                for (auto& item : list.items) {
                    if (Math::removeQuotations(item.asString()) == itemToFind.asString()) {
                        return Value(index);
                    }
                    index++;
                }
            }
        }
    }
    return Value();
}

Value DataBlocks::lengthOfList(Block& block, Sprite* sprite) {
    std::string listName = block.fields.at("LIST")[1];
    for (Sprite* currentSprite : sprites) {
        for (auto& [id, list] : currentSprite->lists) {
            if (id == listName) {
                return Value(static_cast<int>(list.items.size()));
            }
        }
    }
    return Value();
}

Value DataBlocks::listContainsItem(Block& block, Sprite* sprite){
    std::string listName = block.fields.at("LIST")[1];
    Value itemToFind = Scratch::getInputValue(block,"ITEM",sprite);
    
    for (Sprite* currentSprite : sprites) {
        for (auto& [id, list] : currentSprite->lists) {
            if (id == listName) {
                for (const auto& item : list.items) {
                    if (item == itemToFind) {
                        return Value(true);
                    }
                }
            }
        }
    }
    return Value(false);
}