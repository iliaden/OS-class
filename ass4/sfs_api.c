#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "sfs_api.h"
#include "disk_emu.h"


#define DISK_LOCATION "/tmp/my_disk"
#define SECTOR_SIZE 1024 //bytes
#define SECTOR_COUNT 4096 //total disk size = 4Mb
#define MAX_FILE_COUNT 100
#define DEBUG 0

//resident structures
inode * root;
fat_entry * fat_table;
int fat_location;
int root_location = 0; //always after sector size and sector count
int file_descriptors[2][MAX_FILE_COUNT]; //read, write; indicating offset

unsigned char * empty_sectors; //1 = free; 0 = occupied

int find_free()
{
    int ii = 0 ;

    while ( ++ii < SECTOR_COUNT )
        if ( empty_sectors[ii] == 1 )
        { return ii; }

    return -1;
}
int find_next_fat()
{
    int ii = 0;

    while ( fat_table[ii].disk_block != 0 )
    { ii++; }

    return ii;
}


void create_empty_table()
{
    //create list of sectors. mark as used from the info stored in the FAT
    empty_sectors = ( unsigned char * )malloc( sizeof( unsigned char ) * SECTOR_COUNT );
    int ii;

    for ( ii = 0; ii < SECTOR_COUNT; ii++ )
    { empty_sectors[ii] = 1; }

    //root and FAT are busy
    //root
    int root_size = MAX_FILE_COUNT * 32;
    int sectors_taken = ( root_size / SECTOR_SIZE );

    if ( root_size % SECTOR_SIZE > 0 )
    { sectors_taken++; }

    //FAT
    int fat_size = ( 8 * SECTOR_COUNT ) / SECTOR_SIZE; //32 sectors

    if ( ( 8 * SECTOR_COUNT ) % SECTOR_SIZE > 0 )
    { fat_size++; }

    for ( ii = 0; ii < ( sectors_taken + fat_size ); ii++ )
    { empty_sectors[ii] = 0; }

    //go through FAT, removing the busy sectors
    for ( ii = 0; ii < SECTOR_COUNT; ii ++ ) {
        //read row for sector
        fat_entry row = fat_table[ii];

        if ( row.disk_block != 0 ) {
            empty_sectors[row.disk_block] = 0;
        }
    }
}

void mksfs( int fresh )
{
    int root_size = MAX_FILE_COUNT * sizeof( inode );
    int sectors_taken = ( root_size / SECTOR_SIZE );

    if ( root_size % SECTOR_SIZE > 0 )
    { sectors_taken++; }

    int fat_size = sizeof( fat_entry ) * SECTOR_COUNT;
    int fat_sectors = fat_size / SECTOR_SIZE; //32 sectors

    if ( ( sizeof( fat_entry ) * SECTOR_COUNT ) % SECTOR_SIZE > 0 )
    { fat_sectors++; }

    if ( fresh ) {
        init_fresh_disk( DISK_LOCATION, SECTOR_SIZE, SECTOR_COUNT );
        //create root and FAT.
        root = ( inode * ) calloc( MAX_FILE_COUNT, sizeof( inode ) );
        fat_table = ( fat_entry * ) calloc( SECTOR_COUNT, sizeof( fat_entry ) );
        //write them to DISK.
        memset( root, 0, MAX_FILE_COUNT * sizeof( inode ) );
        memset( fat_table, 0, SECTOR_COUNT * sizeof( fat_entry ) );
        write_blocks( 0, sectors_taken, ( void * ) root );
        write_blocks( sectors_taken, fat_sectors, ( void * ) fat_table );
        create_empty_table();
        /*hack: create a file, then delete it*/
        int id = sfs_fopen( "AAAAAAAA.AAA" );
        char * data = calloc( 50000, sizeof( char ) );
        sfs_fwrite( id, data, 50000 );
        sfs_fclose( id );
        sfs_remove( "AAAAAAAA.AAA" );
    } else {
        init_disk( DISK_LOCATION, SECTOR_SIZE, SECTOR_COUNT );
        //read root and FAT into RAM
        root =  ( inode * ) malloc ( sectors_taken * SECTOR_SIZE );
        read_blocks( 0, sectors_taken, root );
        fat_table = ( fat_entry * ) malloc( fat_sectors * SECTOR_COUNT );
        read_blocks( sectors_taken, fat_sectors, fat_table );
    }

    create_empty_table();
}
void sfs_ls()
{
    //go through root, print all
    int ii;
    printf( "%12s\t%13s\t%13s\n", "filename", "size", "created_time" );

    for ( ii = 0 ; ii < MAX_FILE_COUNT ; ii++ ) {
        if ( strlen( root[ii].filename ) != 0 ) {
            printf( "%12s\t%13d\t%13d\n", root[ii].filename, root[ii].size, root[ii].created_time );
        }
    }
}
void write_structs()
{
    int root_size = MAX_FILE_COUNT * sizeof( inode );
    int sectors_taken = ( root_size / SECTOR_SIZE );

    if ( root_size % SECTOR_SIZE > 0 )
    { sectors_taken++; }

    int fat_size = sizeof( fat_entry ) * SECTOR_COUNT;
    int fat_sectors = fat_size / SECTOR_SIZE; //32 sectors

    if ( ( sizeof( fat_entry ) * SECTOR_COUNT ) % SECTOR_SIZE > 0 )
    { fat_sectors++; }

    write_blocks( 0, sectors_taken, ( void * ) root );
    write_blocks( sectors_taken, fat_sectors, ( void * ) fat_table );
    free( empty_sectors );
    create_empty_table();
}

int exists( char * name )
{
    int ii;

    for ( ii = 0; ii < MAX_FILE_COUNT; ii++ ) {
        if ( root[ii].filename != NULL ) {
            int same = 1;
            int jj;

            for ( jj = 0; jj < 12; jj++ )
                if ( root[ii].filename[jj] != name[jj] )
                { same = 0; }

            if ( same )
            { return ii; }
        }
    }

    return -1;
}
int sfs_fopen( char * name )
{
    //verify if file is in root
    int index = exists( name );
//  printf("exists : %s\t%d\n",name,index);
    int ii;

    if ( index == -1 ) {
        inode * curr = ( inode * ) malloc( sizeof( inode ) );
        strcpy( curr->filename, name );
        curr->filename[MAX_FILENAME_LENGTH + 1] = '\0';
        curr->first_row = 0;
        curr->size = 0;
        //attributes
        curr->created_time = time( NULL );
        curr->last_access_time = time( NULL );
        curr->last_modified_time = time ( NULL );

        for ( ii = 0; ii < MAX_FILE_COUNT; ii++ ) {
            if ( strlen( root[ii].filename ) == 0 )
            { break; }
        }

        if ( ii < MAX_FILE_COUNT )
        { root[ii] = *curr; }
        else {
            //error!
            return -1;
        }

        index = ii;
    }

    //create pointers
    file_descriptors[0][index]  = 0;
    file_descriptors[1][index]  = root[index].size;
    write_structs();

    if ( DEBUG )
    { printf( "file :%s \t index: %d\n", name, ii ); }

    return index;
}
void sfs_fclose( int fileID )
{
    file_descriptors[0][fileID] = 0;
    file_descriptors[1][fileID] = 0;
}

int find_last_disk( int row )
{
    while ( fat_table[row].next_entry != 0 )
    { row = fat_table[row].next_entry; }

    return fat_table[row].disk_block;
}

void sfs_fwrite( int index, char * buf, int length )
{
    //offset is needed in case seek() happened
    if ( DEBUG )
    { printf( "index: %d\tsize: %d\t writing: %d\n", index, root[index].size, length ); }

    int offset = file_descriptors[1][index];

    if ( root[index].first_row == 0 ) {
        //assign FAT row
        int fat_free = find_next_fat();
        root[index].first_row = fat_free;
        //assign disk space
        int free_sector = find_free();
        fat_table[fat_free].disk_block = free_sector;
        //write
        int min = ( length > SECTOR_SIZE ) ? SECTOR_SIZE : length ;
        void * data = malloc( SECTOR_SIZE );
        memcpy( data, buf, min );
        write_blocks( free_sector, 1, data );
        free( data );
        root[index].size += min;
        root[index].last_access_time = time( NULL );
        root[index].last_modified_time = time( NULL );
        file_descriptors[1][index] += min;

        if ( length > min ) {
            create_empty_table();
            sfs_fwrite( index, buf + min, length - min );
        } else {
            write_structs();
        }

        return;
    } else if ( offset % SECTOR_SIZE != 0 ) {
        if ( DEBUG )
        { printf( "reading... curr.disk_block [%i]\n", find_last_disk( root[index].first_row ) ); }

        //append to current sector
        int remaining = SECTOR_SIZE - ( offset % SECTOR_SIZE );
        char * buffer = malloc( SECTOR_SIZE );
        read_blocks( find_last_disk( root[index].first_row ) , 1, buffer );
        //buffer is now filled with current contents
        //we append the rest to it, and save.
        void * final = malloc( SECTOR_SIZE );
        memcpy( final, buffer, ( offset % SECTOR_SIZE ) );
        int min = ( length > remaining ) ? remaining : length;
        memcpy( final + ( offset % SECTOR_SIZE ), buf, min );
        write_blocks( find_last_disk( root[index].first_row ), 1, final );
        root[index].size += min;
        root[index].last_access_time = time( NULL );
        root[index].last_modified_time = time( NULL );
        file_descriptors[1][index] += min;
        free( buffer );
        free( final );

        if ( length > remaining ) {
            create_empty_table();
            sfs_fwrite( index, buf + min, length - min );
        } else {
            write_structs();
        }

        return;
    } else {
        //find next free sector
        int free_slot = find_free();
        int fat_row = find_next_fat();
        int row = root[index].first_row;

        while ( fat_table[row].next_entry != 0 )
        { row = fat_table[row].next_entry; }

        fat_table[row].next_entry = fat_row;
        fat_table[fat_row].disk_block = free_slot;
        int min = ( length > SECTOR_SIZE ) ? SECTOR_SIZE : length;
        void * data = malloc( SECTOR_SIZE );
        memcpy ( data, buf, min );
        write_blocks( free_slot, 1, data ); //TODO: do I need to add padding?
        free( data );
        root[index].size += min;
        root[index].last_access_time = time( NULL );
        root[index].last_modified_time = time( NULL );
        file_descriptors[1][index] += min;

        if ( length > SECTOR_SIZE ) {
            //if we still need to write more
            create_empty_table();
            sfs_fwrite( index, buf + min, length - min );
        } else {
            write_structs();
        }
    }
}
void sfs_fread( int fileID, char * buf, int length )
{
    //assume no offset now...
    int read = 0;
    int size = root[fileID].size;
    int offset = file_descriptors[0][fileID];
    int currblock = root[fileID].first_row;

    if ( DEBUG )
    { printf( "expecting last byte to be at position %d. offset = [%d]\n", offset + length, offset ); }

    void * my_buf = malloc( SECTOR_SIZE );

    while ( size > 0 && length > 0 ) {
        int min = ( size > length ) ? length : size;

        if ( min > SECTOR_SIZE )
        { min = SECTOR_SIZE; }

        read_blocks( fat_table[currblock].disk_block, 1, my_buf );

        if ( read - offset < 0 && read - offset + SECTOR_SIZE < 0 ) {
            read += SECTOR_SIZE;
            size -= SECTOR_SIZE;
        } else if ( read - offset < 0 ) {
            if ( DEBUG )
            { printf( "starting option 2: read [%d] offset [%d] length [%d]\n", read, offset, length ); }

            //find out where to copy from...
            int start = offset - read;
            min = ( ( SECTOR_SIZE - start ) > length ) ? length : ( SECTOR_SIZE - start ) ;

            if ( DEBUG )
            { printf( "computed: min [%d] ; start [%d]\n", min, start ); }

            memcpy ( buf , my_buf + start, min );
            read += start;
            length -= min;
            size -= min;
            read += min;

            if ( DEBUG )
            { printf( "wrote %d bytes into buf; remaining [%d] \n", min, length ); }

            file_descriptors[0][fileID] += min;
        } else {
            if ( DEBUG )
            { printf( "option3: read [%d] offset [%d]\n", read, offset ); }

            memcpy( buf + read - offset, my_buf, min );
            length -= min;
            size -= min;
            read += min;

            if ( DEBUG )
            { printf( "wrote %d bytes into buf; remaining [%d] \n", min, length ); }

            file_descriptors[0][fileID] += min;
        }

        if ( DEBUG )
        { printf( "read: block [%d] offset [%d] min [%d]\n", fat_table[currblock].disk_block , offset, min ); }

        currblock = fat_table[currblock].next_entry;
    }

    free( my_buf );
}
void sfs_fseek( int fileID, int loc )
{
    file_descriptors[0][fileID] = loc;
    file_descriptors[1][fileID] = loc;
}
int sfs_remove( char * file )
{
    int index = exists( file );

    if ( index == -1 )
    { return -1; }

    int curr_fat = root[index].first_row;
    int ii;

    for ( ii = 0; ii < 12; ii++ )
    { root[index].filename[ii] = '\0'; }

    //go thtough fat table...
    while ( curr_fat != 0 ) {
        int next_fat = fat_table[curr_fat].next_entry;
        fat_table[curr_fat].next_entry = 0;
        fat_table[curr_fat].disk_block = 0;
        curr_fat = next_fat;
    }

    return 0;
}


