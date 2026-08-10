#include "icpak.hpp"

#include <iostream>
#include <stdio.h>

#define TOC_VERSION 1
#define ICPAK_VERSION 1
#define ICPAK_INDEX_VERSION 1
#define ICPAK_ASSET_VERSION 1
#define ICPAK_ASSET_HEADER_VERSION 1

struct paker_ctx
{
    bool debug_mode = false;
    int block_size = 1048576;
};

static paker_ctx ctx;

struct depot_manifest
{
    
};

int WritePak(std::string name, std::vector<int> blocks)
{
    FILE* icpakfile = fopen64((std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/staging/paks/" + name + ".icpak").c_str(), "wb");
    if(!icpakfile) return 1;

    for(auto block : blocks)
    {
        FILE* icblockfile = fopen((std::string(ctx.debug_mode ? "depot-debug" : "depot") + "/blocks/" + std::to_string(block) + ".icblock").c_str(), "rb");
        if(!icblockfile) return 2;
        
        int read_size = ctx.block_size / 128;
        std::vector<std::uint8_t> read_buf(read_size);
        size_t ret_code = fread(read_buf.data(), sizeof(std::uint8_t), read_size, icblockfile);
        while(ret_code == read_size)
        {
            fwrite(read_buf.data(), sizeof(std::uint8_t), read_size, icpakfile);
            ret_code = fread(read_buf.data(), sizeof(std::uint8_t), read_size, icblockfile);
        }

        fclose(icblockfile);
    }

    fclose(icpakfile);
    return 0;
}

int paker()
{
    bool exit = true;

    while (!exit)
    {
        std::string command;
    }

    return 0;
}