//
//  main.cpp
//  make_spiffs
//
//  Created by Ivan Grokhotkov on 13/05/15.
//  Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.
//
#define TCLAP_SETBASE_ZERO 1

#include <iostream>
#include "spiffs/spiffs.h"
#include <vector>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <memory>
#include <cstdlib>
#include "tclap/CmdLine.h"
#include "tclap/UnlabeledValueArg.h"

static std::vector<uint8_t> s_flashmem;

static std::string s_dirName;
static std::string s_imageName;
static int s_imageSize;
static int s_pageSize;
static int s_blockSize;

enum Action { ACTION_NONE, ACTION_PACK, ACTION_LIST, ACTION_VISUALIZE };
static Action s_action = ACTION_NONE;

static spiffs s_fs;

static std::vector<uint8_t> s_spiffsWorkBuf;
static std::vector<uint8_t> s_spiffsFds;
static std::vector<uint8_t> s_spiffsCache;


static s32_t api_spiffs_read(u32_t addr, u32_t size, u8_t *dst)
{
    memcpy(dst, &s_flashmem[0] + addr, size);
    return SPIFFS_OK;
}

static s32_t api_spiffs_write(u32_t addr, u32_t size, u8_t *src)
{
    memcpy(&s_flashmem[0] + addr, src, size);
    return SPIFFS_OK;
}

static s32_t api_spiffs_erase(u32_t addr, u32_t size)
{
    memset(&s_flashmem[0] + addr, 0xff, size);
    return SPIFFS_OK;
} 

void checkCallback(spiffs_check_type type, spiffs_check_report report,
                                   u32_t arg1, u32_t arg2)
{
}

void spiffsMount(bool init)
{
    spiffs_config cfg = {0};
    
    cfg.phys_addr = 0x0000;
    cfg.phys_size = (u32_t) s_flashmem.size();
        
    cfg.phys_erase_block = s_blockSize;
    cfg.log_block_size = s_blockSize;
    cfg.log_page_size = s_pageSize;
    
    cfg.hal_read_f = api_spiffs_read;
    cfg.hal_write_f = api_spiffs_write;
    cfg.hal_erase_f = api_spiffs_erase;
    
    const int maxOpenFiles = 4;
    s_spiffsWorkBuf.resize(s_pageSize * 2);
    s_spiffsFds.resize(32 * maxOpenFiles);
    s_spiffsCache.resize((32 + s_pageSize) * maxOpenFiles);


    SPIFFS_mount(&s_fs, &cfg, 
        &s_spiffsWorkBuf[0], 
        &s_spiffsFds[0], s_spiffsFds.size(),
        &s_spiffsCache[0], s_spiffsCache.size(),
        NULL);
    
//    debugf("mount res: %d\n", res);
    if (init) {
        spiffs_file fd = SPIFFS_open(&s_fs, (char*) "tmp", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
        uint8_t tmp = 0xff;
        SPIFFS_write(&s_fs, fd, &tmp, 1);
        SPIFFS_fremove(&s_fs, fd);
        SPIFFS_close(&s_fs, fd);
    } else {
        s_fs.check_cb_f = &checkCallback;
        //SPIFFS_check(&s_fs);
    }
}

void spiffsUnmount()
{
    SPIFFS_unmount(&s_fs);
}

int addFile(char* name, const char* path) {
    const size_t write_size = 512;
    uint8_t write_block[write_size];
    
    FILE* src = fopen(path, "rb");
    if (!src) {
        std::cerr << "error: failed to open " << path << " for reading" << std::endl;
        return 1;
    }
    
    spiffs_file dst = SPIFFS_open(&s_fs, name, SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
    
    fseek(src, 0, SEEK_END);
    size_t size = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    size_t left = size;
    while (left > 0)
    {
        size_t will_write = std::min(left, write_size);
        if (will_write != fread(write_block, 1, will_write, src)) {
            fclose(src);
            SPIFFS_close(&s_fs, dst);
            return 1;
        }
        int res = SPIFFS_write(&s_fs, dst, write_block, write_size);
        if (res < 0) {
            fclose(src);
            SPIFFS_close(&s_fs, dst);
            return 1;
        }
        left -= will_write;
    }
    
    SPIFFS_close(&s_fs, dst);
    fclose(src);
    return 0;
}

int addFiles(const char* dirname)
{
    DIR *dir;
    struct dirent *ent;
    bool error = false;
    if ((dir = opendir (dirname)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_name[0] == '.')
                continue;
            std::string fullpath = dirname;
            fullpath += '/';
            fullpath += ent->d_name;
            struct stat path_stat;
            stat (fullpath.c_str(), &path_stat);
            if (!S_ISREG(path_stat.st_mode)) {
                std::cerr << "skipping " << ent->d_name << std::endl;
                continue;
            }
            
            std::string filepath = "/";
            filepath += ent->d_name;
            std::cout << ent->d_name << std::endl;
            if (addFile((char*)filepath.c_str(), fullpath.c_str()) != 0) {
                std::cerr << "error adding file!" << std::endl;
                error = true;
                break;
            }
        }
        closedir (dir);
    } else {
        std::cerr << "warning: can't read source directory" << std::endl;
        return 1;
    }
    return (error) ? 1 : 0;
}

void listFiles() {
    spiffs_DIR dir;
    spiffs_dirent ent;
    
    SPIFFS_opendir(&s_fs, 0, &dir);
    spiffs_dirent* it;
    while (true) {
        it = SPIFFS_readdir(&dir, &ent);
        if (!it)
            break;
        
        std::cout << it->size << '\t' << it->name << std::endl;
    }
    SPIFFS_closedir(&dir);
}



int actionPack() {
    s_flashmem.resize(s_imageSize, 0xff);
    
    FILE* fdres = fopen(s_imageName.c_str(), "wb");
    if (!fdres) {
        std::cerr << "error: failed to open image file" << std::endl;
        return 1;
    }
    
    spiffsMount(true);
    int result = addFiles(s_dirName.c_str());
    spiffsUnmount();
    
    fwrite(&s_flashmem[0], 4, s_flashmem.size()/4, fdres);
    fclose(fdres);
    
    return result;
}


int actionList() {
    s_flashmem.resize(s_imageSize, 0xff);
    
    FILE* fdsrc = fopen(s_imageName.c_str(), "rb");
    if (!fdsrc) {
        std::cerr << "error: failed to open image file" << std::endl;
        return 1;
    }
    
    fread(&s_flashmem[0], 4, s_flashmem.size()/4, fdsrc);
    fclose(fdsrc);
    
    spiffsMount(false);
    listFiles();
    spiffsUnmount();
    
    return 0;
}

int actionVisualize() {
    s_flashmem.resize(s_imageSize, 0xff);
    
    FILE* fdsrc = fopen(s_imageName.c_str(), "rb");
    if (!fdsrc) {
        std::cerr << "error: failed to open image file" << std::endl;
        return 1;
    }
    
    fread(&s_flashmem[0], 4, s_flashmem.size()/4, fdsrc);
    fclose(fdsrc);
    
    spiffsMount(false);
    SPIFFS_vis(&s_fs);
    uint32_t total, used;
    SPIFFS_info(&s_fs, &total, &used);
    std::cout << "total: " << total <<  std::endl << "used: " << used << std::endl;
    spiffsUnmount();
    
    return 0;
}

void processArgs(int argc, const char** argv) {
    
    TCLAP::CmdLine cmd("", ' ', VERSION);
    TCLAP::ValueArg<std::string> packArg( "c", "create", "create spiffs image from a directory", true, "", "pack_dir");
    TCLAP::SwitchArg listArg( "l", "list", "list files in spiffs image", false);
    TCLAP::SwitchArg visualizeArg( "i", "visualize", "visualize spiffs image", false);
    TCLAP::UnlabeledValueArg<std::string> outNameArg( "image_file", "spiffs image file", true, "", "image_file"  );
    TCLAP::ValueArg<int> imageSizeArg( "s", "size", "fs image size, in bytes", false, 0x10000, "number" );
    TCLAP::ValueArg<int> pageSizeArg( "p", "page", "fs page size, in bytes", false, 256, "number" );
    TCLAP::ValueArg<int> blockSizeArg( "b", "block", "fs block size, in bytes", false, 4096, "number" );

    cmd.add( imageSizeArg );
    cmd.add( pageSizeArg );
    cmd.add( blockSizeArg );
    std::vector<TCLAP::Arg*> args = {&packArg, &listArg, &visualizeArg};
    cmd.xorAdd( args );
    cmd.add( outNameArg );
    cmd.parse( argc, argv );
    
    if (packArg.isSet()) {
        s_dirName = packArg.getValue();
        s_action = ACTION_PACK;
    }
    else if (listArg.isSet()) {
        s_action = ACTION_LIST;
    }
    else if (visualizeArg.isSet()) {
        s_action = ACTION_VISUALIZE;
    }
    
    s_imageName = outNameArg.getValue();
    s_imageSize = imageSizeArg.getValue();
    s_pageSize  = pageSizeArg.getValue();
    s_blockSize = blockSizeArg.getValue();
}

int main(int argc, const char * argv[]) {
    
    try {
        processArgs(argc, argv);
    }
    catch(...) {
        std::cerr << "Invalid arguments" << std::endl;
        return 1;
    }
    
    
    switch (s_action) {
        case ACTION_PACK: return actionPack();
        case ACTION_LIST: return actionList();
        case ACTION_VISUALIZE: return actionVisualize();
        default: ;
    }
    
    return 1;
}
