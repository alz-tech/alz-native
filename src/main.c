#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/compiler.h"
#include "../include/vm.h"
#include "../include/alz_stdlib.h"

static void print_usage() {
    printf("\n");
    printf("  AlzScript Native Runtime\n");
    printf("  alzc <file.az>          run a file\n");
    printf("  alzc --disasm <file.az> show bytecode\n");
    printf("  alzc --version          show version\n");
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }

    // --version
    if (strcmp(argv[1], "--version") == 0) {
        printf("AlzScript Native v1.0.0\n");
        return 0;
    }

    // --disasm <file>
    int disasm = 0;
    const char *path = argv[1];
    if (strcmp(argv[1], "--disasm") == 0) {
        if (argc < 3) { fprintf(stderr, "alzc: --disasm requires a file\n"); return 1; }
        disasm = 1;
        path = argv[2];
    }

    // Compile
    Chunk *chunk = compile_file(path);
    if (!chunk) return 1;

    if (disasm) {
        chunk_disasm(chunk);
        chunk_free(chunk);
        return 0;
    }

    // Execute
    VM *vm = vm_new();
    stdlib_register(vm);
    VMResult result = vm_run(vm, chunk);
    int exit_code = (result == VM_ERROR) ? 1 : 0;

    vm_free(vm);
    chunk_free(chunk);
    return exit_code;
}
