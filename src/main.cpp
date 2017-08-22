#include <iostream>
#include <mutex>
#include <vector>

#include "Bar.h"
#include "Block.h"

int main() {
    std::vector<Block*> blocks;
    std::mutex blockMutex;

    Block* block = new Block();
    block->text = L"Yess";
    block->color = 0xffffff;
    blocks.push_back(block);

    Bar bar (&blocks, &blockMutex);


    return 0;
}
