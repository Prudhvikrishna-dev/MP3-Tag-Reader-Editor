/*
 * File: view.c
 * Description:
 * This file reads and displays metadata stored in MP3 files.
 * It supports ID3v2.3, ID3v2.4, and ID3v1 tags, converts supported
 * ID3v2 text encodings into readable text, and selects the tag format
 * available in the input file.
 */

#include "common.h"
#include "types.h"
#include "view.h"

/*
 * Function: copy_frame_text
 * Description: Extracts text from an ID3v2 frame and converts supported
 * encodings into a null-terminated UTF-8 string.
 * Input: destination - address of the output string pointer;
 *        data - frame data; frame_size - frame data size;
 *        is_comment - indicates whether the frame is COMM.
 * Return: e_success on success, otherwise e_failure.
 */
Status copy_frame_text(char **destination, unsigned char *data, unsigned int frame_size, int is_comment)
{
    // Reject frames that do not contain enough data for a text encoding and value.
    if(frame_size<=1)
        return e_failure;

    unsigned int i=1;   // read position (data[0] is the encoding byte)
    unsigned int j=0;   // write position in the output
    size_t destination_size=(frame_size*2)+1;

    char *temp=malloc(destination_size);

    if(temp==NULL)
    {
        printf("Memory allocation failed\n");
        return e_failure;
    }

    // Encoding 0 (ISO-8859-1) or 3 (UTF-8): copy the bytes as they are.
    if(data[0]==0 || data[0]==3)
    {
        if(is_comment)
        {
            // Skip the language code and the description.
            i=4;

            while(i<frame_size && data[i]!='\0')
                i++;

            if(i<frame_size)
                i++;
        }

        while(i<frame_size && data[i]!='\0' && j<destination_size-1)
        {
            temp[j]=data[i];
            i++;
            j++;
        }
    }
    // Encoding 1 or 2 (UTF-16): read two bytes at a time and convert to UTF-8.
    else if(data[0]==1 || data[0]==2)
    {
        int little_endian=0;

        if(is_comment)
        {
            // Skip the language code.
            i=4;

            // Encoding 1 starts with a byte order mark (BOM).
            if(data[0]==1)
            {
                if(i+1<frame_size && data[i]==0xFF && data[i+1]==0xFE)
                {
                    little_endian=1;
                    i+=2;
                }
                else if(i+1<frame_size && data[i]==0xFE && data[i+1]==0xFF)
                {
                    i+=2;
                }
                else
                {
                    free(temp);
                    return e_failure;
                }
            }

            // Skip the description.
            while(i+1<frame_size && (data[i]!=0 || data[i+1]!=0))
                i+=2;

            if(i+1<frame_size)
                i+=2;
        }
        else if(data[0]==1)
        {
            // Read the byte order mark (BOM).
            if(i+1<frame_size && data[i]==0xFF && data[i+1]==0xFE)
            {
                little_endian=1;
                i+=2;
            }
            else if(i+1<frame_size && data[i]==0xFE && data[i+1]==0xFF)
            {
                i+=2;
            }
            else
            {
                free(temp);
                return e_failure;
            }
        }

        while(i+1<frame_size && j<destination_size-1)
        {
            unsigned int value;

            // Two zero bytes end the text.
            if(data[i]==0 && data[i+1]==0)
                break;

            if(little_endian)
                value=data[i]|((unsigned int)data[i+1]<<8);
            else
                value=((unsigned int)data[i]<<8)|data[i+1];

            // Convert the character to 1, 2 or 3 UTF-8 bytes.
            if(value<0x80)
            {
                temp[j++]=value;
            }
            else if(value<0x800)
            {
                if(j+2>destination_size-1)
                    break;

                temp[j++]=0xC0|(value>>6);
                temp[j++]=0x80|(value&0x3F);
            }
            else if(value<0xD800 || value>0xDFFF)
            {
                if(j+3>destination_size-1)
                    break;

                temp[j++]=0xE0|(value>>12);
                temp[j++]=0x80|((value>>6)&0x3F);
                temp[j++]=0x80|(value&0x3F);
            }
            else
            {
                // Surrogate pairs are not supported.
                break;
            }

            i+=2;
        }
    }
    else
    {
        // Unknown encoding.
        free(temp);
        return e_failure;
    }

    temp[j]='\0';

    // Copy to a string of the exact size.
    char *result=malloc(j+1);

    if(result==NULL)
    {
        free(temp);
        printf("Memory allocation failed\n");
        return e_failure;
    }

    strcpy(result,temp);

    free(temp);

    // Replace the old value, if any.
    free(*destination);

    *destination=result;

    return e_success;
}

/*
 * Function: read_id3v2
 * Description: Reads the ID3v2 header and frames, extracts supported
 * metadata fields, and displays their values.
 * Input: fp - file pointer to the MP3 file.
 * Return: e_success on success, otherwise e_failure.
 */
Status read_id3v2(FILE *fp)
{
    unsigned char buffer[10];

    if(fseek(fp,0,SEEK_SET)!=0)
    {
        printf("Unable to seek to ID3v2 header\n");
        return e_failure;
    }

    if(fread(buffer,1,10,fp)!=10)
    {
        printf("Error reading ID3v2 header\n");
        return e_failure;
    }

    // The supported ID3 version determines how frame sizes are decoded.
    if(buffer[3]!=3 && buffer[3]!=4)
    {
        printf("This function currently supports ID3v2.3 and ID3v2.4 only\n");
        return e_failure;
    }

    // The ID3 tag size occupies header bytes 6 through 9 in synchsafe format.
    unsigned int tag_size=0;
    int i=6;

    while(i<10)
    {
        tag_size=(tag_size<<7)|(buffer[i]&0x7F);
        i++;
    }

    unsigned int processed=0;

    // Skip the extended header (if any) so it is not read as a frame.
    if(buffer[5]&0x40)
    {
        unsigned char ext[4];
        unsigned int ext_size;

        if(fread(ext,1,4,fp)!=4)
        {
            printf("Error reading extended header\n");
            return e_failure;
        }

        if(buffer[3]==3)
        {
            // Version 2.3 size does not include these 4 bytes.
            ext_size=((unsigned int)ext[0]<<24)|
                     ((unsigned int)ext[1]<<16)|
                     ((unsigned int)ext[2]<<8)|
                     (unsigned int)ext[3];

            if(ext_size>tag_size)
            {
                printf("Invalid extended header size\n");
                return e_failure;
            }

            ext_size+=4;
        }
        else
        {
            // Version 2.4 size is synchsafe and includes these 4 bytes.
            ext_size=((unsigned int)(ext[0]&0x7F)<<21)|
                     ((unsigned int)(ext[1]&0x7F)<<14)|
                     ((unsigned int)(ext[2]&0x7F)<<7)|
                     (unsigned int)(ext[3]&0x7F);
        }

        if(ext_size<4 || ext_size>tag_size)
        {
            printf("Invalid extended header size\n");
            return e_failure;
        }

        if(fseek(fp,ext_size-4,SEEK_CUR)!=0)
        {
            printf("Unable to skip extended header\n");
            return e_failure;
        }

        processed=ext_size;
    }

    while(processed+10<=tag_size)
    {
        unsigned char frame[10];

        if(fread(frame,1,10,fp)!=10)
        {
            printf("Error reading frame header\n");
            return e_failure;
        }

        // Padding reached, no more frames.
        if(frame[0]==0)
        {
            break;
        }

        unsigned int frame_size;

        // ID3v2.3 uses a big-endian frame size; ID3v2.4 uses synchsafe bytes.
        if(buffer[3]==3)
        {
            frame_size=((unsigned int)frame[4]<<24)|
                       ((unsigned int)frame[5]<<16)|
                       ((unsigned int)frame[6]<<8)|
                       (unsigned int)frame[7];
        }
        else
        {
            frame_size=((unsigned int)(frame[4]&0x7F)<<21)|
                       ((unsigned int)(frame[5]&0x7F)<<14)|
                       ((unsigned int)(frame[6]&0x7F)<<7)|
                       (unsigned int)(frame[7]&0x7F);
        }

        processed+=10;

        // Ensure the frame data fits inside the remaining declared tag area.
        if(frame_size>tag_size-processed)
        {
            printf("Invalid frame size\n");
            return e_failure;
        }

        unsigned char *data=malloc(frame_size);

        if(data==NULL && frame_size>0)
        {
            printf("Memory allocation failed\n");
            return e_failure;
        }

        if(frame_size>0 && fread(data,1,frame_size,fp)!=frame_size)
        {
            free(data);
            printf("Error reading frame data\n");
            return e_failure;
        }

        printf("Frame: %.4s, Size: %u\n",frame,frame_size);

        if(frame_size>0)
        {
            // Identify the frame and store its text in the matching metadata field.
            if(memcmp(frame,"TIT2",4)==0)
            {
                if(copy_frame_text(&tag.title,data,frame_size,0)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
            else if(memcmp(frame,"TPE1",4)==0)
            {
                if(copy_frame_text(&tag.artist,data,frame_size,0)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
            else if(memcmp(frame,"TALB",4)==0)
            {
                if(copy_frame_text(&tag.album,data,frame_size,0)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
            else if(memcmp(frame,"TYER",4)==0 || memcmp(frame,"TDRC",4)==0)
            {
                if(copy_frame_text(&tag.year,data,frame_size,0)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
            else if(memcmp(frame,"TCON",4)==0)
            {
                if(copy_frame_text(&tag.genre1,data,frame_size,0)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
            else if(memcmp(frame,"COMM",4)==0)
            {
                if(copy_frame_text(&tag.comment,data,frame_size,1)==e_failure)
                {
                    free(data);
                    return e_failure;
                }
            }
        }

        free(data);
        processed+=frame_size;
    }

    // Missing fields are shown as empty.
    printf("Title: %s\n",tag.title==NULL ? "" : tag.title);
    printf("Artist: %s\n",tag.artist==NULL ? "" : tag.artist);
    printf("Album: %s\n",tag.album==NULL ? "" : tag.album);
    printf("Year: %s\n",tag.year==NULL ? "" : tag.year);
    printf("Comment: %s\n",tag.comment==NULL ? "" : tag.comment);
    printf("Genre: %s\n",tag.genre1==NULL ? "" : tag.genre1);

    return e_success;
}

/*
 * Function: read_id3v1_field
 * Description: Reads a fixed-size ID3v1 field and stores it as a
 * null-terminated string.
 * Input: destination - address of the output string pointer;
 *        fp - file pointer positioned at the field; size - bytes to read.
 * Return: e_success on success, otherwise e_failure.
 */
Status read_id3v1_field(char **destination, FILE *fp, unsigned int size)
{
    // One extra byte for the end of string.
    char *buffer=malloc(size+1);

    if(buffer==NULL)
    {
        printf("Memory allocation failed\n");
        return e_failure;
    }

    if(fread(buffer,1,size,fp)!=size)
    {
        free(buffer);
        return e_failure;
    }

    buffer[size]='\0';

    free(*destination);
    *destination=buffer;

    return e_success;
}

/*
 * Function: display_id3v1
 * Description: Reads the fixed-size ID3v1 fields from the end of the
 * MP3 file and displays their values.
 * Input: fp - file pointer to the MP3 file.
 * Return: e_success on success, otherwise e_failure.
 */
Status display_id3v1(FILE *fp)
{
    // The title starts 125 bytes before EOF within the 128-byte ID3v1 tag.
    if(fseek(fp,-125,SEEK_END)!=0)
    {
        printf("Unable to seek to ID3v1 title\n");
        return e_failure;
    }

    // Fields must be read in the order they are stored.
    if(read_id3v1_field(&tag.title,fp,30)==e_failure)
    {
        printf("Error reading title\n");
        return e_failure;
    }

    if(read_id3v1_field(&tag.artist,fp,30)==e_failure)
    {
        printf("Error reading artist\n");
        return e_failure;
    }

    if(read_id3v1_field(&tag.album,fp,30)==e_failure)
    {
        printf("Error reading album\n");
        return e_failure;
    }

    if(read_id3v1_field(&tag.year,fp,4)==e_failure)
    {
        printf("Error reading year\n");
        return e_failure;
    }

    if(read_id3v1_field(&tag.comment,fp,30)==e_failure)
    {
        printf("Error reading comment\n");
        return e_failure;
    }

    // The genre is a single number.
    unsigned char genre;

    if(fread(&genre,1,1,fp)==1)
    {
        tag.genre=genre;
    }
    else
    {
        printf("Error reading genre\n");
        return e_failure;
    }

    printf("Title: %s\n",tag.title);
    printf("Artist: %s\n",tag.artist);
    printf("Album: %s\n",tag.album);
    printf("Year: %s\n",tag.year);
    printf("Comment: %s\n",tag.comment);
    printf("Genre: %u\n",tag.genre);

    return e_success;
}

/*
 * Function: view_tags
 * Description: Checks the MP3 file for ID3v2 and ID3v1 tags, prioritizes
 * ID3v2 when both are present, and calls the corresponding reader.
 * Input: argv - command-line arguments; is_edit - selects the filename
 *        argument position (0 for view, 1 for edit).
 * Return: e_success when a supported tag is read; otherwise e_failure.
 */
Status view_tags(char *argv[],int is_edit)
{
    FILE *fp=NULL;
    int id3v1=1;
    int id3v2=1;
    char buffer[3];
    Status status=e_failure;

    // Release previous values so a new read does not retain stale metadata.
    free(tag.title);
    free(tag.artist);
    free(tag.album);
    free(tag.year);
    free(tag.comment);
    free(tag.genre1);

    tag.title=NULL;
    tag.artist=NULL;
    tag.album=NULL;
    tag.year=NULL;
    tag.comment=NULL;
    tag.genre1=NULL;

    // The filename is argv[2] for view and argv[4] when called by edit.
    if(is_edit==0)
    {
        if(open_file(argv[2],&fp)==e_failure)
        {
            return e_failure;
        }
    }
    else
    {
        if(open_file(argv[4],&fp)==e_failure)
        {
            return e_failure;
        }
    }

    if(fread(buffer,1,3,fp)!=3)
    {
        printf("There seems to be a problem reading from audio file\n");
        goto cleanup;
    }

    int i=0;

    // Check for the "ID3" marker at the start of the file.
    while(i<3)
    {
        if(buffer[i]!=ID3V2_TAG[i])
            id3v2=0;

        i++;
    }

    i=0;

    if(fseek(fp,0,SEEK_END)!=0)
    {
        printf("Unable to determine file size\n");
        goto cleanup;
    }

    long size=ftell(fp);

    if(size==-1)
    {
        printf("Unable to determine file size\n");
        goto cleanup;
    }

    // An ID3v1 tag is the last 128 bytes, so the file must be at least that big.
    if(size>=128)
    {
        if(fseek(fp,-128,SEEK_END)!=0)
        {
            printf("Unable to seek in audio file\n");
            goto cleanup;
        }

        if(fread(buffer,1,3,fp)!=3)
        {
            printf("There seems to be a problem reading from audio file\n");
            goto cleanup;
        }

        i=0;

        // Check for the "TAG" marker.
        while(i<3)
        {
            if(buffer[i]!=ID3V1_TAG[i])
                id3v1=0;

            i++;
        }
    }
    else
    {
        id3v1=0;
    }

    // Prefer ID3v2 if available; otherwise fall back to ID3v1.
    if(id3v2==1)
    {
        if(read_id3v2(fp)==e_failure)
            goto cleanup;
    }
    else if(id3v1==1)
    {
        if(display_id3v1(fp)==e_failure)
            goto cleanup;
    }
    else
    {
        printf("No supported ID3 tag found\n");
        goto cleanup;
    }

    status=e_success;

    // The file is always closed here.
    cleanup:
    if(fp!=NULL)
        fclose(fp);

    return status;
}

/*
 * Function: open_file
 * Description: Opens an MP3 file in binary read mode so tag bytes are
 * read without text-mode conversion.
 * Input: filename - file to open; fp - address of the file pointer.
 * Return: e_success if opened; otherwise e_failure.
 */
Status open_file(char *filename, FILE **fp)
{
    *fp=fopen(filename,"rb");

    if(*fp==NULL)
    {
        printf("Error: Unable to open file\n");
        return e_failure;
    }

    return e_success;
}
