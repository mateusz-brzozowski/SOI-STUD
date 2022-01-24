#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define BLOCK_SIZE 8192

#define FILENAME_LEN 16

#define DISCNAME_LEN 16

// 1/MAX_FILES_PROPORTION will be space for inodes
#define MAX_FILES_PROPORTION 8

#define DIR_ELEMS_PER_BLOCK (BLOCK_SIZE / sizeof(directory_element))

enum inode_type{
    EMPTY = 0,
    TYPE_FILE = 1,
    TYPE_DIRECTORY = 2
};

struct datablock
{
    u_int64_t next;
    u_int8_t data[BLOCK_SIZE];
};

struct inode
{
    u_int64_t size;
    u_int64_t first_data_block;
    u_int8_t type;
    u_int8_t references_num;
};

struct directory_element
{
    u_int16_t inode_id;
    u_int8_t used;
    u_int8_t name[FILENAME_LEN];
};

struct superblock
{

    u_int64_t disc_size;
    u_int64_t nodes_offset;
    u_int64_t data_map_offset;
    u_int64_t datablocks_offset;

    u_int32_t datablocks_num;
    u_int32_t free_datablocks_num;
    u_int32_t inodes_num;
    u_int32_t free_inodes_num;

    u_int8_t name[DISCNAME_LEN];

};

class VirtualDisc {
public:
    FILE* file;
    char name[DISCNAME_LEN];
    superblock superblock_;
    u_int64_t node_tab_len;
    u_int64_t data_map_len;
    u_int64_t datablock_tab_len;
    inode *node_tab;
    bool *data_map;
    datablock *datablock_tab;
    void set_name(char file_name[]);

    void create(char file_name[], u_int64_t size);
    void open();
    void save();

    bool validate_filename(char* filename);
    u_int32_t find_free_node();
    u_int32_t find_free_datablock();
    void remove_datablock_from_inode(inode* node, uint32_t idx);

    inode* find_in_dir(inode* dir, char* filename);
    directory_element* find_elem_in_dir(inode* dir, char* filename);
    inode* find_dir_inode(char* path);
    void add_elem_to_dir(inode* dir, char* name, u_int16_t node_id);

    void print_dir_content(inode* dir);
    uint64_t get_sum_files_in_dir(inode* dir);
    uint64_t get_sum_files_in_dir_recursive(inode* dir);

    void make_dir(char* path);

    void copy_file_to_disc(char* filename, char* path);
    void copy_file_from_disc(char* path_to_dir, char* filename, char* name_on_pd);
    void link_to_inode(inode* file_node, char* path);

};

void VirtualDisc::set_name(char file_name[])
{
    strncpy((char*)this->name, file_name, DISCNAME_LEN);
}

void VirtualDisc::create(char file_name[], u_int64_t size)
{
    strncpy((char*)superblock_.name, file_name, FILENAME_LEN);
    strncpy((char*)name, file_name, FILENAME_LEN);
    file = fopen((char*)superblock_.name, "wb+");

    superblock_.disc_size = size;
    unsigned inodes = size / MAX_FILES_PROPORTION / sizeof(inode);
    u_int32_t possible_datablocks_num = (superblock_.disc_size - sizeof(superblock) - inodes * sizeof(inode)) / sizeof(datablock);
    superblock_.datablocks_num = (superblock_.disc_size - sizeof(superblock) - inodes * sizeof(inode) - possible_datablocks_num * sizeof(bool)) / sizeof(datablock);
    superblock_.free_datablocks_num = superblock_.datablocks_num;
    superblock_.inodes_num = inodes;
    superblock_.free_inodes_num = inodes - 1; // One for root dir
    superblock_.nodes_offset = sizeof(superblock);
    superblock_.data_map_offset = superblock_.nodes_offset + superblock_.inodes_num * sizeof(inode);
    superblock_.datablocks_offset = superblock_.data_map_offset + superblock_.datablocks_num * sizeof(bool);

    fwrite(&superblock_, sizeof(superblock), 1, file);

    inode root_inode{};
    root_inode.first_data_block = -1;
    root_inode.type = inode_type::TYPE_DIRECTORY;
    root_inode.references_num = 1;

    fwrite(&root_inode, sizeof(root_inode), 1, file);

    for(int i = 0; i < inodes - 1; ++i)
    {
        inode node{};
        node.first_data_block = -1;
        fwrite(&node, sizeof(node), 1, file);
    }

    for(int i = 0; i < superblock_.datablocks_num; i++) {
        int j = 0;
        fwrite(&j, sizeof(bool), 1, file);
    }

    for(int i = 0; i < superblock_.datablocks_num; i++)
    {
        datablock db{};
        db.next = -1;
        fwrite(&db, sizeof(db), 1, file);
    }

    fclose(file);
    file = nullptr;
}

void VirtualDisc::open()
{
    // Open file
    file = fopen(this->name, "rb+");

    //Read superblock
    fread(&this->superblock_, sizeof(superblock), 1, file);

    this->node_tab_len = this->superblock_.inodes_num;
    this->data_map_len = this->superblock_.datablocks_num;
    this->datablock_tab_len = this->superblock_.datablocks_num;

    //Read inodes
    node_tab = new inode[superblock_.inodes_num];
    fread(node_tab, sizeof(inode), superblock_.inodes_num, file);

    //Read data map
    data_map = new bool[superblock_.datablocks_num];
    fread(data_map, sizeof(bool), superblock_.datablocks_num, file);

    //Read datablocks
    datablock_tab = new datablock[superblock_.datablocks_num];
    fread(datablock_tab, sizeof(datablock), superblock_.datablocks_num, file);

}

void VirtualDisc::save()
{
    fseek(file, 0, 0);
    fwrite(&this->superblock_, sizeof(superblock), 1, file);
    fwrite(this->node_tab, sizeof(inode), this->node_tab_len, file);
    fwrite(this->data_map, sizeof(bool), this->data_map_len, file);
    fwrite(this->datablock_tab, sizeof(datablock), this->datablock_tab_len, file);
    fclose(file);
    file = nullptr;
}

bool VirtualDisc::validate_filename(char* filename)
{
    return !(strlen(filename) >= FILENAME_LEN || strpbrk(filename, "/"));
}

u_int32_t VirtualDisc::find_free_node()
{
    for(int i = 0; i < superblock_.inodes_num; i++)
    {
        if(node_tab[i].type == inode_type::EMPTY)
        {
            return i;
        }
    }
    return -1;
}

u_int32_t VirtualDisc::find_free_datablock()
{
    for (int i = 0; i < superblock_.datablocks_num; i++)
    {
        if(data_map[i] == false)
        {
            return i;
        }
    }
    return -1;

}

directory_element* VirtualDisc::find_elem_in_dir(inode* dir, char* filename)
{
    if(dir->type != inode_type::TYPE_DIRECTORY)
        return nullptr;

    uint64_t datablock_idx = dir->first_data_block;
    while(datablock_idx != -1)
    {
        directory_element* directory_elements = (directory_element*)datablock_tab[datablock_idx].data;
        for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
        {
            directory_element curr_dir_elem = directory_elements[i];
            if ((curr_dir_elem.used == true) && strcmp((char*)curr_dir_elem.name, filename) == 0)
            {
                return &directory_elements[i];
            }
        }
        datablock_idx = datablock_tab[datablock_idx].next;
    }
    return nullptr;
}

inode* VirtualDisc::find_in_dir(inode* dir, char* filename)
{
    directory_element* elem = find_elem_in_dir(dir, filename);
    if(!elem)
        return nullptr;
    return &node_tab[elem->inode_id];
}

inode* VirtualDisc::find_dir_inode(char* path)
{
    // Start at root dir
    inode* curr_path = node_tab;

    for(char* curr_dir = strtok(path, "/"); curr_dir != nullptr; curr_dir = strtok(nullptr, "/"))
    {
        if(!validate_filename(curr_dir))
            std::cout <<"Invalid path" << std::endl;

        inode* next = find_in_dir(curr_path, curr_dir);

        if(next && next->type == inode_type::TYPE_DIRECTORY)
        {
            curr_path = next;
        }
        else
        {
            std::cerr << "Not a directory\n";
            return nullptr;
        }

    }
    return curr_path;
}

void VirtualDisc::add_elem_to_dir(inode* dir, char* name, u_int16_t node_id)
{
    directory_element dir_elem{0};
    dir_elem.inode_id = node_id;
    dir_elem.used = true;
    strncpy((char*)dir_elem.name, name, FILENAME_LEN);

    uint64_t data_block_idx = dir->first_data_block;
    uint64_t last_data_block = data_block_idx;
    bool success = false;
    while(data_block_idx != -1)
    {
        directory_element* directory_elements = (directory_element*)datablock_tab[data_block_idx].data;
        for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
        {
            directory_element curr_dir_elem = directory_elements[i];
            if (!curr_dir_elem.used)
            {
                directory_elements[i] = dir_elem;
                success = true;
                break;
            }
        }
        if (datablock_tab[data_block_idx].next)
        {
            last_data_block = data_block_idx;
        }
        data_block_idx = datablock_tab[data_block_idx].next;
    }
    if(!success)
    {
        datablock_tab[last_data_block].next = find_free_datablock();
        if(datablock_tab[last_data_block].next = -1)
        {
            std::cerr << "No free datablocks found.\n";
            return;
        }
        uint64_t new_block_id = datablock_tab[last_data_block].next;
        if (datablock_tab[last_data_block].next == -1)
            return;
        *(directory_element*)(datablock_tab[new_block_id].data) = dir_elem;
        data_map[datablock_tab[last_data_block].next] = true;
    }
}

void VirtualDisc::remove_datablock_from_inode(inode* node, uint32_t idx)
{
    uint32_t curr_db_idx = node->first_data_block;
    if(curr_db_idx == idx)
    {
        node->first_data_block = -1;
        return;
    }
    while(curr_db_idx != -1)
    {
        if(datablock_tab[curr_db_idx].next == idx)
        {
            datablock_tab[curr_db_idx].next = -1;
            return;
        }
        curr_db_idx = datablock_tab[curr_db_idx].next;
    }
}

void VirtualDisc::copy_file_to_disc(char* filename, char* path)
{
    inode* dir = find_dir_inode(path);
    if (!dir)
    {
        std::cerr << "Not a valid path\n";
        return;
    }
    if(find_in_dir(dir, filename))
    {
        std::cerr << "File already exists\n";
        return;
    }
    u_int32_t file_inode = find_free_node();
    if(file_inode == -1)
        {
            std::cerr << "No free nodes found.\n";
            return;
        }

    node_tab[file_inode].references_num = 1;
    node_tab[file_inode].type = inode_type::TYPE_FILE;
    node_tab[file_inode].size = 0;

    add_elem_to_dir(dir, filename, file_inode);

    FILE* file_to_copy = fopen(filename, "rb+");

    bool has_data = true;

    node_tab[file_inode].first_data_block = find_free_datablock();
    if(node_tab[file_inode].first_data_block == -1)
    {
        std::cerr << "No free datablocks\n";
        return;
    }

    uint32_t curr_datablock_id = node_tab[file_inode].first_data_block;

    while(curr_datablock_id != -1)
    {
        uint read_amount_this_block = 0;
        uint read_amount = 0;
        while(read_amount_this_block < BLOCK_SIZE)
        {
            read_amount = fread(datablock_tab[curr_datablock_id].data, 1, BLOCK_SIZE - read_amount_this_block, file_to_copy);

            if(!read_amount && feof(file_to_copy))
            {
                break;
            }
            else if (!read_amount)
            {
                std::cerr << "Error while reading file\n";
                return;
            }

            read_amount_this_block += read_amount;
            node_tab[file_inode].size += read_amount;
        }
        if(!read_amount_this_block)
        {
            remove_datablock_from_inode(&node_tab[file_inode], curr_datablock_id);
            curr_datablock_id = -1;
        }
        else if (feof(file_to_copy))
        {
            data_map[curr_datablock_id] = true;
            datablock_tab[curr_datablock_id].next = -1;
            curr_datablock_id = -1;
        }
        else
        {
            data_map[curr_datablock_id] = true;
            datablock_tab[curr_datablock_id].next = find_free_datablock();
            if(datablock_tab[curr_datablock_id].next == -1)
            {
                std::cerr << "No free datablocks\n";
                return;
            }
            curr_datablock_id = datablock_tab[curr_datablock_id].next;
        }

    }
}

void VirtualDisc::copy_file_from_disc(char* path_to_dir, char* filename, char* name_on_pd)
{
    inode* dir = find_dir_inode(path_to_dir);
    if(!dir)
    {
        std::cerr << "Invalid path to directory\n";
        return;
    }

    inode* file = find_in_dir(dir, filename);
    if(!file)
    {
        std::cerr << "Invalid filename\n";
        return;
    }

    FILE* file_on_pd = fopen(name_on_pd, "wb+");

    uint32_t curr_datablock_index = file->first_data_block;
    uint64_t size_to_read = file->size;
    while(curr_datablock_index != -1)
    {
        uint8_t* data = datablock_tab[curr_datablock_index].data;
        fwrite(data, (size_to_read < BLOCK_SIZE ? size_to_read : BLOCK_SIZE), 1, file_on_pd);
        size_to_read -= (size_to_read < BLOCK_SIZE ? size_to_read : BLOCK_SIZE);
        curr_datablock_index = datablock_tab[curr_datablock_index].next;
    }
}

void VirtualDisc::make_dir(char* path)
{
    // Start at root dir
    inode* curr_path = node_tab;

    for(char* curr_dir = strtok(path, "/"); curr_dir != nullptr; curr_dir = strtok(nullptr, "/"))
    {
        if(!validate_filename(curr_dir))
            std::cout <<"Invalid path" << std::endl;

        inode* next = find_in_dir(curr_path, curr_dir);

        if(next && next->type == inode_type::TYPE_DIRECTORY)
        {
            curr_path = next;
        }
        else if (next)
        {
            std::cerr << "Not a directory\n";
            return;
        }
        else
        {
            u_int32_t dir_node = find_free_node();
            if(dir_node == -1)
                {
                    std::cerr << "No free nodes found.\n";
                    return;
                }

            node_tab[dir_node].references_num = 1;
            node_tab[dir_node].type = inode_type::TYPE_DIRECTORY;

            directory_element dir_elem{0};
            dir_elem.inode_id = dir_node;
            dir_elem.used = true;
            strncpy((char*)dir_elem.name, curr_dir, FILENAME_LEN);
            dir_elem.used = true;

            //if to wyjebania
            if (curr_path->first_data_block == -1)
            {
                curr_path->first_data_block = find_free_datablock();
                if (curr_path->first_data_block == -1)
                    {
                        std::cerr << "No free datablocks found.\n";
                        return;
                    }
                *(directory_element*)(datablock_tab[curr_path->first_data_block].data) = dir_elem;
                data_map[curr_path->first_data_block] = true;
            }
            else
            {
                uint64_t data_block_idx = curr_path->first_data_block;
                uint64_t last_data_block = data_block_idx;
                bool success = false;
                while(data_block_idx != -1)
                {
                    directory_element* directory_elements = (directory_element*)datablock_tab[data_block_idx].data;
                    for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
                    {
                        directory_element curr_dir_elem = directory_elements[i];
                        if (!curr_dir_elem.used)
                        {
                            directory_elements[i] = dir_elem;
                            success = true;
                            break;
                        }
                    }
                    if (datablock_tab[data_block_idx].next)
                    {
                        last_data_block = data_block_idx;
                    }
                    data_block_idx = datablock_tab[data_block_idx].next;
                }
                if(!success)
                {
                    datablock_tab[last_data_block].next = find_free_datablock();
                    if(datablock_tab[last_data_block].next = -1)
                    {
                        std::cerr << "No free datablocks found.\n";
                        return;
                    }
                    uint64_t new_block_id = datablock_tab[last_data_block].next;
                    if (datablock_tab[last_data_block].next == -1)
                        return;
                    *(directory_element*)(datablock_tab[new_block_id].data) = dir_elem;
                    data_map[datablock_tab[last_data_block].next] = true;
                }
            }
            curr_path = &node_tab[dir_node];
        }

    }

}

void VirtualDisc::print_dir_content(inode* dir)
{
    if(!(dir->type == inode_type::TYPE_DIRECTORY))
    {
        std::cerr << "Not a directory\n";
        return;
    }
    uint64_t datablock_idx = dir->first_data_block;
    while(datablock_idx != -1)
    {
        directory_element* directory_elements = (directory_element*)datablock_tab[datablock_idx].data;
        for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
        {
            directory_element curr_dir_elem = directory_elements[i];
            if (curr_dir_elem.used == false)
            {
                continue;
            }
            std::cout << "- " << curr_dir_elem.name << std::endl;
            std::cout << "\tinode id: " << curr_dir_elem.inode_id << std::endl;
            std::cout << "\ttype: " << std::to_string(node_tab[curr_dir_elem.inode_id].type) << std::endl;

        }
        datablock_idx = datablock_tab[datablock_idx].next;
    }
}

uint64_t VirtualDisc::get_sum_files_in_dir(inode* dir)
{
    uint64_t sum = 0;
    uint64_t curr_datablock_idx = dir->first_data_block;
    while(curr_datablock_idx != -1)
    {
        directory_element* directory_elements = (directory_element*)datablock_tab[curr_datablock_idx].data;
        for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
        {
            directory_element curr_dir_elem = directory_elements[i];
            if(!curr_dir_elem.used)
                continue;
            inode node = node_tab[curr_dir_elem.inode_id];
            sum += node.size;
        }
        curr_datablock_idx = datablock_tab[curr_datablock_idx].next;
    }
    return sum;
}

uint64_t VirtualDisc::get_sum_files_in_dir_recursive(inode* dir)
{
    uint64_t sum = 0;
    uint64_t curr_datablock_idx = dir->first_data_block;
    while(curr_datablock_idx != -1)
    {
        directory_element* directory_elements = (directory_element*)datablock_tab[curr_datablock_idx].data;
        for(int i = 0; i < DIR_ELEMS_PER_BLOCK; i++)
        {
            directory_element curr_dir_elem = directory_elements[i];
            if(!curr_dir_elem.used)
                continue;
            inode node = node_tab[curr_dir_elem.inode_id];
            if(node.type == inode_type::TYPE_DIRECTORY)
            {
                sum += get_sum_files_in_dir_recursive(&node);
            }
            else if(node.type == inode_type::TYPE_FILE)
            {
                sum += node.size;
            }
        }
        curr_datablock_idx = datablock_tab[curr_datablock_idx].next;
    }
    return sum;
}

int main(int argc, char* argv[])
{
    VirtualDisc vd;
    // vd.set_name("test");
    vd.create("test", 1024*1024);
    vd.open();
    // for(int i = 0; i < 420; i++)
    // {
    //     std::string tmp = std::to_string(i);
    //     vd.make_dir((char*)tmp.c_str());
    // }
    // char dirs[80];
    // strcpy(dirs, "a/b/c");
    // vd.make_dir(dirs);
    // vd.make_dir("katalog2");
    char dirs2[80];
    strcpy(dirs2, "a/b/c/d");
    vd.make_dir(dirs2);
    char file[80];
    strcpy(file, "dupa");
    strcpy(dirs2, "a/b");
    vd.copy_file_to_disc(file, dirs2);
    vd.save();
    vd.open();
    char path[80];
    strcpy(path, "");
    uint64_t sum = vd.get_sum_files_in_dir_recursive(vd.find_dir_inode(path));
    std::cout << sum << std::endl;
    // vd.print_dir_content(vd.find_dir_inode(path));
    // char fileonpd[80];
    // strcpy(fileonpd, "dupa_test");
    // vd.copy_file_from_disc(path, file, fileonpd);
    vd.save();

    // std::cout << ((directory_element*)vd.datablock_tab[vd.node_tab[0].first_data_block].data)->name;
    // std::cout << DIR_ELEMS_PER_BLOCK;
    // vd.save();
    return 0;
}