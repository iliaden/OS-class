#define MAX_FILENAME_LENGTH 12

void mksfs( int fresh );
void sfs_ls();
int sfs_fopen( char * name );
void sfs_fclose( int fileID );
void sfs_fwrite( int fileID, char * buf, int length );
void sfs_fread( int fileID, char * buf, int length );
void sfs_fseek( int fileID, int loc );
int sfs_remove( char * file );
int find_free();
int find_next_fat();
void create_empty_table();
int find_last_disk( int row );


typedef struct {
    char filename [MAX_FILENAME_LENGTH + 1];
    unsigned int first_row;
    unsigned int size;
    //attributes
    int32_t created_time;
    int32_t last_access_time;
    int32_t last_modified_time;
} inode;

typedef struct {
    unsigned int disk_block; //0 -> free
    unsigned int next_entry;
} fat_entry;
