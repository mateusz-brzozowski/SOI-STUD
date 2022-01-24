#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define DATA_BLOCK_SIZE 8192
#define NAME_LENGTH 16
#define FILES_SPACE 8
#define DIRECTORY_LINKS_IN_DATA_BLOCK (DATA_BLOCK_SIZE / sizeof(DirectoryLink))

#pragma region structures

enum INodeType{
    UNUSED_NODE,
    FILE_NODE,
    DIRECTORY_NODE
};

struct INode{
    u_int64_t size;
    u_int64_t data_block_index;

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

#pragma endregion


class VirtualDisc{
public:
    char name[NAME_LENGTH];
    FILE* file;
    INode *inodes;
    bool *data_maps;
    DataBlock *data_blocks;
    SuperBlock super_block;
    u_int64_t inodes_length;
    u_int64_t data_maps_length;
    u_int64_t datablocks_length;


public:
    void create(char *file_name, u_int64_t disc_size){
        strncpy(name, file_name, NAME_LENGTH);
        file = fopen(name, "wb+");

        u_int64_t number_of_inodes = disc_size / sizeof(INode) / FILES_SPACE;
        u_int64_t max_number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode)) / sizeof(DataBlock);
        u_int64_t number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode) - max_number_of_data_blocks * sizeof(bool)) / sizeof(DataBlock);
        u_int64_t data_map_offset = sizeof(SuperBlock) + number_of_inodes * sizeof(INode);
        u_int64_t data_block_offset = data_map_offset + number_of_data_blocks * sizeof(bool);

        strncpy((char*)super_block.name, name, NAME_LENGTH);
        super_block.disc_size = disc_size;
        super_block.inodes_count = number_of_inodes;
        super_block.unused_inodes = number_of_inodes;
        super_block.datablocks_count = number_of_data_blocks;
        super_block.unused_datablocks = number_of_data_blocks;
        super_block.inode_offset = sizeof(SuperBlock);
        super_block.data_map_offset = data_map_offset;
        super_block.data_block_offset = data_block_offset;

        load_lengths(super_block);

        INode root{};
        root.type = INodeType::DIRECTORY_NODE;
        root.data_block_index = -1;
        root.reference_count = 1;
        super_block.unused_inodes -= 1;

        fwrite(&super_block, sizeof(SuperBlock), 1, file);
        fwrite(&root, sizeof(INode), 1, file);

        for(int i = 0; i < inodes_length - 1; i++){
            INode inode{};
            inode.data_block_index = -1;
            fwrite(&inode, sizeof(INode), 1, file);
        }

        for(int i = 0; i < data_maps_length; i++){
            unsigned int idx;
            fwrite(&idx, sizeof(bool), 1, file);
        }

        for(int i = 0; i < datablocks_length; i++){
            DataBlock data_block{};
            data_block.offset = -1;
            fwrite(&data_block, sizeof(DataBlock), 1, file);
        }

        fclose(file);
    }

    void open(){
        file = fopen(name, "rb+");
        fread(&super_block, sizeof(SuperBlock), 1 ,file);
        load_lengths(super_block);
        inodes = new INode[inodes_length];
        fread(inodes, sizeof(INode), inodes_length, file);
        data_maps = new bool[data_maps_length];
        fread(data_maps, sizeof(bool), data_maps_length, file);
        data_blocks = new DataBlock[datablocks_length];
        fread(data_blocks, sizeof(DataBlock), datablocks_length, file);
    }

    void close(){
        fseek(file, 0, 0);
        fwrite(&super_block, sizeof(SuperBlock), 1 ,file);
        fwrite(inodes, sizeof(INode), inodes_length, file);
        fwrite(data_maps, sizeof(bool), data_maps_length, file);
        fwrite(data_blocks, sizeof(DataBlock), datablocks_length, file);
        fclose(file);
    }

    void set_name(char *file_name){
        strncpy(name, file_name, NAME_LENGTH);
    }

    void create_directory(char *path){
        INode *current_direcotry_inode = inodes;
        char *current_directory_name = strtok(path, "/");
        for(current_directory_name; current_directory_name != NULL; current_directory_name = strtok(NULL, "/")){
            if(!is_valid_name(current_directory_name)){
                std::cerr << "Invalid directory name: " << current_directory_name << "\n";
                exit(EXIT_FAILURE);
            }

            INode* next = get_inode_in_inode(current_direcotry_inode, current_directory_name);

            if(next && next->type == INodeType::DIRECTORY_NODE){
                current_direcotry_inode = next;
            } else if (next){
                std::cerr << "Missing directory: " << next << "\n";
                return;
            } else{
                u_int64_t new_inode_idx = get_empty_inode();
                // TODO: CHECK IF EXISTS
                inodes[new_inode_idx].reference_count = 1;
                inodes[new_inode_idx].type = INodeType::DIRECTORY_NODE;

                DirectoryLink new_directory_link{};
                new_directory_link.used = true;
                new_directory_link.inode_id = new_inode_idx;
                strncpy((char *)new_directory_link.name, current_directory_name, NAME_LENGTH);

                if(current_direcotry_inode->data_block_index == -1){
                    u_int64_t new_data_block_idx = get_empty_data_block();
                    // TOOD: CHECK IF EXISTS
                    current_direcotry_inode->data_block_index = new_data_block_idx;
                    *(DirectoryLink*)data_blocks[new_data_block_idx].data = new_directory_link;
                    data_maps[new_data_block_idx] = true;
                } else{
                    u_int64_t current_data_block_idx = current_direcotry_inode->data_block_index;
                    u_int64_t last_data_block_idx = current_data_block_idx;
                    bool is_place_for_link = false;
                    while(current_data_block_idx != -1){
                        DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
                        for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                            if(!direcotry_links[idx].used){
                                direcotry_links[idx] = new_directory_link;
                                is_place_for_link = true;
                                break;
                            }
                        }
                        if(data_blocks[current_data_block_idx].offset){
                            last_data_block_idx = current_data_block_idx;
                        }
                        current_data_block_idx = data_blocks[current_data_block_idx].offset;
                    }
                    if(!is_place_for_link){
                        u_int64_t new_data_block_idx = get_empty_data_block();
                        data_blocks[last_data_block_idx].offset = new_data_block_idx;
                        *(DirectoryLink*)data_blocks[new_data_block_idx].data = new_directory_link;
                        data_maps[last_data_block_idx] = true;
                    }
                }
                current_direcotry_inode = &inodes[new_inode_idx];
            }
        }
    }

    void show_files_tree(INode* directory_inode, unsigned prefix){
        for (u_int64_t i = 0; i < inodes_length; i++){
            if(inodes[i].data_block_index == -1)
                break;
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[i].data;
            for(u_int64_t j = 0; j < DIRECTORY_LINKS_IN_DATA_BLOCK; j++){
                if(!direcotry_links[j].used)
                    continue;
                std::cout << j << direcotry_links[j].name << ", ";
            }
        }
    }

private:
    bool is_valid_name(char *name){
        if(strlen(name) >= NAME_LENGTH or !strcmp(name, ".") or !strcmp(name, "..") or strpbrk(name, "/") or name[0] == 0)
            return false;
        return true;
    }

    u_int32_t get_empty_inode(){
        for(u_int32_t i = 0; i < inodes_length; i++)
            if (inodes[i].type == INodeType::UNUSED_NODE)
                return i;
        return -1;
    }

    u_int32_t get_empty_data_block(){
        for(u_int32_t i = 0; i < datablocks_length; i++)
            if (data_maps[i] == false)
                return i;
        return -1;
    }

    INode* get_inode_in_inode(INode *direcotry, char *name){
        DirectoryLink *directory_link = get_direcotry_in_inode(direcotry, name);
        if(!directory_link)
            return NULL;
        return &inodes[directory_link->inode_id];
    }

    DirectoryLink *get_direcotry_in_inode(INode *direcotry, char *name){
        if(direcotry->type != INodeType::DIRECTORY_NODE)
            return NULL;
        u_int64_t current_data_block_idx = direcotry->data_block_index;
        while(current_data_block_idx != -1){
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
            for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                if(direcotry_links[idx].used && strcmp((char*)direcotry_links[idx].name, name) == 0)
                    return &direcotry_links[idx];
            }
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
        return NULL;
    }

    void load_lengths(SuperBlock super_block_){
        inodes_length = super_block_.inodes_count;
        data_maps_length = super_block_.unused_datablocks;
        datablocks_length = super_block_.unused_datablocks;
    }
};

int main(int argc, char* argv[]) {
    VirtualDisc virtual_disc;
    virtual_disc.set_name((char*)"test");
    virtual_disc.create((char*)"test", 1024*1024);
    virtual_disc.open();
    char direcotries[80];
    strcpy(direcotries, "a/b/c");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/b/d");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/b/e");
    virtual_disc.create_directory(direcotries);
    // for(int i = 0; i < 64; i++){
    //     virtual_disc.create_directory("aaa");
    //     virtual_disc.create_directory("bbb");
    //     virtual_disc.create_directory("ccc");
    // }
    // virtual_disc.show_files_tree(&virtual_disc.inodes[0], 0);
    // std::cout << virtual_disc.super_block.disc_size;
    // virtual_disc.create_directory((char *)"dupa");
    virtual_disc.close();
    return 0;
}