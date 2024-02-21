#ifndef WIZARDRY_BUILD_H
#define WIZARDRY_BUILD_H

#include<stdlib.h>
#include<stdio.h>
#include<stdarg.h>
#include<dirent.h>
#include<sys/stat.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<sys/wait.h>
#include<time.h>
#include<dlfcn.h>

// NOTE: Replacement has been made
// REPLACE ALL ASSERTS WITH OWN IMPLEMENTATION
//#include<assert.h>

// MEMORY
// Just a scratch pad that gets malloc'ed
// The idea is it's faster to do one fat malloc and use that for
// everything rather than making seperate mallocs
// Some "frees" are made throughout by placing "free points" and
// moving the memory pointer back to that point
#define MAX_MEMORY 1024 * 4
static void* wiz_memory; 
static void* memory_ptr, *free_point;
static size_t max_alloc;
void* wiz_allocate(size_t size);
void wiz_alloc_reset();
void wiz_alloc_set_free_point(); // scratch within a scratch, 
void wiz_alloc_free_to_point();  // set a temporary point at the current mem pointer and roll back to it


// FILE OPERATIONS
enum file_operations_t { FILE_OLDER = 0, FILE_NEWER, FILE_VALID_FORMAT, FILE_INVALID_FORMAT };
typedef enum file_operations_t file_operations_t;

typedef struct
{
    char* file_path; // relative
    struct stat file_info;
} file_t;
file_t file_init(const char* file_path);
file_t file_init_stat_only(char* file_path); // file_path is already allocated
void file_update_stat(file_t* file);
file_operations_t file_compare_date(file_t* file_a, file_t* file_b);
char* file_get_format(char* file_path); // from path

typedef struct
{
    char* dir_path;
    DIR* dir;
    file_t* files;
    size_t len; // length of files array
} dir_t;
// va args for file formats to look for e.g. .h .c .cpp
#define dir_init(DIR_PATH)\
    dir_init_wrapped(DIR_PATH)
dir_t dir_init_wrapped(const char* dir_path);


// SHELL COMMANDS
typedef struct
{
  //  char* command;
    char** args; // NOTE: args[0] == command, args come after, NULL terminated
    size_t len; // number of of args
} command_t;

command_t command_init(const char* command, ...); 
void command_append_arg(command_t* command, command_t command_to_append);
void command_execute(command_t command);


// NOTE: Welcome to macro land
#define MAKE_CMD(...)\
    command_init(__VA_ARGS__, NULL)
#define EXEC_CMD(COMMAND)\
    command_execute(COMMAND)
#define CMD_APPEND(COMMAND, ...)\
    command_append_arg(COMMAND, MAKE_CMD(__VA_ARGS__, NULL))
#define CMD(...)\
do{\
    wiz_alloc_set_free_point();\
    command_t COMMAND = MAKE_CMD(__VA_ARGS__);\
    command_execute( COMMAND );\
    wiz_alloc_free_to_point();\
}while(0)

#define BIN(BIN_NAME)\
    "/usr/bin/" BIN_NAME

#define FILE_EXISTS(FILE_PATH)\
({\
    int RESULT = 0;\
    FILE* FP = fopen(FILE_PATH, "r");\
    if(FP == NULL)\
    { RESULT = -1; }\
    else\
    { fclose(FP); }\
    RESULT;\
})
#define MKDIR(...) CMD(BIN("mkdir"), __VA_ARGS__)
#define TOUCH(...) CMD(BIN("touch"), __VA_ARGS__)
#define MV(FROM, TO) CMD(BIN("bash"), "mv", FROM, TO)

file_t compile_target;

#define SET_COMPILE_TARGET(TARGET)\
    TOUCH(TARGET);\
    compile_target = file_init(TARGET)

#define CHECK_COMPILE_STATUS()\
{\
    long LAST_MODIFY = compile_target.file_info.st_mtim.tv_nsec;\
    file_update_stat(&compile_target);\
    if(LAST_MODIFY == compile_target.file_info.st_mtim.tv_sec)\
    {LOG("Compilation of " BLUE("%s") " has " RED("FAILED"), compile_target.file_path);}\
    else\
    {LOG("Compilation of " BLUE("%s") " has " GREEN("SUCCEEDED"), compile_target.file_path);}\
}

#define NULL_TERMINATOR 1

#ifndef COMPILER
#define COMPILER "clang"
#endif

#ifndef DEBUG_FLAGS
#define DEBUG_FLAGS "-Wall", "-Wextra", "-g"
#endif

#define STRCPY(DEST, SRC)\
    DEST = wiz_allocate(strlen(SRC) + NULL_TERMINATOR);\
    (void)memcpy(DEST, SRC, strlen(SRC) + 1);

#define STRCAT(DEST, SRC)\
({\
    size_t DEST_LEN = strlen(DEST);\
    size_t SRC_LEN = strlen(SRC);\
    char* NEW_STR = wiz_allocate(DEST_LEN + SRC_LEN + NULL_TERMINATOR);\
    (void)memcpy(NEW_STR, DEST, DEST_LEN);\
    (void)memcpy(NEW_STR + DEST_LEN, SRC, SRC_LEN);\
    NEW_STR[DEST_LEN + SRC_LEN] = '\0';\
    NEW_STR;\
})

#define STRDUP(STRING)\
({\
    size_t LEN = strlen(STRING);\
    char* NEW_STR = wiz_allocate(LEN + NULL_TERMINATOR);\
    (void)memcpy(NEW_STR, STRING, LEN);\
    NEW_STR[LEN] = '\0';\
    NEW_STR;\
})

// NOTE: Doesn't work
#define COUNT_VA_ARGS(VA_LIST, FIRST)\
{\
    size_t count = 0;\
    va_start(VA_LIST, FIRST);\
    for(char* arg = va_arg(VA_LIST); arg != NULL; arg = va_arg(VA_LIST, char*))\
    { count++; }\
    va_end(VA_LIST);\
    count;\
}

#define GET_FILE_FORMAT(FILE_NAME)\
    file_get_format(FILE_NAME)

// NOTE: FOR FILE IN DIR SECTION - don't use these macros outside a FOR_FILE_IN_DIR loop

#define FILE_NAME CURRENT_FILE->d_name
#define FILE_PATH STRCAT(DIR_PATH_ALLOC, FILE_NAME)

#define WHERE(CONDITION) if ( CONDITION )

#define STRCMP(STR1, STR2) strcmp(STR1, STR2) == 0
#define FILE_FORMAT(FORMAT) STRCMP(file_get_format(STRDUP(CURRENT_FILE->d_name)), FORMAT)

#define FILE_LAST_MODIFIED(FILE_PATH)\
({\
    wiz_alloc_set_free_point();\
    file_t FILE_A = file_init(FILE_PATH);\
    file_t FILE_B = file_init( STRCAT(DIR_PATH_ALLOC, CURRENT_FILE->d_name) );\
    file_operations_t RESULT = file_compare_date(&FILE_A, &FILE_B);\
    wiz_alloc_set_free_point();\
    RESULT;\
})
#define FILE_OLDER_THAN(FILE_PATH) FILE_LAST_MODIFIED(FILE_PATH) == 0
#define FILE_NEWER_THAN(FILE_PATH) FILE_LAST_MODIFIED(FILE_PATH) == 1

#define FOR_FILE_IN_DIR(DIR_PATH, SORT_BY, ...)\
({\
    DIR* DIR = opendir(DIR_PATH);\
    size_t FILES_MATCHED = 0;\
    ASSERT(DIR != NULL);\
    char* DIR_PATH_ALLOC = NULL;\
    if(DIR_PATH[strlen(DIR_PATH)-1] != '/'){\
    STRCPY(DIR_PATH_ALLOC, STRCAT(DIR_PATH, "/"));}\
    else{\
    STRCPY(DIR_PATH_ALLOC, DIR_PATH);}\
    struct dirent *CURRENT_FILE = NULL;\
    while((CURRENT_FILE = readdir(DIR)) != NULL)\
    {\
        if(CURRENT_FILE->d_name[0] == '.')\
        {continue;}\
        else if(CURRENT_FILE->d_type == DT_DIR)\
        {continue;}\
        SORT_BY \
        {\
            __VA_ARGS__;\
            FILES_MATCHED++;\
        }\
    }\
    FILES_MATCHED;\
})


//            COMMAND = MAKE_CMD(__VA_ARGS__, STRCAT(DIR_PATH_ALLOC, CURRENT_FILE->d_name));
//            EXEC_CMD(COMMAND);
// NOTE: END OF FOR FILE IN DIR SECTION

// Strictly for debugging wiz_build
#define PRINT_CMD(COMMAND)\
    for(size_t i = 0; i < COMMAND.len; i++)\
    {\
        fprintf(stderr, "%s ", COMMAND.args[i]);\
    }


// LOGGING
#define RESET_COLOR         "\033[0m"

#define FG_BRIGHT_GRAY      "\033[0;90m"
#define FG_BRIGHT_RED       "\033[0;91m"
#define FG_BRIGHT_GREEN     "\033[0;92m"
#define FG_BRIGHT_YELLOW    "\033[0;93m"
#define FG_BRIGHT_BLUE      "\033[0;94m"
#define FG_BRIGHT_MAGENTA   "\033[0;95m"
#define FG_BRIGHT_CYAN      "\033[0;96m"
#define FG_BRIGHT_WHITE     "\033[0;97m"

#define BLUE(FILE_NAME)\
    FG_BRIGHT_BLUE FILE_NAME RESET_COLOR
#define RED(FILE_NAME)\
    FG_BRIGHT_RED FILE_NAME RESET_COLOR
#define YELLOW(FILE_NAME)\
    FG_BRIGHT_YELLOW FILE_NAME RESET_COLOR
#define GREEN(FILE_NAME)\
    FG_BRIGHT_GREEN FILE_NAME RESET_COLOR
#define MAGENTA(FILE_NAME)\
    FG_BRIGHT_MAGENTA FILE_NAME RESET_COLOR
#define CYAN(FILE_NAME)\
    FG_BRIGHT_CYAN FILE_NAME RESET_COLOR

#define SET_COLOR(COLOR) printf(COLOR)
#define LOG(...)\
    fprintf(stderr, FG_BRIGHT_WHITE "|WIZ-LOG| " RESET_COLOR __VA_ARGS__);\
    fprintf(stderr, "\n")
#define WARN(...)\
    fprintf(stderr, FG_BRIGHT_YELLOW "|WIZ-LOG| " RESET_COLOR __VA_ARGS__);\
    fprintf(stderr, "\n")
#define ERROR(...)\
    fprintf(stderr, FG_BRIGHT_RED "|WIZ-LOG| " RESET_COLOR __VA_ARGS__);\
    fprintf(stderr, "\n")

#define ASSERT(expr)\
    if(expr)\
    {}\
    else\
    {\
        PANIC(MAGENTA("EXPECT") "(" CYAN(#expr) ")" RED(" FAILED") " at: " BLUE(__FILE__) ":" YELLOW("%d"), __LINE__ );\
    }

#define TEST(expr)\
    if(expr)\
    {\
        LOG(MAGENTA("TEST") "(" CYAN(#expr) ")" GREEN(" SUCCEEDED") " at: " BLUE(__FILE__) ":" YELLOW("%d"), __LINE__ );\
    }\
    else\
    {\
        WARN(MAGENTA("TEST") "(" CYAN(#expr) ")" RED(" FAILED") " at: " BLUE(__FILE__) ":" YELLOW("%d"), __LINE__ );\
    }

// wiz_build self-rebuild + alloc scratch
#define WIZ_BUILD_INIT(ARGC, ARGV) wiz_build_init(ARGC, ARGV)
#define WIZ_BUILD_DEINIT() wiz_build_deinit()
void wiz_build_init(int argc, char **argv);
void wiz_build_deinit();

#define PANIC(...)\
    ERROR(__VA_ARGS__);\
    wiz_build_deinit();


// ## Function definitions

void* wiz_allocate(size_t size)
{
    ASSERT((memory_ptr + size) <= (wiz_memory + (size_t)(MAX_MEMORY)));

    void* alloc = memory_ptr;
    memory_ptr += size;

    if((size_t)(memory_ptr - wiz_memory) > max_alloc)
        max_alloc = memory_ptr - wiz_memory;

    return alloc;
}

void wiz_alloc_reset()
{
    memory_ptr = wiz_memory;
}

// Allows for some freeing, significantly reduces overall peak memory usage
void wiz_alloc_set_free_point()
{
    free_point = memory_ptr;
}
void wiz_alloc_free_to_point()
{
    ASSERT(free_point != NULL);
    memory_ptr = free_point;
}

file_t file_init(const char* file_path)
{
    file_t new_file = {0};
    size_t file_name_len = strlen(file_path) + NULL_TERMINATOR;
    int result = 0;
    void *alloc = NULL;

    result = stat(file_path, &new_file.file_info);
    ASSERT(result == 0);

    alloc = wiz_allocate(file_name_len);
    ASSERT(alloc != NULL);
    new_file.file_path = alloc;

    (void)memcpy(alloc, file_path, file_name_len);

    return new_file;
}

// File_path already allocated
file_t file_init_stat_only(char* file_path)
{
    file_t new_file = {0};
    int result = 0;

    result = stat(file_path, &new_file.file_info);
    ASSERT(result == 0);

    new_file.file_path = file_path;

    return new_file;
}

// Compares last modified date
file_operations_t
file_compare_date(file_t* file_a, file_t* file_b)
{
    // stat -> status modified time -> time spec.tv_sec(onds) 
    long time_a = file_a->file_info.st_mtim.tv_sec;
    long time_b = file_b->file_info.st_mtim.tv_sec;

//    fprintf(stderr, "a-bin: %ld, b-src: %ld\n", time_a, time_b);
    if(time_a > time_b)
        return FILE_OLDER;
    return FILE_NEWER;
}

void file_update_stat(file_t* file)
{
    ASSERT( stat(file->file_path, &file->file_info) == 0 );
}

// TODO: Handle cases where file doesn't have format
char* file_get_format(char* file_path)
{
    char* substr = strtok(file_path, ".");
    char* prev_substr = substr;

    while((substr = strtok(NULL, ".")) != NULL)
        prev_substr = substr;
    return prev_substr;
}

// TODO: Make it recursive and add file formats as argument
// NOTE: Actually, might just scrap all of this(see FOR_FILE_IN_DIR macro)
dir_t dir_init_wrapped(const char* dir_path)
{
    dir_t new_dir = {0};
    struct dirent *current_file = NULL;

    char* file_format = NULL;
    file_t new_file = {0};
    size_t max_files = 0, files = 0;

    //TODO: handle this case v
//    if(dir_path[strlen(dir_path)] != '/');

    new_dir.dir = opendir(dir_path);
    ASSERT(new_dir.dir != NULL);

    while((current_file = readdir(new_dir.dir)) != NULL)
    {
        if(current_file->d_name[0] == '.')
            continue; 
        max_files++;
    }
    rewinddir(new_dir.dir);

    // TODO: better way to approximate max files
    new_dir.files = wiz_allocate(max_files * sizeof(file_t));

    while((current_file = readdir(new_dir.dir)) != NULL)
    {
        if(current_file->d_name[0] == '.')
            continue; 
        // TODO: cut down memory usage by passing reference
        file_format = file_get_format(STRDUP(current_file->d_name));
        // TODO: support for custom file types via VA_LIST
        if(file_format[0] == 'c' || file_format[0] == 'h')
        {
            new_file = file_init_stat_only(STRCAT(dir_path, current_file->d_name));
            new_dir.files[files] = new_file;
            files++;
        }
    }

    ASSERT(closedir(new_dir.dir) == 0);
    new_dir.len = files;

    return new_dir;
}

command_t command_init(const char* command, ...)
{
    command_t new_command = {0};

    va_list arguments;

    // TODO: make this a macro
    size_t va_list_len = 0; 
    va_start(arguments, command);
    for(char* arg = va_arg(arguments, char*); arg != NULL; arg = va_arg(arguments, char*))
    {va_list_len++;}
    va_end(arguments);

    // +1 to account for command also needing to be in args[0]
    // +1 to account for NULL terminator
    new_command.args = wiz_allocate(sizeof(char*) * (va_list_len + NULL_TERMINATOR + 1));
//    new_command.command = wiz_allocate(sizeof(char*));

    STRCPY(new_command.args[0], command);
 //   new_command.command = new_command.args[0];
    new_command.len++;

    va_start(arguments, command);

    for(char* arg = va_arg(arguments, char*);
        arg != NULL;
        arg = va_arg(arguments, char*))
    {
        STRCPY(new_command.args[new_command.len], arg);
        new_command.len++;
    }

    va_end(arguments);
    
    // execvp() expects last element in argv to be NULL
    new_command.args[new_command.len] = NULL;

    return new_command;
}

// NOTE: Untested
void command_append_arg(command_t* command, command_t command_to_append)
{
    size_t args_buffer_size = 
        (command->len + command_to_append.len + NULL_TERMINATOR)
            * (sizeof(char*) );

    size_t str_total_len = 0;

    for(size_t i = 0; i < command->len; i++)
        str_total_len += strlen(command->args[i]);

    for(size_t i = 0; i < command_to_append.len; i++)
        str_total_len += strlen(command_to_append.args[i]);

    if((sizeof(char*)*3)+args_buffer_size + str_total_len + command == memory_ptr)
        memory_ptr = command;

    char** args_buffer = wiz_allocate(args_buffer_size);

    for(size_t i = 0; i < command->len; i++)
    {
        STRCPY(args_buffer[i], command->args[i]);
    }

    for(size_t i = 0; i < command_to_append.len; i++)
    {
        STRCPY(args_buffer[command->len+i], command_to_append.args[i]);
    }

    command->args = args_buffer;
    command->len += command_to_append.len;
    command->args[command->len] = NULL;
}

void command_execute(command_t command)
{
    pid_t pid = fork();
    int status = 0;
    ASSERT(pid >= 0);

    // Parent waits for child
    if(pid == 0)
    {
        ASSERT(execvp(command.args[0], command.args) != -1);
        fprintf(stderr, "ERRNO: %s\n", strerror(errno));
    } 
    else{
        ASSERT(waitpid(pid, &status, 0) >= 0);
    }
}

void wiz_build_init(int argc, char **argv)
{
    LOG("Running in debug mode");
    wiz_memory = malloc(MAX_MEMORY);
    max_alloc = 0;

    ASSERT(wiz_memory != NULL);
    memory_ptr = wiz_memory;

    ASSERT(argc != 0);
    file_t bin_file = file_init(argv[0]);
    file_t src_file = file_init(STRCAT(argv[0], ".c"));
    
    file_operations_t result = file_compare_date(&bin_file, &src_file);

    if(result == FILE_NEWER)
    {
        long last_modified = bin_file.file_info.st_mtim.tv_nsec;
        struct timespec begin, end;

        LOG("Changes in " BLUE("%s") " detected, " YELLOW("RECOMPILING..."), src_file.file_path);

        clock_gettime(CLOCK_MONOTONIC_RAW, &begin);
        CMD(BIN(COMPILER), DEBUG_FLAGS, src_file.file_path, "-o", argv[0]); 
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        file_update_stat(&bin_file); // If bin file hasn't been modified, then we'll assume the compilation failed
        if(last_modified == bin_file.file_info.st_mtim.tv_nsec)
        {
            LOG("Compilation of " BLUE("%s") " has " RED("FAILED") "\n", src_file.file_path);
            wiz_build_deinit();
        }else
        {
            LOG("Compilation of " BLUE("%s") " has " GREEN("SUCCEEDED") " in " MAGENTA("%.4f") "s\n",
                    src_file.file_path,
                    (end.tv_nsec - begin.tv_nsec) / 1000000000.0 + (end.tv_sec - begin.tv_sec));
            (void)execvp(argv[0], &argv[0]);
        }
    }
    wiz_alloc_reset();
    // TEST DIR
    /*
    dir_t dir_test = dir_init("./");

    LOG("File from dir reader test: " BLUE("%s") " found in dir", dir_test.files[0].file_path);
    LOG("File from dir reader test: " BLUE("%s") " found in dir", dir_test.files[1].file_path);
    wiz_alloc_reset();
*/
}

void wiz_build_deinit()
{
    size_t used_memory = (size_t)(memory_ptr - wiz_memory);
    free(wiz_memory);
    wiz_memory = NULL;

    LOG(YELLOW("%zu/%d") " peak bytes allocated", max_alloc, MAX_MEMORY);
    LOG(YELLOW("%zu/%d") " bytes allocated on exit", used_memory, MAX_MEMORY);
    exit(0);
}


#endif /* WIZARDRY_BUILD_H */
