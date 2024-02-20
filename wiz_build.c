
#define COMPILER "clang"

#define DEBUG_FLAGS "-g", "-Wall", "-Wextra", "-lX11", "-I", "./lib/"

#define MAIN_SRC "./src/main.c"
#define OUTPUT "./bin/main"

#include "./wiz_build.h"

int main(int argc, char** argv)
{
    WIZ_BUILD_INIT(argc, argv);
    command_t compile = MAKE_CMD(BIN(COMPILER), DEBUG_FLAGS, "-o", OUTPUT);

    size_t files =
        FOR_FILE_IN_DIR("./src/", WHERE( FILE_FORMAT("c") ),
            CMD_APPEND(&compile, FILE_PATH); 
            LOG("Adding " BLUE("%s") " to build", FILE_NAME);
            );

    ASSERT(files > 0);

    SET_COMPILE_TARGET(OUTPUT);
    EXEC_CMD(compile);
    CHECK_COMPILE_STATUS();

    LOG("Running " OUTPUT);

    if(argc > 1 && STRCMP(argv[1], "gdb"))
        CMD(BIN("gdb"), OUTPUT);
    else if(argc > 1 && STRCMP(argv[1], "val"))
        CMD(BIN("valgrind"), OUTPUT);
    else if(argc > 1 && STRCMP(argv[1], "run"))
        CMD(BIN("startx"), "./xinitrc", "--", BIN("Xephyr"), ":1", "-ac", "-screen", "1920x1080" );
    else if(argc > 1 && STRCMP(argv[1], "startx"))
        CMD(BIN("startx"), "./xinitrc", "--", ":1", "-ac" );
    
    WIZ_BUILD_DEINIT();
    return 0;
}

