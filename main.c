/*
 * Project: MP3 Tag Reader and Editor
 * Author: Prudhvikrishna Damaraju
 * Description:
 * This program reads and edits metadata stored in MP3 files.
 * It supports displaying ID3v1/ID3v2 tags, editing ID3v2 tags,
 * and displaying usage instructions.
 */

#include "common.h"
#include "types.h"
#include "view.h"
#include "edit.h"

ID3vTag tag;

/*
 * Function: display_help
 * Description:
 * Displays the available operations and their command-line syntax.
 * This helps the user understand how to execute the program.
 *
 * Input: None
 * Return: None
 */
void display_help()
{
    printf("Usage:\n");
    printf("./a.out -h\n");
    printf("./a.out -v <mp3_file>\n");
    printf("./a.out -e <option> <new_value> <mp3_file>\n");
}

/*
 * Function: check_operation
 * Description:
 * Identifies the operation specified by the user.
 * This allows main() to execute the corresponding functionality.
 *
 * Input: Command-line arguments
 * Return: The identified operation type.
 */
OperationType check_operation(char *argv[])
{
    if(strcmp(argv[1],"-h")==0)
    {
         return e_help;
    }
    else if(strcmp(argv[1],"-v")==0)
    {
        return  e_view;
    }
    else if(strcmp(argv[1],"-e")==0)
    {
        return e_edit;
    }
    else
    {
        return  e_unsupported;
    }
}

/*
 * Function: main
 * Description:
 * Acts as the entry point of the program.
 * Validates the command-line arguments and routes execution
 * to the help, view, or edit operation.
 *
 * Input:
 * argc - Number of command-line arguments.
 * argv - Array containing the command-line arguments.
 *
 * Return:
 * 0 - Successful execution.
 * 1 - Invalid arguments or operation.
 */
int main(int argc, char *argv[])
{
    // At least one argument is required to identify the operation.
    if(argc<2)
    {
        printf("No of arguments were invalid\n");
        printf("Usage:./a.out -h\n"
               "       ./a.out -v <mp3_file>\n"
               "       ./a.out -e <option> <new_value> <mp3_file>\n");
        return 1;
    }
    else
    {
        // Identify the operation requested by the user.
        OperationType res=check_operation(argv);

        if(res==e_help)
        {
             // Help operation accepts only the -h option.
             if(argc==2)
             {
                 display_help();
             }
             else
             {
                 printf("Invalid arguments for help\n");
             }
        }
        else  if(res==e_view)
        {
           // View operation requires exactly one MP3 filename.
           if(argc==3)
           {
               int len=strlen(argv[2]);

               // Verify the file extension before attempting to read the file.
               if(len>4 && strcmp(argv[2]+len-4,".mp3")==0)
               {
                      if(view_tags(argv,0)==e_failure)
                      printf("Seems to have a problem\n");
               }
               else
               {
                printf("Invalid file type.Please provide MP3 file");
                return 0;
               }
           }
           else
           {
               printf("Invalid arguments for view\n");
           }
        }
       else if(res==e_edit)
       {
          // Edit operation requires an option, new value, and MP3 filename.
          if(argc==5)
         {
        // Accept only the tag fields supported by the edit operation.
        if(strcmp(argv[2],"-t")!=0 && strcmp(argv[2],"-a")!=0 &&
           strcmp(argv[2],"-A")!=0 && strcmp(argv[2],"-y")!=0 &&
           strcmp(argv[2],"-c")!=0 && strcmp(argv[2],"-g")!=0)
        {
            printf("Invalid edit option\n");
            return 1;
        }

        // A new value must be provided for the selected tag field.
        if(strlen(argv[3])==0)
        {
            printf("Invalid new value\n");
            return 1;
        }

        int len=strlen(argv[4]);

        // Verify the filename extension before starting the edit operation.
        if(len>4 && strcmp(argv[4]+len-4,".mp3")==0)
        {
            // Pass the filename, selected field, and replacement value.
            edit_tags(argv[4],argv[2],argv[3],argv);
        }
        else
        {
            printf("Invalid file type. Please provide MP3 file\n");
            return 1;
        }
        }
        else
        {
        printf("Invalid arguments for edit\n");
        return 1;
        }
       }
        else  if(res==e_unsupported)
        {
          // Handle any operation that is not supported by the program.
          printf("Unsupported operation\n");
        }
    }
    return 0;
}