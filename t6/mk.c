#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/**************************
 * FILE SYSTEM STRUCTURES *
 **************************/

// Some math for the whole file system:
//
// For BLOCK_SIZE = n, we can store 32 * ((n / 4) - 2) blocks
// So, assuming block size = 8192:
// - We can have at most 65472 blocks
// - Thus, the max size of the file system is 536346624 B = 511.5 MiB
// - To correctly identify a block we need ⌈log2(65472)⌉ = 16 bits
// Assuming 61 blocks per inode, the max file size is 499712

#define EXPECTED_MAGIC 0x73667376

#define BLOCK_SIZE 8192
#define BLOCK_MAP_SIZE ((BLOCK_SIZE / 4) - 2)
#define INODE_BLOCK_COUNT 61

#define INODE_BLOCKS_PROPORTION 8

#define MAX_BLOCK_COUNT (32 * BLOCK_MAP_SIZE)
#define MAX_FS_SIZE (BLOCK_SIZE * BLOCK_MAP_SIZE * 32)
#define MAX_FILE_SIZE (BLOCK_SIZE * INODE_BLOCK_COUNT)
#define MAX_FILENAME_LEN 13

static_assert(MAX_BLOCK_COUNT < UINT16_MAX,
              "block count must fit in a uint16_t");
static_assert(MAX_FILE_SIZE < UINT32_MAX, "file size must fit in a uint32_t");

enum INodeType { INODE_NIL = 0, INODE_FILE = 1, INODE_DIRECTORY = 2 };

typedef struct {
    uint32_t magic;
    uint32_t inode_blocks;
    uint32_t block_map[BLOCK_MAP_SIZE];
} superblock_t;

typedef struct {
    uint32_t filesize;
    uint16_t content_blocks[INODE_BLOCK_COUNT];
    uint8_t type;
    uint8_t ref_count;
} inode_t;

typedef struct {
    uint16_t inode_idx;
    uint8_t used;
    int8_t name[MAX_FILENAME_LEN];
} directory_entry_t;

static_assert(sizeof(superblock_t) == BLOCK_SIZE,
              "superblock must be the size of a single block");

static_assert((BLOCK_SIZE % sizeof(inode_t)) == 0,
              "inode size must perfectly divide a single block");

static_assert((BLOCK_SIZE % sizeof(directory_entry_t)) == 0,
              "directory entry size must perfectly divide a single block");

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(inode_t))
#define DIR_ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(directory_entry_t))

/**************************
 * CURRENT FS DESCRIPTORS *
 *    (PROGRAM STATE)     *
 **************************/

static int fd = -1;

static superblock_t* superblock = NULL;
static inode_t* inodes = NULL;
static uint32_t inodes_size = 0;
static void* allocated_block = NULL;

void cleanup(void) {
    int result;

    if (allocated_block) {
        result = munmap(allocated_block, BLOCK_SIZE);
        if (result < 0) fprintf(stderr, "munmap(block): %m\n");
        allocated_block = NULL;
    }

    if (inodes) {
        assert(superblock && "inodes allocated without a superblock");
        result = munmap(inodes, BLOCK_SIZE * superblock->inode_blocks);
        if (result < 0) fprintf(stderr, "munmap(inodes): %m\n");

        inodes = NULL;
        inodes_size = 0;
    }

    if (superblock) {
        result = munmap(superblock, BLOCK_SIZE);
        if (result < 0) fprintf(stderr, "munmap(superblock): %m\n");
        superblock = NULL;
    }

    if (fd > 0) {
        result = close(fd);
        if (result < 0) fprintf(stderr, "close(fd): %m\n");
        fd = -1;
    }
}

/******************
 * HELPER METHODS *
 ******************/

/**
 * Checks whether a particular block is used
 */
bool block_is_used(unsigned block_idx) {
    assert(superblock && "A superblock must be allocated");
    assert(block_idx < MAX_BLOCK_COUNT && "block_idx too big");
    unsigned h = block_idx / 32;
    unsigned l = block_idx % 32;

    return superblock->block_map[h] & ((uint32_t)1 << l);
}

/**
 * Marks a particular block (in the superblock's block_map) as used
 */
void block_set_used(unsigned block_idx) {
    assert(superblock && "A superblock must be allocated");
    assert(block_idx < MAX_BLOCK_COUNT && "block_idx too big");
    unsigned h = block_idx / 32;
    unsigned l = block_idx % 32;

    superblock->block_map[h] |= ((uint32_t)1 << l);
}

/**
 * Marks a particular block (in the superblock's block_map) as unused
 */
void block_clear_used(unsigned block_idx) {
    assert(superblock && "A superblock must be allocated");
    assert(block_idx < MAX_BLOCK_COUNT && "block_idx too big");
    unsigned h = block_idx / 32;
    unsigned l = block_idx % 32;

    superblock->block_map[h] &= ~((uint32_t)1 << l);
}

/**
 * Counts how many blocks are currently used.
 */
size_t count_used_blocks() {
    size_t count = 0;
    for (int i = 0; i < BLOCK_MAP_SIZE; ++i)
        count += __builtin_popcount(superblock->block_map[i]);
    return count;
}

/**
 * Counts how many inodes are currently used.
 */
size_t count_used_inodes() {
    size_t count = 0;
    for (uint32_t i = 0; i < inodes_size; ++i)
        count += inodes[i].type != INODE_NIL;
    return count;
}

/**
 * Returns the index of the first empty inode,
 * or `0` if no free nodes are available.
 */
uint32_t find_empty_inode(void) {
    for (uint32_t i = 0; i < inodes_size; ++i) {
        if (inodes[i].type == INODE_NIL) return i;
    }

    return 0;
}

/**
 * Returns the index of the first free block,
 * or `0` id no free blocks are available.
 */
uint32_t find_empty_block(void) {
    for (uint32_t i = 0; i < MAX_BLOCK_COUNT; ++i) {
        if (!block_is_used(i)) return i;
    }

    return 0;
}

/**
 * Checks whether a vsfs filename is considered valid - that is:
 * It contains at most 12 chars, doesn't contain '/',
 * and isn't one of the following: "", ".", "..".
 */
bool is_valid_filename(const char* vsfs_filename) {
    if (vsfs_filename[0] == 0 || strlen(vsfs_filename) >= MAX_FILENAME_LEN ||
        strcmp(".", vsfs_filename) == 0 || strcmp("..", vsfs_filename) == 0 ||
        strpbrk(vsfs_filename, "/"))
        return false;

    return true;
}

/**
 * Tries to memory-map a given block into `allocated_block`.
 *
 * No blocks can be allocated when invoking this function.
 * Exits on failure.
 */
void allocate_block_or_exit(uint32_t block_idx, bool writable) {
    assert(!allocated_block);
    allocated_block =
        mmap(NULL, BLOCK_SIZE, PROT_READ | (writable ? PROT_WRITE : 0),
             MAP_SHARED, fd, block_idx * BLOCK_SIZE);
    if (!allocated_block) {
        perror("mmap(block)");
        exit(1);
    }
}

/**
 * Tries to unmap the current allocated block.
 * Exits on failure.
 */
void deallocate_block_or_exit(void) {
    assert(allocated_block);
    if (munmap(allocated_block, BLOCK_SIZE) < 0) {
        perror("munmap(block)");
        exit(1);
    }
    allocated_block = NULL;
}

/**
 * Puts the charachter C N times into the given sink.
 */
void put_n_chars(FILE* sink, char c, int n) {
    for (int i = 0; i < n; ++i) putc(c, sink);
}

/****************
 * VSFS OPENING *
 ****************/

/**
 * Generates a new, valid vsfs at the provided path, of size `size`.
 * Memory-maps the new file's superblock and inodes.
 *
 * Exits on failure.
 */
void open_new_vsfs(const char* host_path, uint64_t size) {
    assert(fd < 0 && "no file must be opened");

    // Verify the file size
    if (size > MAX_FS_SIZE) {
        fprintf(stderr, "Too big file size (max %lo)", size);
        exit(2);
    }
    if (size < 3 * BLOCK_SIZE) {
        fprintf(stderr, "Too small file size (min %d)", 3 * BLOCK_SIZE);
        exit(2);
    }

    // Create the file
    fd = open(host_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Resize the file - which is automatically zero-filled
    int result = ftruncate(fd, size);
    if (result < 0) {
        perror("ftruncate");
        exit(1);
    }

    // Calculate the amount of inode blocks
    uint32_t total_blocks = size / BLOCK_SIZE;
    uint32_t inode_blocks = total_blocks / INODE_BLOCK_COUNT;
    if (!inode_blocks) inode_blocks = 1;

    // Map the superblock
    superblock = mmap(NULL, sizeof(superblock_t), PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (!superblock) {
        perror("mmap(superblock)");
        exit(1);
    }

    superblock->magic = EXPECTED_MAGIC;
    superblock->inode_blocks = inode_blocks;

    // Mark the superblock and inode blocks as used
    for (uint16_t i = 0; i < (inode_blocks + 1); ++i) block_set_used(i);

    // Map the inodes
    inodes_size = inode_blocks * INODES_PER_BLOCK;
    inodes = mmap(NULL, inode_blocks * BLOCK_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, BLOCK_SIZE);
    if (!inodes) {
        perror("mmap(inodes)");
        exit(1);
    }

    // Make the first inode a directory (the root directory)
    inodes[0].ref_count = 1;
    inodes[0].type = INODE_DIRECTORY;

    // Mark out-of-bounds blocks as reserved
    for (uint32_t block = MAX_BLOCK_COUNT - 1; block >= total_blocks; --block)
        block_set_used(block);
}

/**
 * Tries to open an existing vsfs file.
 *
 * Exits on failure.
 */
void open_existing_vsfs(const char* host_path) {
    assert(fd < 0 && "no file must be opened");

    // Open the file
    fd = open(host_path, O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Map the superblock
    superblock =
        mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (superblock == NULL) {
        perror("mmap(superblock)");
        exit(1);
    }

    // Verify the magic number
    if (superblock->magic != EXPECTED_MAGIC) {
        fputs("Magic value mismatch\n", stderr);
        exit(3);
    }

    // Map the inode blocks
    inodes = mmap(NULL, BLOCK_SIZE * superblock->inode_blocks,
                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, BLOCK_SIZE);
    if (inodes == NULL) {
        perror("mmap(inodes)");
        exit(1);
    }
    inodes_size = INODES_PER_BLOCK * superblock->inode_blocks;
}

/************************
 * FILE TREE NAVIGATION *
 ************************/

/**
 * Tries to find a file with a given filename in a directory -
 * returns that file's directory_entry.
 *
 * No blocks can be allocated when invoking this function.
 * Leaves the block containing the entry memory-mapped
 * (as long as anything other than NULL was returned).
 *
 * If the provided inode is not directory - returns NULL.
 * If a file with this given name doesn't exist - returns NULL.
 */
directory_entry_t* find_entry_in_directory(inode_t* directory,
                                           char* vsfs_filename) {
    assert(directory);
    if (directory->type != INODE_DIRECTORY) return NULL;

    for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
        if (!directory->content_blocks[i]) break;

        // mmap the block
        allocate_block_or_exit(directory->content_blocks[i], true);

        // search the directory for the wanted file;
        for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
            directory_entry_t* entry =
                ((directory_entry_t*)allocated_block) + j;
            if (entry->used && strcmp((char*)entry->name, vsfs_filename) == 0) {
                return entry;
            }
        }

        // De-allocate the directory block
        deallocate_block_or_exit();
    }

    return NULL;
}

/**
 * Tries to find a file with a given filename in a directory -
 * returns that file's inode.
 *
 * No blocks can be allocated when invoking this function.
 *
 * If the provided inode is not directory - returns NULL.
 * If a file with this given name doesn't exist - returns NULL.
 */
inode_t* find_in_directory(inode_t* directory, char* vsfs_filename) {
    directory_entry_t* entry =
        find_entry_in_directory(directory, vsfs_filename);
    if (!entry) return NULL;

    inode_t* inode = inodes + entry->inode_idx;
    deallocate_block_or_exit();
    return inode;
}

/**
 * Finds a file by a given path from the root of the filesystem.
 *
 * No blocks can be allocated when invoking this function.
 *
 * Exits on failure.
 */
inode_t* find_nested(char* vsfs_path) {
    // Remove initial '/'
    if (vsfs_path[0] == '/') ++vsfs_path;

    // Start the search at vsfs root
    inode_t* pwd = inodes;

    // Walk the tree to find the appriopate inode
    for (char* path_part = strtok(vsfs_path, "/"); path_part != NULL;
         path_part = strtok(NULL, "/")) {
        if (!is_valid_filename(path_part)) {
            fprintf(stderr, "Invalid path part: '%s'\n", path_part);
            exit(2);
        }

        if (pwd->type != INODE_DIRECTORY) {
            fputs("Not a directory!\n", stderr);
            exit(2);
        }

        pwd = find_in_directory(pwd, path_part);

        if (!pwd) {
            fputs("No such file!\n", stderr);
            exit(2);
        }
    }

    return pwd;
}

/**
 * Finds the dirname of the provided path,
 * and saves the basename of the provided path in `vsfs_filename`.
 *
 * Starts the search at `pwd` - if that's NULL starts at the root of the
 * filesystem.
 *
 * The generic idea is that `find_directory_containing("/foo/bar/baz", &str)`
 * will return the inode corresponding to `/foo/bar` and save `baz` to `str`.
 *
 * Exits on failure.
 */
inode_t* find_directory_containing(char* vsfs_path, char** vsfs_filename,
                                   inode_t* pwd) {
    if (pwd == NULL) {
        // Remove initial '/' and start at fs root (by default)
        if (vsfs_path[0] == '/') ++vsfs_path;
        pwd = inodes;
    }

    // Start the search at vsfs root
    char* next = strtok(vsfs_path, "/");
    char* next2 = strtok(NULL, "/");

    while (next2) {
        pwd = find_in_directory(pwd, next);

        if (pwd == NULL) {
            fputs("No such directory\n", stderr);
            exit(2);
        } else if (pwd->type != INODE_DIRECTORY) {
            fputs("Not a directory\n", stderr);
            exit(2);
        }

        next = next2;
        next2 = strtok(NULL, "/");
    }

    if (vsfs_filename) *vsfs_filename = next;
    return pwd;
}

/************************
 * DIRECTORY OPERATIONS *
 ************************/

/**
 * Finds an empty directory entry in a directory;
 * allocating a new block if no space in existing blocks is found.
 *
 * No blocks can be allocated at the time of calling,
 * and on return the block containing the entry will be left
 * memory-mapped.
 *
 * Exits on failure.
 */
directory_entry_t* add_entry_to_directory(inode_t* directory) {
    assert(directory && directory->type == INODE_DIRECTORY);

    for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
        if (directory->content_blocks[i]) {
            // mmap the block
            allocate_block_or_exit(directory->content_blocks[i], true);

            // search the directory for an empty entry
            for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
                directory_entry_t* entry =
                    ((directory_entry_t*)allocated_block) + j;
                if (!entry->used) return entry;
            }

            // Nothing found - deallocate the block and try the next one
            deallocate_block_or_exit();
        } else {
            // no empty entries in existing blocks - allocate a new block
            int64_t block_idx = find_empty_block();
            if (!block_idx) return NULL;

            // mmap the new block
            allocate_block_or_exit(block_idx, true);

            // mark the block as used
            block_set_used(block_idx);
            directory->content_blocks[i] = block_idx;

            // clear the block
            memset(allocated_block, 0, BLOCK_SIZE);

            // return the first element of the block
            return (directory_entry_t*)allocated_block;
        }
    }

    return NULL;
}

/**
 * Ensures a directory entry with a given name exists in a directory.
 *
 * If one exists - returns its inode;
 * creates a new directory entry.
 *
 * No blocks may be allocated when calling this function.
 * Leaves the block containing the entry memory-mapped.
 *
 * Exits on failure.
 */
directory_entry_t* ensure_entry_in_directory(inode_t* directory,
                                             char* vsfs_filename) {
    if (!is_valid_filename(vsfs_filename)) {
        fputs("Invalid filename\n", stderr);
        exit(2);
    }

    // First try to find an already-existing entry
    directory_entry_t* result =
        find_entry_in_directory(directory, vsfs_filename);
    if (result) return result;

    // Otherwise make a new entry
    directory_entry_t* new_entry = add_entry_to_directory(directory);
    if (!new_entry) {
        fputs("No free blocks\n", stderr);
    }

    new_entry->used = true;
    strncpy((char*)new_entry->name, vsfs_filename, MAX_FILENAME_LEN);

    return new_entry;
}

/**
 * Ensures a file with a given name exists in a directory.
 *
 * If one exists - returns its inode; otherwise
 * creates a new directory entry and a new inode.
 *
 * No blocks may be allocated when calling this function.
 * Exits on failure.
 */
inode_t* ensure_file_in_directory(inode_t* directory, char* vsfs_filename) {
    directory_entry_t* entry =
        ensure_entry_in_directory(directory, vsfs_filename);

    // Ensure an entry has its corresponding inode
    if (!entry->inode_idx) {
        uint32_t new_inode_idx = find_empty_inode();
        if (!new_inode_idx) {
            entry->used = false;
            fputs("No free inodes\n", stderr);
            exit(1);
        }
        entry->inode_idx = new_inode_idx;
    }

    inode_t* inode = inodes + entry->inode_idx;
    deallocate_block_or_exit();

    switch (inode->type) {
        case (INODE_FILE):
            break;

        case (INODE_NIL):
            inode->type = INODE_FILE;
            inode->ref_count = 1;
            break;

        default:
            fputs("Not a file!\n", stderr);
            exit(2);
    }

    return inode;
}

/**
 * Ensures all directories given in a path are present.
 *
 * Exits on failure - which might still leave the fs changed.
 */
void mkdir_nested(char* vsfs_path) {
    // Remove initial '/'
    if (vsfs_path[0] == '/') ++vsfs_path;

    // Start the search at vsfs root
    inode_t* pwd = inodes;

    // Recursively make directories
    for (char* path_part = strtok(vsfs_path, "/"); path_part != NULL;
         path_part = strtok(NULL, "/")) {
        if (!is_valid_filename(path_part)) {
            fprintf(stderr, "Invalid path part: '%s'\n", path_part);
            exit(2);
        }
        inode_t* next = find_in_directory(pwd, path_part);

        if (next && next->type == INODE_DIRECTORY) {
            pwd = next;
        } else if (next) {
            fputs("Not a directory\n", stderr);
            exit(2);
        } else {
            uint32_t new_dir_inode = find_empty_inode();
            if (!new_dir_inode) {
                fputs("No free inodes\n", stderr);
                exit(1);
            }

            directory_entry_t* new_dir_entry = add_entry_to_directory(pwd);
            if (!new_dir_entry) {
                fputs("No free blocks\n", stderr);
                exit(1);
            }

            inodes[new_dir_inode].ref_count = 1;
            inodes[new_dir_inode].type = INODE_DIRECTORY;
            new_dir_entry->used = true;
            new_dir_entry->inode_idx = new_dir_inode;
            strncpy((char*)new_dir_entry->name, path_part, MAX_FILENAME_LEN);

            // Unmap the directory entries block (allocated by
            // add_entry_to_directory)
            deallocate_block_or_exit();
            pwd = inodes + new_dir_inode;
        }
    }
}

/**
 * dump_directory_content prints the contents of a directory
 * onto stdout. Each entry is prefixed with `prefix` spaces.
 */
void dump_directory_content(inode_t* directory, unsigned prefix) {
    assert(directory && directory->type == INODE_DIRECTORY);

    for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
        if (!directory->content_blocks[i]) break;

        // mmap the block
        allocate_block_or_exit(directory->content_blocks[i], false);

        // iterate over the entries
        for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
            // Get the directory entry
            directory_entry_t* entry =
                ((directory_entry_t*)allocated_block) + j;
            if (!entry->used) continue;

            // Get the corresponding entry
            inode_t* entry_inode = inodes + entry->inode_idx;
            assert(entry_inode->type != INODE_NIL);

            // Print the prefix and the directory name
            put_n_chars(stdout, ' ', prefix);
            fputs((char*)entry->name, stdout);

            if (entry_inode->type == INODE_DIRECTORY) {
                // Show it's a directory with a slash -
                // then recurse into that directory
                putc('/', stdout);
                putc('\n', stdout);

                // NOTE: This tactic of node re-allocation is stupid,
                // but it works
                deallocate_block_or_exit();
                dump_directory_content(entry_inode, prefix + 2);
                allocate_block_or_exit(directory->content_blocks[i], false);
            } else {
                putc('\n', stdout);
            }
        }

        // De-allocate the directory block
        deallocate_block_or_exit();
    }
}

/*******************
 * FILE OPERATIONS *
 *******************/

/**
 * Copies data from `stream` to a file inode.
 *
 * If stdin is longer than MAX_FILE_SIZE, file is truncated.
 * Exits on failure - in such case only a part of stdin might get saved.
 */
void write_to_file(inode_t* file, FILE* stream) {
    assert(file && file->type == INODE_FILE &&
           "Writes can only be done to a file");

    file->filesize = 0;
    bool has_data = true;

    for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
        size_t read_this_chunk = 0, actually_read = 0;

        if (has_data) {
            // Data to read - allocate the block
            if (!file->content_blocks[i]) {
                file->content_blocks[i] = find_empty_block();
                if (!file->content_blocks[i]) {
                    fputs("No free blocks!\n", stderr);
                    exit(1);
                }
                block_set_used(file->content_blocks[i]);
            }
            allocate_block_or_exit(file->content_blocks[i], true);

            // Read the data
            while (has_data && read_this_chunk < BLOCK_SIZE) {
                actually_read = fread(allocated_block + read_this_chunk, 1,
                                      BLOCK_SIZE - read_this_chunk, stream);

                if (!actually_read && feof(stream)) {
                    has_data = false;
                } else if (!actually_read && ferror(stream)) {
                    fputs("Can't read input file", stderr);
                    exit(1);
                }

                read_this_chunk += actually_read;
                file->filesize += actually_read;
            }

            // Deallocate the block; and free it if actually no more data was
            // read
            deallocate_block_or_exit();

            if (read_this_chunk == 0) {
                block_clear_used(file->content_blocks[i]);
                file->content_blocks[i] = 0;
            }
        } else if (file->content_blocks[i]) {
            // No more data - ensure more blocks are cleared
            block_clear_used(file->content_blocks[i]);
            file->content_blocks[i] = 0;
        }
    }

    if (!feof(stream)) {
        fprintf(stderr, "File too long (truncated to %i bytes)\n",
                MAX_FILE_SIZE);
        exit(2);
    }
}

/**
 * Writes the content of the file to stdout.
 *
 * Exits on failure.
 */
void write_from_file(inode_t* file, FILE* sink) {
    assert(file);
    if (file->type != INODE_FILE) {
        fputs("Not a file!\n", stderr);
    }

    size_t total_to_read = file->filesize;

    for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
        if (!file->content_blocks[i]) continue;
        size_t to_read = total_to_read % BLOCK_SIZE;
        if (!to_read) to_read = BLOCK_SIZE;

        allocate_block_or_exit(file->content_blocks[i], false);
        size_t wrote = fwrite(allocated_block, 1, to_read, sink);
        deallocate_block_or_exit();

        if (ferror(sink)) {
            fputs("Can't write to output\n", stderr);
            exit(1);
        }

        assert(wrote == to_read && "Partial writes are not handled");
        total_to_read -= to_read;
    }

    assert(total_to_read == 0 &&
           "inode's filesize is longer than its real content");
}

/********************
 * OTHER OPERATIONS *
 ********************/

/**
 * Decrements the reference counter of a node -
 * and if that counter reached zero -
 * deallocates its content.
 *
 * No blocks can be allocated when calling this function.
 * Exits on failures.
 *
 * If a failure occurs when unlinking a directory -
 * the file system is left in a broken state.
 */
void unlink_inode(inode_t* inode) {
    assert(inode->ref_count > 0 &&
           "Can't unlink node whose reference counter is already zero");

    // Decrement node's reference count.
    // If it's not zero - don't do anything.
    if (--inode->ref_count) return;

    // Ref count reached zero - deallocation time!

    if (inode->type == INODE_FILE) {
        // Files - simply deallocate the blocks
        for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
            if (inode->content_blocks[i])
                block_clear_used(inode->content_blocks[i]);
        }
    } else if (inode->type == INODE_DIRECTORY) {
        // Directories - recursively unlink inodes

        for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
            if (!inode->content_blocks[i]) continue;

            // mmap the block
            allocate_block_or_exit(inode->content_blocks[i], true);

            // Unlink every child inode
            for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
                directory_entry_t* entry =
                    ((directory_entry_t*)allocated_block) + j;
                inode_t* entry_inode = inodes + entry->inode_idx;

                if (!entry->used) continue;

                // If the child inode is a directory - we must
                // deallocate and then reallocate the current block
                if (entry_inode->type == INODE_DIRECTORY) {
                    deallocate_block_or_exit();
                    unlink_inode(entry_inode);
                    allocate_block_or_exit(inode->content_blocks[i], true);
                } else {
                    unlink_inode(entry_inode);
                }
            }

            // Finally - clear the block
            deallocate_block_or_exit();
            block_clear_used(inode->content_blocks[i]);
        }
    }

    // Clear the inode data
    memset(inode, 0, sizeof(inode_t));
}

/**
 * Ensures a directory_entry for `vsfs_path` points
 * to the `target` inode.
 *
 * Contrary to other functions - it is an error if `vsfs_path` exists,
 * as linking is an operation on the containing directory, not the inode itself.
 *
 * Exits on failure.
 */
void link_inode(inode_t* target, char* vsfs_path) {
    assert(target && target->type != INODE_NIL);

    // Prevent ref_count overflow
    if (target->ref_count == UINT8_MAX) {
        fputs("Max link count for target exceeded", stderr);
        exit(2);
    }

    char* link_name = NULL;
    inode_t* link_directory =
        find_directory_containing(vsfs_path, &link_name, NULL);
    directory_entry_t* file =
        ensure_entry_in_directory(link_directory, link_name);

    // Existing file - fail
    if (file->inode_idx) {
        fputs("An inode at the target already exists\n", stderr);
        exit(2);
    }

    file->inode_idx = (uint16_t)(target - inodes);
    ++target->ref_count;
    deallocate_block_or_exit();
}

/**
 * Calculates the size of the content in a given inode.
 *
 * For file inodes - that's the size of the underlaying file.
 * For directory inodes - that's the sum of all contained directories and files.
 *
 * No blocks can be allocated when calling this function.
 * Exits on error.
 */
void calculate_content_size(inode_t* inode, size_t* size_arr) {
    assert(inode);
    unsigned inode_idx = inode - inodes;

    // If already calculated - don't recurse into
    if (size_arr[inode_idx]) return;

    switch (inode->type) {
        case INODE_NIL:
            break;

        case INODE_FILE:
            size_arr[inode_idx] = inode->filesize;
            break;

        case INODE_DIRECTORY:
            // Recursively walk over the directory to sum its content
            for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
                if (!inode->content_blocks[i]) break;

                // mmap the block
                allocate_block_or_exit(inode->content_blocks[i], false);

                // search the directory for the wanted file;
                for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
                    directory_entry_t* entry =
                        ((directory_entry_t*)allocated_block) + j;
                    if (!entry->used) continue;

                    if (!size_arr[entry->inode_idx]) {
                        inode_t* entry_inode = inodes + entry->inode_idx;

                        if (entry_inode->type == INODE_DIRECTORY) {
                            // NOTE: Stupid block allocation and deallocation
                            deallocate_block_or_exit();
                            calculate_content_size(entry_inode, size_arr);
                            allocate_block_or_exit(inode->content_blocks[i],
                                                   false);
                        } else {
                            calculate_content_size(entry_inode, size_arr);
                        }
                    }

                    size_arr[inode_idx] += size_arr[entry->inode_idx];
                }

                // De-allocate the directory block
                deallocate_block_or_exit();
            }
    }
}

/**
 * Truncates a file by `how_much` bytes - deallocating no-longer-required
 * blocks.
 *
 * Exits on error (if inode is not a file or how_much is bigger than the file)
 */
void truncate_inode(inode_t* file, uint32_t how_much) {
    assert(file);
    if (file->type != INODE_FILE) {
        fputs("Not a file!\n", stderr);
        exit(2);
    }

    if (file->filesize < how_much) {
        fputs("File is smaller than the amount of bytes to truncate\n", stderr);
        exit(2);
    }

    // Truncate the filesize
    file->filesize -= how_much;

    // Calculate the number of required blocks
    uint32_t blocks_required = file->filesize / BLOCK_SIZE;
    blocks_required += file->filesize % BLOCK_SIZE ? 1 : 0;

    // Deallocate any blocks that are no longer required
    for (uint32_t i = blocks_required; i < INODE_BLOCK_COUNT; ++i) {
        if (!file->content_blocks[i]) break;

        block_clear_used(file->content_blocks[i]);
        file->content_blocks[i] = 0;
    }
}

/**
 * Extends a file by `to_extend` bytes - allocating new blocks if required.
 *
 * Exits on error - which may leave the file only partially extended.
 */
void extend_inode(inode_t* file, uint32_t to_extend) {
    assert(file);
    if (file->type != INODE_FILE) {
        fputs("Not a file!\n", stderr);
        exit(2);
    }

    if (file->filesize + to_extend > MAX_FILE_SIZE) {
        fputs("Extension would exceed max file size\n", stderr);
        exit(2);
    }

    // Find the last allocated block - and how much data is allocated in that
    // block.
    uint32_t last_block_size = file->filesize % BLOCK_SIZE;
    int last_block_idx = -1;
    for (; last_block_idx < INODE_BLOCK_COUNT; ++last_block_idx) {
        if (!file->content_blocks[last_block_idx + 1] ||
            last_block_idx + 1 == INODE_BLOCK_COUNT)
            break;
    }

    // First, try to extend the current block
    if (last_block_size) {
        uint32_t extend_last_block_by =
            BLOCK_SIZE - (file->filesize % BLOCK_SIZE);
        if (extend_last_block_by > to_extend) extend_last_block_by = to_extend;

        // Allocate the last block
        assert(last_block_idx >= 0 && "File with non-zero size and no blocks");
        allocate_block_or_exit(file->content_blocks[last_block_idx], true);

        // Zero-out the extended part
        memset(allocated_block + last_block_size, 0, extend_last_block_by);

        // Deallocate the block
        deallocate_block_or_exit();

        // Update the sizes
        to_extend -= extend_last_block_by;
        file->filesize += extend_last_block_by;
    }

    // Second, allocate new blocks
    while (to_extend) {
        // Find a new block and allocate it
        file->content_blocks[++last_block_idx] = find_empty_block();
        if (!file->content_blocks[last_block_idx]) {
            fputs("No free blocks!\n", stderr);
            exit(1);
        }
        block_set_used(file->content_blocks[last_block_idx]);
        allocate_block_or_exit(file->content_blocks[last_block_idx], true);

        // Clear out the contents
        uint32_t bytes_used_in_block =
            to_extend < BLOCK_SIZE ? to_extend : BLOCK_SIZE;
        memset(allocated_block, 0, bytes_used_in_block);

        // Deallocate the block
        deallocate_block_or_exit();

        // Update the sizes
        to_extend -= bytes_used_in_block;
        file->filesize += bytes_used_in_block;
    }
}

/****************
 * ENTRY POINTS *
 ****************/

void usage(void) {
    fputs("Usage: ./vsfs path_to_vsfs action [action arguments]\n\n", stderr);
    fputs("Actions:\n", stderr);
    fputs(
        "- create SIZE_IN_BYTES: ensures an empty vsfs exists at the provided "
        "path\n",
        stderr);
    fputs("- tree: show the file system tree\n", stderr);
    fputs("- mkdir nested_path: ensure provided directories exist\n", stderr);
    fputs(
        "- rm vsfs_file_or_directory: ensure provided directory/file doesn't "
        "exist\n",
        stderr);
    fputs("- to vsfs_file: copies stdin to the provided file\n", stderr);
    fputs("- from vsfs_file: copies file's content to stdout\n", stderr);
    fputs(
        "- ln target vsfs_path: creates a hard link to `target` at "
        "`vsfs_path`\n",
        stderr);
    fputs("    No entry can exist at `vsfs_path`.\n", stderr);
    fputs("- ls vsfs_path: lists elements in a directory (and their sizes)\n",
          stderr);
    fputs("- truncate vsfs_file n: removes last n bytes from a file\n", stderr);
    fputs("- extend vsfs_file n: extends a file by n null bytes\n", stderr);
    fputs(
        "    if an error occurs during this operation, the file might be only "
        "partially extended\n",
        stderr);
    exit(2);
}

int main_create(int argc, char** argv) {
    if (argc != 4) usage();
    uint64_t size = strtoul(argv[3], NULL, 10);

    open_new_vsfs(argv[1], size);
    return 0;
}

int main_tree(int argc, char** argv) {
    if (argc != 3) usage();
    open_existing_vsfs(argv[1]);

    fputc('/', stdout);
    fputc('\n', stdout);
    dump_directory_content(inodes, 0);

    return 0;
}

int main_mkdir(int argc, char** argv) {
    if (argc != 4) usage();
    open_existing_vsfs(argv[1]);
    mkdir_nested(argv[3]);
    return 0;
}

int main_rm(int argc, char** argv) {
    if (argc != 4) usage();
    open_existing_vsfs(argv[1]);

    char* to_remove_name = NULL;
    inode_t* pwd = find_directory_containing(argv[3], &to_remove_name, NULL);
    directory_entry_t* to_remove_entry =
        find_entry_in_directory(pwd, to_remove_name);
    inode_t* to_remove_inode = inodes + to_remove_entry->inode_idx;

    // Clear the directory entry
    // TODO: Garbage collection of unused directory blocks?
    memset(to_remove_entry, 0, sizeof(directory_entry_t));
    deallocate_block_or_exit();

    // Unlink the inode itself
    unlink_inode(to_remove_inode);

    return 0;
}

int main_to(int argc, char** argv) {
    if (argc != 4) usage();
    open_existing_vsfs(argv[1]);

    // Find the inode for the file
    char* vsfs_filename = NULL;
    inode_t* pwd = find_directory_containing(argv[3], &vsfs_filename, NULL);
    inode_t* file = ensure_file_in_directory(pwd, vsfs_filename);

    // Copy stdin to that inode
    write_to_file(file, stdin);

    return 0;
}

int main_from(int argc, char** argv) {
    if (argc != 4) usage();
    open_existing_vsfs(argv[1]);
    inode_t* file = find_nested(argv[3]);
    write_from_file(file, stdout);
    return 0;
}

int main_ln(int argc, char** argv) {
    if (argc != 5) usage();
    open_existing_vsfs(argv[1]);
    link_inode(find_nested(argv[3]), argv[4]);
    return 0;
}

int main_ls(int argc, char** argv) {
    if (argc != 4) usage();
    open_existing_vsfs(argv[1]);

    // Copy the pathname for printing
    size_t name_len = strlen(argv[3]);
    char* path_copy = malloc(name_len + 1);
    memcpy(path_copy, argv[3], name_len);
    path_copy[name_len] = 0;

    inode_t* inode = find_nested(argv[3]);

    switch (inode->type) {
        case INODE_NIL:
            assert(0 && "Directory can't contain nil inodes");

        case INODE_FILE:
            printf("%s\t(file, %u B)\n", path_copy, inode->filesize);
            break;

        case INODE_DIRECTORY: {
            size_t* sizes = calloc(inodes_size, sizeof(size_t));
            calculate_content_size(inode, sizes);

            // Print the directory
            printf("%s\t(directory, %lu B)\n", path_copy,
                   sizes[inodes - inode]);

            // Print the content
            for (int i = 0; i < INODE_BLOCK_COUNT; ++i) {
                if (!inode->content_blocks[i]) break;

                // mmap the block
                allocate_block_or_exit(inode->content_blocks[i], true);

                // Show entries from this block
                for (size_t j = 0; j < DIR_ENTRIES_PER_BLOCK; ++j) {
                    directory_entry_t* entry =
                        ((directory_entry_t*)allocated_block) + j;
                    if (!entry->used) continue;

                    inode_t* inode_entry = inodes + entry->inode_idx;

                    switch (inode_entry->type) {
                        case INODE_FILE:
                            printf("\t%s\t(file, %u B)\n", entry->name,
                                   inode_entry->filesize);
                            break;

                        case INODE_DIRECTORY:
                            printf("\t%s\t(directory, %lu B)\n", path_copy,
                                   sizes[entry->inode_idx]);
                            break;

                        case INODE_NIL:
                            assert(0 && "Directory can't contain nil inodes");
                    }
                }
                // De-allocate the directory block
                deallocate_block_or_exit();
            }

            free(sizes);
            break;
        }
    }

    free(path_copy);
    return 0;
}

int main_du(int argc, char** argv) {
    if (argc != 3) usage();
    open_existing_vsfs(argv[1]);

    size_t used_blocks = count_used_blocks();
    size_t free_space = BLOCK_SIZE * (MAX_BLOCK_COUNT - used_blocks);
    double used_blocks_percentage =
        100.0 * (double)used_blocks / (double)MAX_BLOCK_COUNT;

    size_t used_inodes = count_used_inodes();
    double used_inodes_percentage =
        100.0 * (double)used_inodes / (double)inodes_size;

    printf("Total available bytes: %zu B\n", free_space);
    printf("Used blocks: %zu / %i (%.3f %%)\n", used_blocks, MAX_BLOCK_COUNT,
           used_blocks_percentage);
    printf("Used inodes: %zu / %u (%.3f %%)\n", used_inodes, inodes_size,
           used_inodes_percentage);
    return 0;
}

int main_truncate(int argc, char** argv) {
    if (argc != 5) usage();
    open_existing_vsfs(argv[1]);

    inode_t* file_to_truncate = find_nested(argv[3]);
    size_t truncate_by = strtoul(argv[4], NULL, 0);
    truncate_inode(file_to_truncate, truncate_by);
    return 0;
}

int main_extend(int argc, char** argv) {
    if (argc != 5) usage();
    open_existing_vsfs(argv[1]);

    inode_t* file_to_extend = find_nested(argv[3]);
    size_t extend_by = strtoul(argv[4], NULL, 0);
    extend_inode(file_to_extend, extend_by);
    return 0;
}

#define MAIN_FUNC_COUNT 11
typedef int (*main_func)(int, char**);
static struct {
    char* name;
    main_func f;
} main_jump_table[MAIN_FUNC_COUNT] = {
    {"create", main_create}, {"tree", main_tree},
    {"mkdir", main_mkdir},   {"rm", main_rm},
    {"to", main_to},         {"from", main_from},
    {"ln", main_ln},         {"ls", main_ls},
    {"du", main_du},         {"truncate", main_truncate},
    {"extend", main_extend}};

int main(int argc, char** argv) {
    // Basic argument check
    if (argc < 3) usage();
    char* action = argv[2];

    // Register the cleanup function
    int result = atexit(cleanup);
    if (result < 0) {
        perror("atexit");
        exit(1);
    }

    // Pick the action's main function
    for (int i = 0; i < MAIN_FUNC_COUNT; ++i) {
        if (strcmp(action, main_jump_table[i].name) == 0)
            return main_jump_table[i].f(argc, argv);
    }

    // No action picked - show usage
    usage();
    return 2;
}
