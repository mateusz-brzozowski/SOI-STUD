#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <fstream>


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
    // char name[NAME_LENGTH];
    std::string name;
    FILE* file;
    INode *inodes;
    bool *data_maps;
    DataBlock *data_blocks;
    SuperBlock super_block;
    u_int64_t inodes_length;
    u_int64_t data_maps_length;
    u_int64_t data_blocks_length;
    std::vector<DirectoryLink*> shown_direcotry_links;
    std::vector<INode*> shown_inodes;


public:
    void create(std::string file_name, u_int64_t disc_size){
        name = file_name;
        std::ofstream file(file_name, std::ios::out | std::ios::binary);
        if(!file){
            std::cout << "Cannot open file";
            exit(EXIT_FAILURE);
        }

        u_int64_t number_of_inodes = disc_size / sizeof(INode) / FILES_SPACE;
        u_int64_t max_number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode)) / sizeof(DataBlock);
        u_int64_t number_of_data_blocks = (disc_size - sizeof(SuperBlock) - number_of_inodes * sizeof(INode) - max_number_of_data_blocks * sizeof(bool)) / sizeof(DataBlock);
        u_int64_t data_map_offset = sizeof(SuperBlock) + number_of_inodes * sizeof(INode);
        u_int64_t data_block_offset = data_map_offset + number_of_data_blocks * sizeof(bool);

        strncpy((char*)super_block.name, name.c_str(), NAME_LENGTH);
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

        file.write((char*)&super_block, sizeof(SuperBlock));
        file.write((char*)&root, sizeof(INode));

        for(int i = 0; i < inodes_length - 1; i++){
            INode inode{};
            inode.data_block_index = -1;
            file.write((char*)&inode, sizeof(INode));
        }

        for(int i = 0; i < data_maps_length; i++){
            unsigned int idx;
            file.write((char*)&idx, sizeof(bool));
        }

        for(int i = 0; i < data_blocks_length; i++){
            DataBlock data_block{};
            data_block.offset = -1;
            file.write((char*)&data_block, sizeof(DataBlock));
        }

        file.close();
        if(!file.good()){
            std::cout << "Writing to file error";
            exit(EXIT_FAILURE);
        }
    }

    void open(){
        std::ifstream file(name, std::ios::out | std::ios::binary);
        // file = fopen(name, "rb+");
        file.read((char*)&super_block, sizeof(SuperBlock));
        // fread(&super_block, sizeof(SuperBlock), 1 ,file);
        load_lengths(super_block);
        inodes = new INode[inodes_length];
        file.read((char*)inodes, inodes_length);
        // fread(inodes, sizeof(INode), inodes_length, file);
        data_maps = new bool[data_maps_length];
        file.read((char*)data_maps, data_maps_length);
        // fread(data_maps, sizeof(bool), data_maps_length, file);
        data_blocks = new DataBlock[data_blocks_length];
        file.read((char*)data_blocks, data_blocks_length);
        // fread(data_blocks, sizeof(DataBlock), data_blocks_length, file);
        file.close();
        if(!file.good()){
            std::cout << "Reagin from file error";
            exit(EXIT_FAILURE);
        }
    }

    void close(){
        std::ofstream file(name, std::ios::out | std::ios::binary);
        if(!file){
            std::cout << "Cannot open file";
            exit(EXIT_FAILURE);
        }
        // fseek(file, 0, 0);
        file.write((char*)&super_block, sizeof(SuperBlock));
        file.write((char*)inodes, sizeof(INode));
        file.write((char*)data_maps, sizeof(bool));
        file.write((char*)data_blocks, sizeof(DataBlock));

        // fwrite(&super_block, sizeof(SuperBlock), 1 ,file);
        // fwrite(inodes, sizeof(INode), inodes_length, file);
        // fwrite(data_maps, sizeof(bool), data_maps_length, file);
        // fwrite(data_blocks, sizeof(DataBlock), data_blocks_length, file);
        file.close();
        if(!file.good()){
            std::cout << "Writing to file error";
            exit(EXIT_FAILURE);
        }
    }

    void set_name(char *file_name){
        // strncpy(name, file_name, NAME_LENGTH);
    }

    void create_directory(char *path){
        INode *current_direcotry_inode = inodes;
        char *current_directory_name = strtok(path, "/");
        for(current_directory_name; current_directory_name != NULL; current_directory_name = strtok(NULL, "/")){
            if(!is_valid_name(current_directory_name)){
                std::cerr << "Invalid directory name: " << current_directory_name << "\n";
                exit(EXIT_FAILURE);
            }
            INode* next_direcotry_inode = get_inode_in_inode(current_direcotry_inode, current_directory_name);
            if(next_direcotry_inode && next_direcotry_inode->type == INodeType::DIRECTORY_NODE){
                current_direcotry_inode = next_direcotry_inode;
            } else if (next_direcotry_inode){
                std::cerr << "Missing directory: " << next_direcotry_inode << "\n";
                return;
            } else{
                u_int64_t new_inode_idx = get_empty_inode();
                // TODO: CHECK IF EXISTS
                inodes[new_inode_idx].reference_count = 1;
                inodes[new_inode_idx].type = INodeType::DIRECTORY_NODE;

                DirectoryLink new_directory_link{0};
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
                    add_link_to_inode(current_direcotry_inode, new_directory_link);
                }
                current_direcotry_inode = &inodes[new_inode_idx];
            }
        }
    }

    void show_files_tree(char *path){
        shown_direcotry_links.clear();
        INode *directory = get_direcotry_inode(path);
        return show_files_inode(directory, 0);
    }

    void file_to_disc(char *path, char *file_name){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        if(get_inode_in_inode(direcotry_inode, file_name)){
            std::cerr << "FIle alraedy exists";
            exit(EXIT_FAILURE);
        }

        u_int64_t new_inode_idx = get_empty_inode();
        // TODO: CHECK IF EXISTS
        inodes[new_inode_idx].reference_count = 1;
        inodes[new_inode_idx].type = INodeType::FILE_NODE;
        inodes[new_inode_idx].size = 0;

        DirectoryLink new_file_link{0};
        new_file_link.used = true;
        new_file_link.inode_id = new_inode_idx;
        strncpy((char *)new_file_link.name, file_name, NAME_LENGTH);

        add_link_to_inode(direcotry_inode, new_file_link);

        FILE *file = fopen(file_name, "rb+");

        inodes[new_inode_idx].data_block_index = get_empty_data_block();
        u_int64_t current_data_block_idx = inodes[new_inode_idx].data_block_index;
        while(current_data_block_idx != -1){
            unsigned left_size_in_block = 0;
            unsigned read_size = 0;
            while(left_size_in_block < DATA_BLOCK_SIZE){
                read_size = fread(data_blocks[current_data_block_idx].data, 1, DATA_BLOCK_SIZE - left_size_in_block, file);
                if(!read_size && feof(file)){
                    break;
                }
                else if (!read_size){
                    std::cerr << "Invalid file";
                    exit(EXIT_FAILURE);
                }
                left_size_in_block += read_size;
                inodes[new_inode_idx].size += read_size;
            }
            if(!left_size_in_block){
                remove_data_block_from_inode(&inodes[new_inode_idx], current_data_block_idx);
                current_data_block_idx = -1;
            } else if (feof(file)){
                data_maps[current_data_block_idx] = true;
                data_blocks[current_data_block_idx].offset = -1;
                current_data_block_idx = -1;
            } else {
                data_maps[current_data_block_idx] = true;
                data_blocks[current_data_block_idx].offset = get_empty_data_block();
                current_data_block_idx = data_blocks[current_data_block_idx].offset;
            }
        }
    }

    void file_from_disc(char *path, char *file_name, char *file_name_destination){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        INode *file= get_inode_in_inode(direcotry_inode, file_name);
        if(!file){
            std::cerr << "Missing file";
            exit(EXIT_FAILURE);
        }
        FILE *file_destination = fopen(file_name_destination, "wb+");
        u_int64_t current_data_block_idx = file->data_block_index;
        u_int64_t left_size = file->size;
        while(current_data_block_idx != -1){
            u_int8_t *data = data_blocks[current_data_block_idx].data;
            fwrite(data, (left_size < DATA_BLOCK_SIZE ? left_size : DATA_BLOCK_SIZE), 1, file_destination);
            left_size -= (left_size < DATA_BLOCK_SIZE ? left_size : DATA_BLOCK_SIZE);
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
    }

    u_int64_t get_left_space(){
        u_int64_t left_space = 0;
        for(int i = 0; i < data_blocks_length; i++)
            if(data_maps[i] == 0)
                left_space += DATA_BLOCK_SIZE;
        return left_space;
    }

    u_int64_t get_size(char *path){
        INode *direcotry = get_direcotry_inode(path);
        u_int64_t size = 0;
        u_int64_t current_data_block_idx = direcotry->data_block_index;
        while(current_data_block_idx != -1){
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
            for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                if(direcotry_links[idx].used)
                   size += inodes[direcotry_links[idx].inode_id].size;
            }
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
        return size;
    }

    u_int64_t get_full_size(char *path){
        shown_inodes.clear();
        INode *directory = get_direcotry_inode(path);
        return get_size_inode(directory);
    }

    void create_link(char *path, char* file_name, char *link_path, char *link_file_name){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        INode *file= get_inode_in_inode(direcotry_inode, file_name);
        if(!file){
            std::cerr << "Missing file";
            exit(EXIT_FAILURE);
        }
        INode *link_directory_inode = get_direcotry_inode(link_path);
        if(!link_directory_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }

        file->reference_count += 1;

        DirectoryLink new_file_link{};
        new_file_link.used = true;
        new_file_link.inode_id = file - inodes;
        strncpy((char *)new_file_link.name, link_file_name, NAME_LENGTH);

        add_link_to_inode(link_directory_inode, new_file_link);
    }

    void show_information(char *path){
        // std:: cout <<
    }

    void remove_link(char *path, char* file_name){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        INode *file= get_inode_in_inode(direcotry_inode, file_name);
        if(!file){
            std::cerr << "Missing file";
            exit(EXIT_FAILURE);
        }
        remove_inode(file);
        DirectoryLink *direcotry_link = get_direcotry_in_inode(direcotry_inode, file_name);
        direcotry_link->inode_id = -1;
        direcotry_link->used = false;
    }

    void cut_file(char *path, char* file_name, size_t size_to_cut){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        INode *file= get_inode_in_inode(direcotry_inode, file_name);
        if(!file){
            std::cerr << "Missing file";
            exit(EXIT_FAILURE);
        }

        if(file->size < size_to_cut){
            std::cerr << "Size to cut greater than file's size";
            exit(EXIT_FAILURE);
        }

        file->size -= size_to_cut;
        u_int64_t start_cut_block = file->size / DATA_BLOCK_SIZE;
        if(file->size % DATA_BLOCK_SIZE != 0)
            start_cut_block++;
        u_int64_t data_block_idx = file->data_block_index;
        u_int64_t prev_data_block_idx = -1;
        for(u_int64_t idx = 0; idx < start_cut_block; idx++){
            prev_data_block_idx = data_block_idx;
            data_block_idx = data_blocks[idx].offset;
        }
        clear_datablocks(data_block_idx);
        if(prev_data_block_idx == -1)
            file->data_block_index = -1;
        else
            data_blocks[prev_data_block_idx].offset = -1;
    }

    void extend_file(char *path, char* file_name, size_t size_to_extend){
        INode *direcotry_inode = get_direcotry_inode(path);
        if(!direcotry_inode){
            std::cerr << "Invalid path";
            exit(EXIT_FAILURE);
        }
        INode *file= get_inode_in_inode(direcotry_inode, file_name);
        if(!file){
            std::cerr << "Missing file";
            exit(EXIT_FAILURE);
        }

        u_int64_t last_datablock_idx = file->data_block_index;
        while(data_blocks[last_datablock_idx].offset != -1)
            last_datablock_idx = data_blocks[last_datablock_idx].offset;

        u_int64_t last_datablock_size = file->size % DATA_BLOCK_SIZE;
        if(last_datablock_size > 0){
            u_int64_t current_size_to_extend = DATA_BLOCK_SIZE - (file->size % DATA_BLOCK_SIZE);
            if(current_size_to_extend > size_to_extend)
                current_size_to_extend = size_to_extend;
            memset(data_blocks[last_datablock_idx].data + last_datablock_size * sizeof(u_int8_t), 0, current_size_to_extend);
            size_to_extend -= current_size_to_extend;
            file->size += current_size_to_extend;
        }
        while(size_to_extend > 0){
            data_blocks[last_datablock_idx].offset = get_empty_data_block();
            if(data_blocks[last_datablock_idx].offset == -1){
                std::cerr << "Invalid path";
                exit(EXIT_FAILURE);
            }
            data_maps[data_blocks[last_datablock_idx].offset] = true;
            u_int64_t new_size;
            if(size_to_extend > DATA_BLOCK_SIZE)
                new_size = DATA_BLOCK_SIZE;
            else
                new_size = size_to_extend;
            memset(data_blocks[data_blocks[last_datablock_idx].offset].data, 0, new_size);
            size_to_extend -= new_size;
            file->size += new_size;
            last_datablock_idx = data_blocks[last_datablock_idx].offset;
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
        for(u_int32_t i = 0; i < data_blocks_length; i++)
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
                DirectoryLink directory_link = direcotry_links[idx];
                if((directory_link.used) && strcmp((char*)directory_link.name, name) == 0)
                    return &direcotry_links[idx];
                // if(direcotry_links[idx].used && strcmp((char*)direcotry_links[idx].name, name) == 0)
                //     return &direcotry_links[idx];
            }
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
        return NULL;
    }

    void load_lengths(SuperBlock super_block_){
        inodes_length = super_block_.inodes_count;
        data_maps_length = super_block_.unused_datablocks;
        data_blocks_length = super_block_.unused_datablocks;
    }

    void add_link_to_inode(INode* inode, DirectoryLink directory_link){
        if(inode->data_block_index == -1)
            inode->data_block_index = get_empty_data_block();
        u_int64_t current_data_block_idx = inode->data_block_index;
        bool is_place_for_link = false;
        while(!is_place_for_link){
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
            for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                if(!direcotry_links[idx].used){
                    direcotry_links[idx] = directory_link;
                    is_place_for_link = true;
                    break;
                }
            }
            if(is_place_for_link)
                break;
            if(data_blocks[current_data_block_idx].offset){
                current_data_block_idx = data_blocks[current_data_block_idx].offset;
            } else{
                u_int64_t new_data_block_idx = get_empty_data_block();
                data_blocks[current_data_block_idx].offset = new_data_block_idx;
                data_maps[current_data_block_idx] = true;
            }
        }
    }

    void remove_data_block_from_inode(INode* inode, u_int64_t data_block_idx){
        u_int64_t current_data_block_idx = inode->data_block_index;
        // TODO: REMOVE THIS IF
        if(current_data_block_idx == data_block_idx){
            inode->data_block_index = -1;
            return;
        }
        while(current_data_block_idx != -1){
            if(current_data_block_idx == data_block_idx){
                data_blocks[current_data_block_idx].offset = -1;
                return;
            }
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
    }

    INode *get_direcotry_inode(char *path){
        INode *current_direcotry_inode = inodes;
        char *current_directory_name = strtok(path, "/");
        for(current_directory_name; current_directory_name != NULL; current_directory_name = strtok(NULL, "/")){
            if(!is_valid_name(current_directory_name)){
                std::cerr << "Invalid directory name: " << current_directory_name << "\n";
                exit(EXIT_FAILURE);
            }
            INode* next_direcotry_inode = get_inode_in_inode(current_direcotry_inode, current_directory_name);
            if(next_direcotry_inode && next_direcotry_inode->type == INodeType::DIRECTORY_NODE){
                current_direcotry_inode = next_direcotry_inode;
            } else{
                std::cerr << "Missing directory: " << next_direcotry_inode << "\n";
                return NULL;
            }
        }
        return current_direcotry_inode;
    }

    u_int64_t get_size_inode(INode *direcotry){
        u_int64_t size = 0;
        u_int64_t current_data_block_idx = direcotry->data_block_index;
        while(current_data_block_idx != -1){
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
            for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                if(direcotry_links[idx].used){
                    if(std::find(shown_inodes.begin(),shown_inodes.end(), &inodes[direcotry_links[idx].inode_id]) != shown_inodes.end())
                        continue;
                    shown_inodes.push_back(&inodes[direcotry_links[idx].inode_id]);
                    if(inodes[direcotry_links[idx].inode_id].type == INodeType::FILE_NODE)
                        size += inodes[direcotry_links[idx].inode_id].size;
                    else
                        size += get_size_inode(&inodes[direcotry_links[idx].inode_id]);
                }
            }
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
        return size;
    }

    void show_files_inode(INode* directory_inode, int rec_lvl){
        if(directory_inode->type != INodeType::DIRECTORY_NODE)
            return;
        u_int64_t current_data_block_idx = directory_inode->data_block_index;
        while(current_data_block_idx != -1){
            std::cout << "\n";
            for(int i = 0; i < rec_lvl; i++)
                std::cout << "  ";
            DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
            for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                if(!direcotry_links[idx].used)
                    continue;
                if(std::find(shown_direcotry_links.begin(),shown_direcotry_links.end(), &direcotry_links[idx]) != shown_direcotry_links.end())
                    continue;
                shown_direcotry_links.push_back(&direcotry_links[idx]);
                if (inodes[direcotry_links[idx].inode_id].type == INodeType::DIRECTORY_NODE)
                    std:: cout << "\x1B[34m" << direcotry_links[idx].name << "\033[0m ";
                else
                    std::cout << direcotry_links[idx].name << " ";
                show_files_inode(&inodes[direcotry_links[idx].inode_id], rec_lvl + 1);
            }
            std::cout << "\n";
            for(int i = 0; i < rec_lvl - 1; i++)
                std::cout << "  ";
            current_data_block_idx = data_blocks[current_data_block_idx].offset;
        }
    }

    void remove_inode(INode* inode){
        inode->reference_count -= 1;
        if(inode->reference_count != 0)
            return;
        if(inode->type == INodeType::DIRECTORY_NODE){
            u_int64_t current_data_block_idx = inode->data_block_index;
            while(current_data_block_idx != -1){
                DirectoryLink* direcotry_links = (DirectoryLink*)data_blocks[current_data_block_idx].data;
                for(int idx = 0; idx < DIRECTORY_LINKS_IN_DATA_BLOCK; idx++){
                    if(direcotry_links[idx].used){
                        remove_inode(&inodes[direcotry_links[idx].inode_id]);
                    }
                }
                current_data_block_idx = data_blocks[current_data_block_idx].offset;
            }
        }
        clear_datablocks(inode->data_block_index);
        inode->data_block_index = -1;
        inode->type = INodeType::UNUSED_NODE;
        inode->type = 0;
        inode->reference_count = 0;
    }

    void clear_datablocks(u_int64_t data_block_idx){
        std::vector<u_int64_t> data_blocks_idxs;
        u_int64_t idx = data_block_idx;
        while(idx != -1){
            data_blocks_idxs.push_back(idx);
            idx = data_blocks[idx].offset;
        }
        for(auto idx:data_blocks_idxs){
            data_blocks[idx].offset = -1;
            data_maps[idx] = false;
        }
    }

    std::vector<std::string> split_pwd(std::string pwd){
        std::vector<std::string> files;
        size_t counter = 0;
        while((counter = pwd.find("/")) != std::string::npos){
            files.push_back(pwd.substr(0, counter));
            pwd.erase(0, counter + 1);
        }
        files.push_back(pwd);
        return files;
    }
};

#pragma region user_interface

void help(int argc, char* argv[]){

}
void mkdir(int argc, char* argv[]){

}
void tree(int argc, char* argv[]){

}
void remove_file(int argc, char* argv[]){

}
void get_file(int argc, char* argv[]){

}
void link(int argc, char* argv[]){

}
void unlink(int argc, char* argv[]){

}
void send_file(int argc, char* argv[]){

}
void information(int argc, char* argv[]){

}
void cut_file(int argc, char* argv[]){

}
void extend_file(int argc, char* argv[]){

}
void create(int argc, char* argv[]){

}
#pragma endregion


int main(int argc, char* argv[]) {
    VirtualDisc virtual_disc;
    virtual_disc.create("test", 1024*1024);
    // virtual_disc.open();
    // virtual_disc.close();
    // char direcotries[80];
    // char file[80];
    // char file_destinaiton[80];
    // char link_name[80];
    // strcpy(direcotries, "a/x");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/y");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/z");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/x/k");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/x/l");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/x/m");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/z/q");
    // virtual_disc.create_directory(direcotries);
    // strcpy(direcotries, "a/z/w");
    // virtual_disc.create_directory(direcotries);

    // strcpy(direcotries, "a/x");
    // strcpy(file, "matejko");
    // virtual_disc.file_to_disc(direcotries, file);

    // strcpy(direcotries, "a/x");
    // strcpy(file, "matejko");
    // strcpy(file_destinaiton, "matejko_out");
    // virtual_disc.file_from_disc(direcotries, file, file_destinaiton);
    // strcpy(direcotries, "");
    // virtual_disc.show_files_tree(direcotries);


    // std::unordered_map<std::string, std::function<void(int, char**)>> functions {
    //     {"help", help}, {"mkdir", mkdir}, {"tree", tree}, {"rm", remove_file},
    //     {"ln", link}, {"uln", unlink}, {"send", send_file}, {"get", get_file},
    //     {"ls", information}, {"cut", cut_file}, {"extend", extend_file}, {"create", create}
    // };
    // std::string function = std::string(argv[2]);
    // for (auto f : functions){
    //     if(function == f.first)
    //         f.second(argc, argv);
    // }
    // help(argc, argv);
    return 0;
}