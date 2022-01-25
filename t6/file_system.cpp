#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>

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
    u_int64_t data_blocks_length;
    std::vector<DirectoryLink*> shown_direcotry_links;
    std::vector<INode*> shown_inodes;


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

        for(int i = 0; i < data_blocks_length; i++){
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
        data_blocks = new DataBlock[data_blocks_length];
        fread(data_blocks, sizeof(DataBlock), data_blocks_length, file);
    }

    void close(){
        fseek(file, 0, 0);
        fwrite(&super_block, sizeof(SuperBlock), 1 ,file);
        fwrite(inodes, sizeof(INode), inodes_length, file);
        fwrite(data_maps, sizeof(bool), data_maps_length, file);
        fwrite(data_blocks, sizeof(DataBlock), data_blocks_length, file);
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

        file->size -= size_to_cut;
        u_int64_t start_cut_block = file->size / DATA_BLOCK_SIZE;
        if(file->size % DATA_BLOCK_SIZE != 0)
            start_cut_block++;
        u_int64_t data_block_idx = file->data_block_index;
        for(u_int64_t idx = 0; idx < start_cut_block; idx++)
            data_block_idx = data_blocks[idx].offset;
        clear_datablocks(data_block_idx);
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
        // TODO: DUZO ROBOTY TU JESZCZE
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
        if(data_block_idx == -1)
            return;
        u_int64_t next_data_block_idx = data_blocks[data_block_idx].offset;
        if(next_data_block_idx != -1)
            data_blocks_idxs.push_back(next_data_block_idx);
        data_blocks[data_block_idx].offset = -1;
        while (next_data_block_idx != -1)
        {
            next_data_block_idx = data_blocks[next_data_block_idx].offset;
            if(next_data_block_idx != -1){
                data_blocks[next_data_block_idx].offset = -1;
                data_blocks_idxs.push_back(next_data_block_idx);
            }
        }
        for(auto idx:data_blocks_idxs)
            data_maps[idx] = false;
    }

    INode *get_inode_in_dictionary(){

    }
};

int main(int argc, char* argv[]) {
    VirtualDisc virtual_disc;
    virtual_disc.set_name((char*)"test");
    virtual_disc.create((char*)"test", 1024*1024);
    virtual_disc.open();
    virtual_disc.close();
    virtual_disc.open();
    virtual_disc.create_directory("a");
    virtual_disc.create_directory("b");
    virtual_disc.create_directory("c");
    char direcotries[80];
    char file[80];
    char file_destinaiton[80];
    char link_name[80];
    strcpy(direcotries, "a/x");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/y");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/z");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/x/k");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/x/l");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/x/m");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/z/q");
    virtual_disc.create_directory(direcotries);
    strcpy(direcotries, "a/z/w");
    virtual_disc.create_directory(direcotries);

    strcpy(direcotries, "a/x");
    strcpy(file, "matejko");
    virtual_disc.file_to_disc(direcotries, file);

    strcpy(direcotries, "a/x");
    strcpy(file, "matejko");
    strcpy(file_destinaiton, "matejko_out");
    virtual_disc.file_from_disc(direcotries, file, file_destinaiton);

    strcpy(direcotries, "");
    virtual_disc.show_files_tree(direcotries);

    std::cout << virtual_disc.get_left_space() << "\n";

    strcpy(direcotries, "a");
    std::cout << virtual_disc.get_size(direcotries) << "\n";

    strcpy(direcotries, "a");
    std::cout << virtual_disc.get_full_size(direcotries) << "\n";

    strcpy(direcotries, "a/x");
    strcpy(file, "matejko");
    strcpy(file_destinaiton, "a/y");
    strcpy(link_name, "matejkolink");
    virtual_disc.create_link(direcotries, file, file_destinaiton, link_name);

    strcpy(direcotries, "");
    strcpy(file, "a");
    strcpy(file_destinaiton, "a/z");
    strcpy(link_name, "alink");
    virtual_disc.create_link(direcotries, file, file_destinaiton, link_name);

    strcpy(direcotries, "a/x");
    strcpy(file, "matejko");
    virtual_disc.remove_link(direcotries, file);

    strcpy(direcotries, "");
    strcpy(file, "a");
    virtual_disc.remove_link(direcotries, file);

    strcpy(direcotries, "");
    std::cout << virtual_disc.get_full_size(direcotries) << "\n";

    strcpy(direcotries, "");
    virtual_disc.show_files_tree(direcotries);

    virtual_disc.close();
    return 0;
}