#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define DATA_BLOCK_SIZE 8192
#define NAME_LENGTH 16
#define FILES_SPACE 8

enum INodeType{
    UNUSED_NODE,
    FILE_NODE,
    DIRECTORY_NODE
};

struct INode{
    u_int64_t size;
    u_int64_t data_block_offset;

    u_int8_t type;
    u_int8_t reference_count;
};

struct DataBlock{
    u_int64_t offset;

    u_int8_t data[DATA_BLOCK_SIZE];
};

struct DirectoryLink{
    u_int16_t inode_id;

    u_int8_t used;
    u_int8_t name[NAME_LENGTH];
};

struct SuperBlock{
    u_int64_t disc_size;
    u_int64_t inode_offset;
    u_int64_t data_map_offset;
    u_int64_t data_block_offset;

    u_int32_t unused_inodes;
    u_int32_t inodes_count;
    u_int32_t unused_datablocks;
    u_int32_t datablocks_count;

    u_int8_t name[NAME_LENGTH];
};

class VirtualDisc{
public:
    char name[NAME_LENGTH];
    FILE* file;
    INode *inodes;
    bool *data_maps;
    DataBlock *datablocks;
    SuperBlock super_block;
    u_int64_t inodes_length;
    u_int64_t data_maps_length;
    u_int64_t datablocks_length;


public:
    VirtualDisc(char *file_name, u_int64_t disc_size){
        strncpy(name, file_name, NAME_LENGTH);
        file = fopen(name, "wb+");

        u_int64_t number_of_inodes = disc_size / sizeof(INode) / FILES_SPACE;
        u_int64_t max_number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode)) / sizeof(DataBlock);
        u_int64_t number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode) - max_number_of_data_blocks * sizeof(bool)) / sizeof(DataBlock);
        u_int64_t data_map_offset = sizeof(SuperBlock) + number_of_inodes * sizeof(INode);
        u_int64_t data_block_offset = data_map_offset + number_of_data_blocks * sizeof(bool);

        inodes_length = number_of_inodes;
        data_maps_length = number_of_data_blocks;
        datablocks_length = number_of_data_blocks;

        strncpy((char*)super_block.name, name, NAME_LENGTH);
        super_block.disc_size = disc_size;
        super_block.inodes_count = number_of_inodes;
        super_block.unused_inodes = number_of_inodes;
        super_block.datablocks_count = number_of_data_blocks;
        super_block.unused_datablocks = number_of_data_blocks;
        super_block.inode_offset = sizeof(SuperBlock);
        super_block.data_map_offset = data_map_offset;
        super_block.data_block_offset = data_block_offset;

        INode root;
        root.type = INodeType::DIRECTORY_NODE;
        root.reference_count = 1;
        super_block.unused_inodes -= 1;

        fwrite(&super_block, sizeof(SuperBlock), 1, file);
        fwrite(&root, sizeof(INode), 1, file);

        for(int i = 0; i < inodes_length - 1; i++){
            INode inode;
            fwrite(&inode, sizeof(INode), 1, file);
        }

        for(int i = 0; i < data_maps_length; i++){
            unsigned int idx;
            fwrite(&idx, sizeof(bool), 1, file);
        }

        for(int i = 0; i < datablocks_length; i++){
            DataBlock data_block;
            fwrite(&data_block, sizeof(DataBlock), 1, file);
        }

        fclose(file);
    }

    void open(){
        file = fopen(name, "wb+");
        fread(&super_block, sizeof(SuperBlock), 1 ,file);
        inodes = new INode[inodes_length];
        fread(inodes, sizeof(INode), inodes_length, file);
        data_maps = new bool[data_maps_length];
        fread(data_maps, sizeof(bool), data_maps_length, file);
        datablocks = new DataBlock[datablocks_length];
        fread(datablocks, sizeof(DataBlock), datablocks_length, file);
    }

    void close(){
        fwrite(&super_block, sizeof(SuperBlock), 1 ,file);
        fwrite(inodes, sizeof(INode), inodes_length, file);
        fwrite(data_maps, sizeof(bool), data_maps_length, file);
        fwrite(datablocks, sizeof(DataBlock), datablocks_length, file);
        fclose(file);
    }
};

int main(int argc, char* argv[]) {
    VirtualDisc virtual_disc(argv[1], 1024*1024);
    virtual_disc.open();
    std::cout << virtual_disc.super_block.disc_size;
    virtual_disc.close();
    return 0;
}