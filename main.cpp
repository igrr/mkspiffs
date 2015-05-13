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

#define LOG_PAGE_SIZE       256

static std::vector<uint8_t> s_flashmem;
static std::string s_dirName;
static std::string s_imageName;
static int s_imageSize;

enum Action { ACTION_NONE, ACTION_PACK, ACTION_LIST };
static Action s_action = ACTION_NONE;

static spiffs _filesystemStorageHandle;
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache[(LOG_PAGE_SIZE+32)*4];


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

spiffs_config spiffs_get_storage_config()
{
    spiffs_config cfg = {0};
    
    cfg.phys_addr = 0x0000;
    cfg.phys_size = (u32_t) s_flashmem.size();
    
    const int flash_sector_size = 4096;
    const int log_page_size = 256;
    
    cfg.phys_erase_block = flash_sector_size; // according to datasheet
    cfg.log_block_size = flash_sector_size * 2; // Important to make large
    cfg.log_page_size = log_page_size; // as we said
    return cfg;
}

void check_callback(spiffs_check_type type, spiffs_check_report report,
                                   u32_t arg1, u32_t arg2)
{
}

void spiffs_mount(bool init)
{
    spiffs_config cfg = spiffs_get_storage_config();
    
//    debugf("fs.start:%x, size:%d Kb\n", cfg.phys_addr, cfg.phys_size / 1024);
    
    cfg.hal_read_f = api_spiffs_read;
    cfg.hal_write_f = api_spiffs_write;
    cfg.hal_erase_f = api_spiffs_erase;
    
    int res = SPIFFS_mount(&_filesystemStorageHandle,
                           &cfg,
                           spiffs_work_buf,
                           spiffs_fds,
                           sizeof(spiffs_fds),
                           spiffs_cache,
                           sizeof(spiffs_cache),
                           NULL);
    
//    debugf("mount res: %d\n", res);
    if (init) {
        spiffs_file fd = SPIFFS_open(&_filesystemStorageHandle, "tmp", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
        uint8_t tmp = 0xff;
        SPIFFS_write(&_filesystemStorageHandle, fd, &tmp, 1);
        SPIFFS_fremove(&_filesystemStorageHandle, fd);
        SPIFFS_close(&_filesystemStorageHandle, fd);
    } else {
        _filesystemStorageHandle.check_cb_f = &check_callback;
        //SPIFFS_check(&_filesystemStorageHandle);
    }
}

void spiffs_unmount()
{
    uint32_t total, used;
    SPIFFS_info(&_filesystemStorageHandle, &total, &used);
    SPIFFS_vis(&_filesystemStorageHandle);
    std::cerr << "total: " << total << ", used: " << used << std::endl;
    SPIFFS_unmount(&_filesystemStorageHandle);
}

int add_file(char* name, const char* path) {
    const size_t write_size = 512;
    uint8_t write_block[write_size];
    
    FILE* src = fopen(path, "rb");
    if (!src) {
        std::cerr << "error: failed to open " << path << " for reading" << std::endl;
        return 1;
    }
    
    spiffs_file dst = SPIFFS_open(&_filesystemStorageHandle, name, SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
    
    fseek(src, 0, SEEK_END);
    size_t size = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    size_t left = size;
    while (left > 0)
    {
        size_t will_write = std::min(left, write_size);
        if (will_write != fread(write_block, 1, will_write, src)) {
            std::cerr << "error: failed to read from file";
            fclose(src);
            SPIFFS_close(&_filesystemStorageHandle, dst);
            return 1;
        }
        SPIFFS_write(&_filesystemStorageHandle, dst, write_block, write_size);
        left -= will_write;
    }
    
    SPIFFS_close(&_filesystemStorageHandle, dst);
    fclose(src);
    return 0;
}

int add_files(const char* dirname)
{
    DIR *dir;
    struct dirent *ent;
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
            
            std::cerr << "adding " << ent->d_name << std::endl;
            if (add_file(ent->d_name, fullpath.c_str()) != 0) {
                break;
            }
        }
        closedir (dir);
    } else {
        std::cerr << "warning: can't read source directory" << std::endl;
        return 1;
    }
    return 0;
}

void listFiles() {
    spiffs_DIR dir;
    spiffs_dirent ent;
    
    spiffs_DIR* res = SPIFFS_opendir(&_filesystemStorageHandle, 0, &dir);
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
    
    spiffs_mount(true);
    add_files(s_dirName.c_str());
    spiffs_unmount();
    
    
    fwrite(&s_flashmem[0], 4, s_flashmem.size()/4, fdres);
    
    fclose(fdres);
    
    return 0;
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
    
    spiffs_mount(false);
    listFiles();
    spiffs_unmount();
    
    return 0;
}

int processArgs(int argc, const char** argv) {
    
    TCLAP::CmdLine cmd("", ' ', "0.1");
    TCLAP::ValueArg<std::string> packArg( "c", "create", "create spiffs image from a directory", true, "", "pack_dir");
    TCLAP::SwitchArg listArg( "l", "list", "list spiffs image to a directory", false);
    TCLAP::UnlabeledValueArg<std::string> outNameArg( "image_file", "spiffs image file", true, "", "image_file"  );
    TCLAP::ValueArg<int> sizeArg( "s", "fs_size", "fs image size, in bytes", false, 0xb000, "number" );

    cmd.add( sizeArg );
    cmd.xorAdd( packArg, listArg );
    cmd.add( outNameArg );
    cmd.parse( argc, argv );
    
    if (packArg.isSet()) {
        s_dirName = packArg.getValue();
        s_action = ACTION_PACK;
    }
    else if (listArg.isSet()) {
        s_action = ACTION_LIST;
    }
    
    s_imageName = outNameArg.getValue();
    s_imageSize = sizeArg.getValue();
    
    return 0;
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
        default: ;
    }
    
    return 1;
}
