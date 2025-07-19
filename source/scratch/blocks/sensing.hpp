#pragma once
#include "../blockExecutor.hpp"

class SensingBlocks{
public:
    static BlockResult resetTimer(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult askAndWait(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult setDragMode(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);

    static Value sensingTimer(Block& block, Sprite* sprite);
    static Value of(Block& block, Sprite* sprite);
    static Value mouseX(Block& block, Sprite* sprite);
    static Value mouseY(Block& block, Sprite* sprite);
    static Value distanceTo(Block& block, Sprite* sprite);
    static Value daysSince2000(Block& block, Sprite* sprite);
    static Value current(Block& block, Sprite* sprite);
    static Value sensingAnswer(Block& block, Sprite* sprite);

    static Value keyPressed(Block& block,Sprite* sprite);
    static Value touchingObject(Block& block, Sprite* sprite);
    static Value mouseDown(Block& block, Sprite* sprite);
    static Value username(Block& block, Sprite* sprite);
};