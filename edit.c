/*
 * File: edit.c
 * Description:
 * This file edits one tag (title, artist, album, year, comment or genre)
 * in the ID3v2 tag of an MP3 file.
 * The file is copied to "<name>.tmp" with the change, and the copy replaces
 * the original only if everything succeeded.
 */

#include "common.h"
#include "types.h"
#include "view.h"
#include "edit.h"

/*
 * Function: get_frame_details
 * Description: Converts an edit option into its ID3v2 frame ID.
 * (-t TIT2, -a TPE1, -A TALB, -y TDRC, -c COMM, -g TCON)
 * Input: option - edit option; frame_id - output buffer (5 bytes).
 * Return: e_success if the option is valid, otherwise e_failure.
 */
Status get_frame_details(char *option,char *frame_id)
{
    if(strcmp(option,"-t")==0)
        strcpy(frame_id,"TIT2");
    else if(strcmp(option,"-a")==0)
        strcpy(frame_id,"TPE1");
    else if(strcmp(option,"-A")==0)
        strcpy(frame_id,"TALB");
    else if(strcmp(option,"-y")==0)
    {
        strcpy(frame_id,"TDRC");
    }
    else if(strcmp(option,"-c")==0)
        strcpy(frame_id,"COMM");
    else if(strcmp(option,"-g")==0)
        strcpy(frame_id,"TCON");
    else
        return e_failure;

    return e_success;
}

/*
 * Function: encode_syncsafe
 * Description: Stores a number in 4 bytes using only 7 bits of each byte
 * (the ID3 "synchsafe" format).
 * Input: size - number to store; buffer - 4-byte output.
 */
void encode_syncsafe(unsigned int size,unsigned char *buffer)
{
    buffer[0]=(size>>21)&0x7F;
    buffer[1]=(size>>14)&0x7F;
    buffer[2]=(size>>7)&0x7F;
    buffer[3]=size&0x7F;
}

/*
 * Function: decode_syncsafe
 * Description: Reads a 4-byte synchsafe number.
 * Input: buffer - 4 bytes from the file.
 * Return: The decoded number.
 */
unsigned int decode_syncsafe(unsigned char *buffer)
{
    return ((unsigned int)buffer[0]<<21)|
           ((unsigned int)buffer[1]<<14)|
           ((unsigned int)buffer[2]<<7)|
           (unsigned int)buffer[3];
}

/*
 * Function: write_frame
 * Description: Writes one frame (10-byte header and text) to the temp file.
 * Input: temp_fp - temp file; frame_id - frame to write;
 *        new_value - new text; version - ID3v2 version (3 or 4).
 * Return: e_success on success, otherwise e_failure.
 */
Status write_frame(FILE *temp_fp,char *frame_id,char *new_value,int version)
{
    unsigned char frame_header[10]={0};
    unsigned int frame_size;
    unsigned char *data=NULL;

    // Version 2.3 allows only ISO-8859-1 (0); version 2.4 also allows UTF-8 (3).
    unsigned char encoding=(version==3)?0:3;

    if(strcmp(frame_id,"COMM")==0)
    {
        // Extra 5 bytes: encoding, language, and an empty description.
        frame_size=strlen(new_value)+5;
        data=malloc(frame_size);

        if(data==NULL)
            return e_failure;

        data[0]=encoding;
        data[1]='e';
        data[2]='n';
        data[3]='g';
        data[4]=0;

        strcpy((char *)&data[5],new_value);
    }
    else
    {
        // Extra 1 byte: encoding.
        frame_size=strlen(new_value)+1;
        data=malloc(frame_size);

        if(data==NULL)
            return e_failure;

        data[0]=encoding;
        strcpy((char *)&data[1],new_value);
    }

    memcpy(frame_header,frame_id,4);

    // Version 2.3 uses a normal size; version 2.4 uses a synchsafe size.
    if(version==3)
    {
        frame_header[4]=(frame_size>>24)&0xFF;
        frame_header[5]=(frame_size>>16)&0xFF;
        frame_header[6]=(frame_size>>8)&0xFF;
        frame_header[7]=frame_size&0xFF;
    }
    else
    {
        encode_syncsafe(frame_size,&frame_header[4]);
    }

    if(fwrite(frame_header,1,10,temp_fp)!=10 ||
       fwrite(data,1,frame_size,temp_fp)!=frame_size)
    {
        free(data);
        return e_failure;
    }

    free(data);

    return e_success;
}

/*
 * Function: edit_id3v2
 * Description: Copies the ID3v2 tag to the temp file with one frame
 * replaced (or added if it is missing), then copies the audio data.
 * Input: fp - original file; temp_fp - temp file;
 *        frame_id - frame to change; new_value - new text.
 * Return: e_success on success, otherwise e_failure.
 */
Status edit_id3v2(FILE *fp,FILE *temp_fp,char *frame_id,char *new_value)
{
    unsigned char header[10];
    unsigned int tag_size;
    unsigned int processed=0;
    unsigned int new_tag_size=0;
    int version;
    int updated=0;

    if(fseek(fp,0,SEEK_SET)!=0)
        return e_failure;

    if(fread(header,1,10,fp)!=10)
        return e_failure;

    version=header[3];

    tag_size=decode_syncsafe(&header[6]);

    // The year frame is TYER in version 2.3 and TDRC in version 2.4.
    if(version==3 && strcmp(frame_id,"TDRC")==0)
        strcpy(frame_id,"TYER");

    // Skip the extended header (if any) and remove it from the new tag.
    if(header[5]&0x40)
    {
        unsigned char ext[4];
        unsigned int ext_size;

        if(fread(ext,1,4,fp)!=4)
            return e_failure;

        if(version==3)
        {
            // Version 2.3 size does not include these 4 bytes.
            ext_size=((unsigned int)ext[0]<<24)|
                     ((unsigned int)ext[1]<<16)|
                     ((unsigned int)ext[2]<<8)|
                     (unsigned int)ext[3];

            if(ext_size>tag_size)
                return e_failure;

            ext_size+=4;
        }
        else
        {
            // Version 2.4 size includes these 4 bytes.
            ext_size=decode_syncsafe(ext);
        }

        if(ext_size<4 || ext_size>tag_size)
            return e_failure;

        if(fseek(fp,ext_size-4,SEEK_CUR)!=0)
            return e_failure;

        processed=ext_size;
        header[5]&=~0x40;
    }

    // Copy the header now; the tag size is updated at the end.
    if(fwrite(header,1,10,temp_fp)!=10)
        return e_failure;

    while(processed+10<=tag_size)
    {
        unsigned char frame[10];

        if(fread(frame,1,10,fp)!=10)
            return e_failure;

        // Padding reached, no more frames.
        if(frame[0]==0)
            break;

        unsigned int size;

        if(version==3)
        {
            size=((unsigned int)frame[4]<<24)|
                 ((unsigned int)frame[5]<<16)|
                 ((unsigned int)frame[6]<<8)|
                 (unsigned int)frame[7];
        }
        else
        {
            size=decode_syncsafe(&frame[4]);
        }

        processed+=10;

        // The frame must fit inside the tag.
        if(size>tag_size-processed)
            return e_failure;

        unsigned char *data=malloc(size);

        if(data==NULL && size>0)
            return e_failure;

        if(size>0 && fread(data,1,size,fp)!=size)
        {
            free(data);
            return e_failure;
        }

        if(memcmp(frame,frame_id,4)==0 && updated==0)
        {
            // This is the frame to change: write the new one instead.
            if(write_frame(temp_fp,frame_id,new_value,version)==e_failure)
            {
                free(data);
                return e_failure;
            }

            if(strcmp(frame_id,"COMM")==0)
                new_tag_size+=10+strlen(new_value)+5;
            else
                new_tag_size+=10+strlen(new_value)+1;

            updated=1;
        }
        else
        {
            // Other frames are copied unchanged.
            if(fwrite(frame,1,10,temp_fp)!=10 ||
               (size>0 && fwrite(data,1,size,temp_fp)!=size))
            {
                free(data);
                return e_failure;
            }

            new_tag_size+=10+size;
        }

        free(data);
        processed+=size;
    }

    // The frame was not found, so add it at the end.
    if(updated==0)
    {
        if(write_frame(temp_fp,frame_id,new_value,version)==e_failure)
            return e_failure;

        if(strcmp(frame_id,"COMM")==0)
            new_tag_size+=10+strlen(new_value)+5;
        else
            new_tag_size+=10+strlen(new_value)+1;
    }

    // Write the new tag size into the header (bytes 6 to 9).
    if(fseek(temp_fp,6,SEEK_SET)!=0)
        return e_failure;

    unsigned char size_buffer[4];

    encode_syncsafe(new_tag_size,size_buffer);

    if(fwrite(size_buffer,1,4,temp_fp)!=4)
        return e_failure;

    if(fseek(temp_fp,0,SEEK_END)!=0)
        return e_failure;

    // Skip the old tag in the original file and copy the rest.
    if(fseek(fp,10+tag_size,SEEK_SET)!=0)
        return e_failure;

    unsigned char buffer[1024];
    size_t bytes;

    while((bytes=fread(buffer,1,sizeof(buffer),fp))>0)
    {
        if(fwrite(buffer,1,bytes,temp_fp)!=bytes)
            return e_failure;
    }

    return e_success;
}

/*
 * Function: edit_tags
 * Description: Edits one tag of an MP3 file using a temporary file.
 * Only ID3v2 tags are supported.
 * Input: filename - MP3 file; option - edit option; new_value - new text;
 *        argv - command-line arguments (used to show the current tags).
 */
void edit_tags(char *filename,char *option,char *new_value,char *argv[])
{
    FILE *fp=NULL;
    FILE *temp_fp=NULL;
    char *temp_filename=NULL;
    char frame_id[5]={0};

    // Show the current tags first.
    if(view_tags(argv,1)==e_failure)
    {
        printf("Not able to get the data to edit\n");
        return;
    }

    if(get_frame_details(option,frame_id)==e_failure)
    {
        printf("Invalid edit option\n");
        return;
    }

    // Temp file name is the original name plus ".tmp".
    temp_filename=malloc(strlen(filename)+5);

    if(temp_filename==NULL)
    {
        printf("Memory allocation failed\n");
        return;
    }

    sprintf(temp_filename,"%s.tmp",filename);

    fp=fopen(filename,"rb");

    if(fp==NULL)
    {
        printf("Unable to open original file\n");
        goto cleanup;
    }

    temp_fp=fopen(temp_filename,"wb");

    if(temp_fp==NULL)
    {
        printf("Unable to create temporary file\n");
        goto cleanup;
    }

    unsigned char header[3];

    if(fread(header,1,3,fp)!=3)
    {
        printf("Error reading MP3 file\n");
        goto cleanup;
    }

    // Check for the "ID3" marker at the start of the file.
    if(memcmp(header,ID3V2_TAG,3)==0)
    {
        if(edit_id3v2(fp,temp_fp,frame_id,new_value)==e_failure)
        {
            printf("Unable to edit ID3v2 tag\n");
            goto cleanup;
        }
    }
    else
    {
        printf("ID3v2 tag not found. ID3v1 editing is not implemented yet\n");
        goto cleanup;
    }

    // Close both files before renaming.
    fclose(fp);
    fp=NULL;

    fclose(temp_fp);
    temp_fp=NULL;

    // Replace the original file with the edited copy.
    if(rename(temp_filename,filename)!=0)
    {
        printf("Unable to rename temporary file\n");
        goto cleanup;
    }

    printf("Tag updated successfully\n");

    cleanup:

    if(fp!=NULL)
        fclose(fp);

    if(temp_fp!=NULL)
        fclose(temp_fp);

    if(temp_filename!=NULL)
    {
        // Removes the temp file if it is still there (after an error).
        remove(temp_filename);
        free(temp_filename);
    }
}
