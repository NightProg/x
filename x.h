#ifndef XBUILD_XBUILD_H
#define XBUILD_XBUILD_H
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdarg.h>

#define RUN_CMD(args...) do { \
    char *cmd[] = { args, NULL }; \
    Cmd *c = new_cmd();   \
    append_args(c, cmd);  \
    execute_cmd(c);         \
} while (0)

#define CMD(args...) \
    new_from_args(args, NULL)


typedef struct {
    int size;
    int capacity;
    char **strings;
} StringList;

char *ltrim(char *s)
{
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s)
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

StringList *new_string_list() {
    StringList *list = malloc(sizeof(StringList));
    if (list == NULL) {
        return NULL;
    }
    list->size = 0;
    list->capacity = 8;
    list->strings = malloc(sizeof(char*) * list->capacity);
    if (list->strings == NULL) {
        free(list);
        printf("Failed to allocate memory\n");
        return NULL;
    }
    return list;
}

int remove_string(StringList *list, int index) {
    if (index < 0 || index >= list->size) {
        return -1;
    }
    for (int i = index; i < list->size - 1; i++) {
        list->strings[i] = list->strings[i + 1];
    }
    list->size--;
    return 0;
}

void append_string(StringList *list, char *string) {
    if (list->size == list->capacity) {
        list->capacity *= 2;
        char** new_strings = malloc(sizeof(char*) * list->capacity);
        if (new_strings == NULL) {
            return;
        }
        memcpy(new_strings, list->strings, sizeof(char*) * list->size);
        free(list->strings);
        list->strings = new_strings;

    }
    list->strings[list->size++] = string;
}

StringList *string_list_from_array(int length, char **array) {
    StringList *list = new_string_list();
    for (int i = 0; i < length; i++) {
        append_string(list, array[i]);
    }
    return list;
}

int find_string(StringList *list, char *string) {
    for (int i = 0; i < list->size; i++) {
        if (strcmp(list->strings[i], string) == 0) {
            return i;
        }
    }
    return -1;
}

void free_string_list(StringList *list) {
    free(list->strings);
    free(list);
}


typedef struct {
    StringList *args;
} Cmd;

Cmd *new_cmd();
void append_arg(Cmd *cmd, char *command);
void append_args(Cmd *cmd, StringList *commands);
int execute_cmd(Cmd *cmd);
char* display_cmd(Cmd *cmd);
void free_cmd(Cmd *cmd) {
    free(cmd->args);
    free(cmd);
};

Cmd *new_cmd() {
    Cmd *cmd = malloc(sizeof(Cmd));
    if (cmd == NULL) {
        return NULL;
    }
    cmd->args = new_string_list();
    return cmd;
}

Cmd* new_from_args(char* first, ...) {
    va_list args;
    va_start(args, first);
    Cmd *cmd = new_cmd();
    char *arg = first;
    while (arg != NULL) {
        append_arg(cmd, arg);
        arg = va_arg(args, char*);
    }
    va_end(args);
    return cmd;
}

void append_arg(Cmd *cmd, char *command) {
    append_string(cmd->args, command);
}

void append_args(Cmd *cmd, StringList *args) {
    for (int i = 0; i < args->size; i++) {
        append_arg(cmd, args->strings[i]);
    }
}

int execute_cmd(Cmd *cmd) {
    if (cmd->args->size == 0) {
        return -1; // No command to execute
    }
    char *command = display_cmd(cmd);
    printf("-%s\n", command);
    system(command);
    return 0;
}

FILE* popen_cmd(Cmd *cmd) {
    if (cmd->args->size == 0) {
        return NULL; // No command to execute
    }
    FILE *fp = popen(display_cmd(cmd), "r");
    if (fp == NULL) {
        return NULL;
    }
    return fp;
}

char* display_cmd(Cmd *cmd) {
    unsigned long length = 0;
    for (int i = 0; i < cmd->args->size; i++) {
        length += strlen(cmd->args->strings[i]) + 1; // +1 for space
    }
    char *result = malloc(length + 1);
    if (result == NULL) {
        return NULL;
    }

    result[length] = '\0'; // Null-terminate the string

    for (int i = 0; i < cmd->args->size; i++) {
        strcat(result, " ");
        strcat(result, cmd->args->strings[i]);
    }

    return result;
}

typedef struct {
    size_t size;
    size_t capacity;
    Cmd **cmds;
} CmdList;

CmdList *new_cmd_list() {
    CmdList *list = malloc(sizeof(CmdList));
    if (list == NULL) {
        return NULL;
    }
    list->size = 0;
    list->capacity = 8;
    list->cmds = malloc(sizeof(Cmd*) * list->capacity);
    if (list->cmds == NULL) {
        free(list);
        return NULL;
    }
    return list;
}

void append_cmd(CmdList *list, Cmd *cmd) {
    if (list->size == list->capacity) {
        list->capacity *= 2;
        Cmd **new_cmds = malloc(sizeof(Cmd*) * list->capacity);
        if (new_cmds == NULL) {
            return;
        }
        memcpy(new_cmds, list->cmds, sizeof(Cmd*) * list->size);
        free(list->cmds);
        list->cmds = new_cmds;
    }
    list->cmds[list->size++] = cmd;
}

void free_cmd_list(CmdList *list) {
    for (int i = 0; i < list->size; i++) {
        free_cmd(list->cmds[i]);
    }
    free(list->cmds);
    free(list);
}



typedef struct {
    StringList *matched;
} PatternFile;

PatternFile *match_file(char *pattern) {
    PatternFile *file = malloc(sizeof(PatternFile));
    if (file == NULL) {
        return NULL;
    }
    file->matched = new_string_list();
    Cmd* cmd = CMD("ls", pattern);
    FILE *fp = popen(display_cmd(cmd), "r");
    if (fp == NULL) {
        return NULL;
    }

    char* buffer = malloc(1024);
    while (fgets(buffer, 1024, fp) != NULL) {
        rtrim(buffer);
        append_string(file->matched, strdup(buffer));
    }
    free(buffer);

    return file;
}

int mkdir_if_not_exist(char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0700);
    }
    return 0;
}


long get_time(char* file) {
    struct stat attr;
    if (stat(file, &attr) == -1) {
        return -1;
    }
    return attr.st_mtime;
}


typedef enum {
    EXECUTABLE,
    STATIC,
    SHARED,
    OTHER
} BuildType;

typedef struct {
    char* name;
    BuildType type;
    char* c_compiler;
    char* output_dir;
    char* output_file;
    char* description;
    StringList *output_files;
    StringList *c_flags;
    StringList *c_libs;
    StringList *c_sources;
    StringList *obj_files;
    CmdList *cmds;

} Target;

Target* new_target(char* name, BuildType type, char* output_dir) {
    Target* target = malloc(sizeof(Target));
    if (target == NULL) {
        return NULL;
    }
    target->name = name;
    target->type = type;
    target->c_flags = new_string_list();
    target->c_sources = new_string_list();
    target->obj_files = new_string_list();
    target->c_libs = new_string_list();
    target->output_files = new_string_list();
    target->cmds = new_cmd_list();

    if (output_dir == NULL) {
        target->output_dir = "dist";
    } else {
        target->output_dir = output_dir;
    }
    mkdir_if_not_exist(target->output_dir);
    char* target_dir = malloc(strlen(target->output_dir) + strlen(target->name) + 2);
    if (target_dir == NULL) {
        return NULL;
    }
    sprintf(target_dir, "%s/%s", target->output_dir, target->name);
    mkdir_if_not_exist(target_dir);

    char* output;
    if (target->type == EXECUTABLE) {
        output = malloc(strlen(target->output_dir) + strlen(target->name) + 3);
        if (output == NULL) {
            return NULL;
        }
        sprintf(output, "%s/%s/%s", target->output_dir, target->name, target->name);
    } else if (target->type == STATIC) {
        output = malloc(strlen(target->output_dir) + strlen(target->name) * 2 + 10);
        if (output == NULL) {
            return NULL;
        }
        sprintf(output, "%s/%s/lib%s.a", target->output_dir, target->name, target->name);
    } else if (target->type == SHARED) {
        output = malloc(strlen(target->output_dir) + strlen(target->name) * 2 + 10);
        if (output == NULL) {
            return NULL;
        }
        sprintf(output, "%s/%s/lib%s.so", target->output_dir, target->name, target->name);
    }
    target->output_file = output;

    return target;
}

void set_target_output_file(Target* target, char* output_file) {
    target->output_file = output_file;
}

void set_target_description(Target* target, char* description) {
    target->description = description;
}

void set_target_compiler(Target* target, char* compiler) {
    target->c_compiler = compiler;
}

void add_target_flag(Target* target, char* flag) {
    append_string(target->c_flags, flag);
}

void add_target_source(Target* target, char* source) {
    append_string(target->c_sources, source);
}

void add_target_lib(Target* target, char* lib) {
    append_string(target->c_libs, lib);
}

void add_target_source_pattern(Target* target, char* pattern) {
    PatternFile *file = match_file(pattern);
    for (int i = 0; i < file->matched->size; i++) {
        add_target_source(target, file->matched->strings[i]);
    }
}

void remove_target_source(Target* target, char* source) {
    int index = find_string(target->c_sources, source);
    if (index != -1) {
        remove_string(target->c_sources, index);
    }
}

void add_target_cmd(Target* target, Cmd* cmd) {
    append_cmd(target->cmds, cmd);
}

void target_link_target(Target* target, Target* link) {
    if (link->type == EXECUTABLE) {
        printf("Cannot link an executable\n");
        return;
    }
    char* flag = malloc(strlen(link->name) + 3);
    if (flag == NULL) {
        return;
    }
    sprintf(flag, "-l%s", link->name);
    add_target_lib(target, flag);
    char *path = malloc(strlen(link->output_dir) + strlen(link->name) + 3);
    if (path == NULL) {
        return;
    }

    sprintf(path, "-L%s/%s", link->output_dir, link->name);
    add_target_lib(target, path);
}

void add_target_sources(Target* target, char* first, ...) {
    va_list args;
    va_start(args, first);
    char *arg = first;
    while (arg != NULL) {
        add_target_source(target, arg);
        arg = va_arg(args, char*);
    }
    va_end(args);
}

char* recursive_mkpath(char* path) {
    char* p = path;
    while (*p != '\0') {
        if (*p == '/') {
            *p = '\0';
            mkdir_if_not_exist(path);
            *p = '/';
        }
        p++;
    }
    return path;
}

void build_object_for(Target* target, char* source) {
    char* output = malloc(strlen(target->output_dir) + strlen(target->name) + strlen(source) + 3);
    if (output == NULL) {
        return;
    }

    sprintf(output, "%s/%s/%s.o", target->output_dir, target->name, source);
    recursive_mkpath(output);
    Cmd* cmd = new_from_args(target->c_compiler, "-c", source, "-o", output, NULL);
    append_args(cmd, target->c_flags);
    execute_cmd(cmd);
    append_string(target->obj_files, output);
}

void build_objects_for(Target* target) {
    for (int i = 0; i < target->c_sources->size; i++) {
        build_object_for(target, target->c_sources->strings[i]);
    }
}

void build_executable(Target* target) {
    if (target->obj_files->size == 0) {
        return; // No object files to build
    }
    Cmd* cmd = new_from_args(target->c_compiler, "-o", target->output_file, NULL);
    append_args(cmd, target->obj_files);
    append_args(cmd, target->c_libs);
    execute_cmd(cmd);
}

void build_static_library(Target* target) {
    if (target->obj_files->size == 0) {
        return; // No object files to build
    }
    Cmd* cmd = new_from_args("ar", "rcs", target->output_file, NULL);
    append_args(cmd, target->obj_files);
    append_args(cmd, target->c_libs);
    execute_cmd(cmd);
}

void build_shared_library(Target* target) {
    Cmd* cmd = new_from_args(target->c_compiler, "-shared", "-o", target->output_file, NULL);
    append_args(cmd, target->obj_files);
    append_args(cmd, target->c_libs);
    execute_cmd(cmd);
}

void build_target(Target* target) {
    for (int i = 0; i < target->cmds->size; i++) {
        execute_cmd(target->cmds->cmds[i]);
    }
    switch (target->type) {
        case EXECUTABLE:
            build_executable(target);
            break;
        case STATIC:
            build_static_library(target);
            break;
        case SHARED:
            build_shared_library(target);
            break;
        case OTHER: // Do nothing
            break;
    }
}


void auto_target_build_for(Target* target, char* file) {
    int index = find_string(target->c_sources, file);
    if (index == -1) {
        return;
    }
    char* o_files = malloc(strlen(target->output_dir) + strlen(target->name) + strlen(file) + 3);
    if (o_files == NULL) {
        return;
    }
    sprintf(o_files, "%s/%s/%s.o", target->output_dir, target->name, file);
    long obj_time = get_time(o_files);
    long source_time = get_time(file);
    if (source_time > obj_time) {
        build_object_for(target, file);
    }
}

void auto_target_build(Target* target) {
    for (int i = 0; i < target->c_sources->size; i++) {
        auto_target_build_for(target, target->c_sources->strings[i]);
    }
    build_target(target);
}


typedef struct {
    Target **target;
    size_t size;
    size_t capacity;

} TargetCli;

TargetCli* new_target_cli() {
    TargetCli *cli = malloc(sizeof(TargetCli));
    if (cli == NULL) {
        return NULL;
    }
    cli->size = 0;
    cli->capacity = 8;
    cli->target = malloc(sizeof(Target*) * cli->capacity);
    if (cli->target == NULL) {
        free(cli);
        return NULL;
    }
    return cli;
}

void append_target_cli(TargetCli *cli, Target *target) {
    if (cli->size == cli->capacity) {
        cli->capacity *= 2;
        Target **new_target = malloc(sizeof(Target*) * cli->capacity);
        if (new_target == NULL) {
            return;
        }
        memcpy(new_target, cli->target, sizeof(Target*) * cli->size);
        free(cli->target);
        cli->target = new_target;
    }
    cli->target[cli->size++] = target;
}

void target_cli_print_usage(TargetCli *cli, int argc, char** argv) {
    printf("Usage: %s <target name> [options]\n", argv[0]);
    printf("Targets:\n");
    for (int i = 0; i < cli->size; i++) {
        printf("\t%s", cli->target[i]->name);
        if (cli->target[i]->description != NULL) {
            printf(" - %s", cli->target[i]->description);
        }
        printf("\n");
    }
    printf("\n");
    printf("Options:\n");
    printf("\t-h, --help\t\tPrint this help message\n");
    printf("\t-a, --all\t\tBuild all targets\n");
    printf("\t-c, --clean\t\tClean all targets\n");
    printf("\t-t, --auto\t\tBuild the targets that need to be built\n");
}

void target_cli(TargetCli *cli, int argc, char** argv) {
    if (argc < 2) {
        target_cli_print_usage(cli, argc, argv);
        return;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        target_cli_print_usage(cli, argc, argv);
        return;
    }

    if (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--all") == 0) {
        for (int i = 0; i < cli->size; i++) {
            build_objects_for(cli->target[i]);
            build_target(cli->target[i]);
        }
        return;
    }

    if (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--clean") == 0) {
        for (int i = 0; i < cli->size; i++) {
            for (int j = 0; j < cli->target[i]->obj_files->size; j++) {
                remove(cli->target[i]->obj_files->strings[j]);
            }
            remove(cli->target[i]->output_file);
        }
        return;
    }
    int is_auto = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--auto") == 0) {
            is_auto = 1;
            continue;
        }

        for (int j = 0; j < cli->size; j++) {
            if (strcmp(argv[i], cli->target[j]->name) == 0) {
                if (is_auto) {
                    auto_target_build(cli->target[j]);
                } else {
                    build_objects_for(cli->target[j]);
                    build_target(cli->target[j]);
                }
                return;
            }
        }

        printf("Unknown target: %s\n", argv[i]);
        target_cli_print_usage(cli, argc, argv);

    }
}


void free_target_cli(TargetCli *cli) {
    for (int i = 0; i < cli->size; i++) {
        free(cli->target[i]);
    }
    free(cli->target);
    free(cli);
}



#endif //XBUILD_XBUILD_H
