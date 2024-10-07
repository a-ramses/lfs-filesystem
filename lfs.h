#include<fuse.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<stdbool.h>
#include<string.h>
#include<time.h>
#include<errno.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX_NAME_LENGTH 128
#define MAX_FILE_SIZE 32768

typedef struct lfs_table lfs_table;
typedef struct lfs_entry lfs_entry;

static lfs_table *table;

int lfs_init_entry(const char*, mode_t, int, char*);
void lfs_new_table();
int lfs_get_path(const char*, char*, char*);
int lfs_generate_unique_id();
lfs_entry* lfs_get_entry(const char*);
lfs_entry* lfs_get_parent_table(const char*);
int lfs_rm_entry(lfs_entry*);
int lfs_write_to_disk(FILE*);
int lfs_load_data_from_disk(FILE*);

// Represents file in file system
struct lfs_entry {
    char path[256];
    char file_name[MAX_NAME_LENGTH];
    char data[MAX_FILE_SIZE];
    int entry_id;
    bool isFile;
    uid_t user_id;
    off_t length;
    __time_t atime;
    __time_t mtime;
};

// Holds all entries in file system
struct lfs_table {
    lfs_entry all_entries[128][128];
    int num_entries;
};

// Root dir
void lfs_new_table() {
    table = calloc(1, sizeof(lfs_table));
    lfs_init_entry("/", 0, 0, NULL);
}

// Makes new entry to file system
int lfs_init_entry(const char *path, mode_t mode, int id, char *data){
    lfs_entry *entry = calloc(1, sizeof(lfs_entry));
    // Insert data if present and set size accordingly
    if(data) {
        if(strlen(data) > MAX_FILE_SIZE) {
            memcpy(entry->data, data, MAX_FILE_SIZE);
            entry->length = MAX_FILE_SIZE;
        } else {
            memcpy(entry->data, data, strlen(data));
            entry->length = strlen(data);
        }
    }
    else
        entry->length = 0;

    // Set id and path
    entry->entry_id = id;
    strcpy(entry->path, path);

    // Set name of entry
    char *name = calloc(1, strlen(path));
    char *temp = calloc(1, strlen(path));
    lfs_get_path(path, name, temp);
    memcpy(entry->file_name, name, strlen(name));
    free(temp);
    free(name);

    // Set time, user_id and type (file or dir)
    entry->atime = time(NULL);
    entry->mtime = time(NULL);
    entry->user_id = getuid();
    entry->isFile = (mode == 1);

    // Insert entry to all_entries and increment num_entries
    memcpy(table->all_entries[table->num_entries], entry, sizeof(lfs_entry));
    table->num_entries++;

    return 1; // return 1 to signal success
}

// Loads filename and parent_dirs into respective char pointers
int lfs_get_path(const char *path, char *filename, char *parent_dirs) {
    int i = MIN(strlen(path), 256);
    int index = i - 1;
    while(index >= 0 && path[index] != '/') { // Finding first '/' from the right
        --index;
    }
    index += 1; // First char of child (/home/1.txt) -> "1.txt"
    memcpy(filename, path+index, i-index);
    memcpy(parent_dirs, path, index-1);
    if(strlen(parent_dirs) == 0) {
            strcpy(parent_dirs,"/");
    }
    return(i-index); // Length of filename
}

// Generates and returns a unique id for a new entry
int lfs_generate_unique_id(){
    int id = 0;
    int found = 0;
    int taken;
    while(id < 128 && !found){
        taken = 0;
        for(int i = 0; i < table->num_entries; ++i) {
            if(id == table->all_entries[i]->entry_id)
                taken = 1;
        }
        if(!taken)
            found = 1;
        else
            id++;
    }
    if(found)
        return id;
    else
        return -1;
}

// Finds entry from path
lfs_entry* lfs_get_entry(const char *path) {
    lfs_entry *entry = NULL; // Signals not found
    for(int i = 0; i < table->num_entries; i++) {
        if(strcmp(table->all_entries[i]->path, path) == 0) // Found
            entry = table->all_entries[i];
    }
    printf("Not Found");
    return entry;
}

lfs_entry* lfs_get_parent_table(const char *path) {
    char *filename = calloc(1, strlen(path));
    char *parent_dirs = calloc(1, strlen(path));
    lfs_get_path(path, filename, parent_dirs);
    lfs_entry *parent = lfs_get_entry(parent_dirs);

    free(filename);
    free(parent_dirs);

    return parent;
}

int lfs_rm_entry(lfs_entry *entry){
    int index = 0;
    // Find index of file
    while(table->all_entries[index]->entry_id != entry->entry_id && index < table->num_entries){
        ++index;
    }

    // Reset all place entry was
    memset(entry->data, 0, entry->length);
    memcpy(table->all_entries[index], table->all_entries[table->num_entries-1], sizeof(lfs_entry));
    memset(table->all_entries[table->num_entries-1], 0, sizeof(lfs_entry));

    // Decrement number of entries
    table->num_entries--;

    return 0;
}

int lfs_write_data_to_disk(FILE *disk) {
    int total_written = 0;
    fseek(disk, 0L, SEEK_SET);
    fwrite(&table->num_entries, sizeof(int), 1, disk);

    for(int i = 1; i < table->num_entries; i++) { // Write entries one by one
        total_written += fwrite(table->all_entries[i], sizeof(lfs_entry), 1, disk);
    }
    // Close disk & return total bytes written
    fclose(disk);
    return total_written;
}

int lfs_load_data_from_disk(FILE *disk) {
    int total_read = 0;
    fseek(disk, 0L, SEEK_END);
    if(ftell(disk) == 0)
        return 0;

    rewind(disk); // Rewinding disk

    if (fread(&table->num_entries, sizeof(int), 1, disk) == 0)
        return -EIO; // IO error code

    for(int i = 1; i < table->num_entries; i++) { // Read entries one by one
        total_read += fread(table->all_entries[i], sizeof(lfs_entry), 1, disk);
    }
    // Close disk & return total bytes read
    fclose(disk);
    return total_read;
}
