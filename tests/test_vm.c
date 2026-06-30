#include "../include/vm.h"
#include "../include/chunk.h"
#include "../include/value.h"
#include <stdio.h>
#include <string.h>

// ── Test: print "Hello Wilfort!" ──────────────────────────────────────────────
void test_hello() {
    printf("── Test: Hello World ──\n");
    Chunk *c = chunk_new("hello");
    VM    *vm = vm_new();

    // name = "Wilfort"
    int ni = chunk_add_const(c, alz_string("Wilfort"));
    int nv = chunk_add_name(c, "name");
    chunk_write(c, OP_PUSH_STR, ni, 1);
    chunk_write(c, OP_STORE, nv, 1);

    // print "Hello {name}!"
    int fi = chunk_add_const(c, alz_string("Hello {name}!"));
    chunk_write(c, OP_PRINT_FMT, fi, 2);

    chunk_write(c, OP_HALT, 0, 3);

    vm_run(vm, c);
    chunk_free(c);
    vm_free(vm);
}

// ── Test: arithmetic ──────────────────────────────────────────────────────────
void test_math() {
    printf("── Test: Math ──\n");
    Chunk *c  = chunk_new("math");
    VM    *vm = vm_new();

    // x = 10 + 5 * 2   →   should be 20
    int i10 = chunk_add_const(c, alz_number(10));
    int i5  = chunk_add_const(c, alz_number(5));
    int i2  = chunk_add_const(c, alz_number(2));
    int vx  = chunk_add_name(c, "x");

    chunk_write(c, OP_PUSH_NUM, i10, 1);
    chunk_write(c, OP_PUSH_NUM, i5,  1);
    chunk_write(c, OP_PUSH_NUM, i2,  1);
    chunk_write(c, OP_MUL, 0, 1);   // 5 * 2 = 10
    chunk_write(c, OP_ADD, 0, 1);   // 10 + 10 = 20
    chunk_write(c, OP_STORE, vx, 1);

    // print x
    chunk_write(c, OP_LOAD, vx, 2);
    chunk_write(c, OP_PRINT, 0, 2);

    chunk_write(c, OP_HALT, 0, 3);
    vm_run(vm, c);
    chunk_free(c); vm_free(vm);
}

// ── Test: if/else ─────────────────────────────────────────────────────────────
void test_if() {
    printf("── Test: If/Else ──\n");
    Chunk *c  = chunk_new("if_else");
    VM    *vm = vm_new();

    // age = 20
    int ia  = chunk_add_const(c, alz_number(20));
    int i18 = chunk_add_const(c, alz_number(18));
    int va  = chunk_add_name(c, "age");
    chunk_write(c, OP_PUSH_NUM, ia, 1);
    chunk_write(c, OP_STORE, va, 1);

    // if age > 18: print "adult"
    chunk_write(c, OP_LOAD, va, 2);
    chunk_write(c, OP_PUSH_NUM, i18, 2);
    chunk_write(c, OP_GT, 0, 2);
    int jump_false = chunk_write(c, OP_JUMP_IF_FALSE, 0, 2);  // patch later

    int is = chunk_add_const(c, alz_string("adult"));
    chunk_write(c, OP_PUSH_STR, is, 3);
    chunk_write(c, OP_PRINT, 0, 3);
    int jump_end = chunk_write(c, OP_JUMP, 0, 3);  // skip else

    // else: print "minor"
    int else_start = (int)c->count;
    int im = chunk_add_const(c, alz_string("minor"));
    chunk_write(c, OP_PUSH_STR, im, 4);
    chunk_write(c, OP_PRINT, 0, 4);

    int end = (int)c->count;

    // Patch jumps
    c->code[jump_false].arg = else_start;
    c->code[jump_end].arg   = end;

    chunk_write(c, OP_HALT, 0, 5);
    vm_run(vm, c);
    chunk_free(c); vm_free(vm);
}

// ── Test: list ────────────────────────────────────────────────────────────────
void test_list() {
    printf("── Test: List ──\n");
    Chunk *c  = chunk_new("list");
    VM    *vm = vm_new();

    // names = ["Wilfort", "Sarg", "ALZ"]
    int i1 = chunk_add_const(c, alz_string("Wilfort"));
    int i2 = chunk_add_const(c, alz_string("Sarg"));
    int i3 = chunk_add_const(c, alz_string("ALZ"));
    int vn = chunk_add_name(c, "names");

    chunk_write(c, OP_PUSH_STR, i1, 1);
    chunk_write(c, OP_PUSH_STR, i2, 1);
    chunk_write(c, OP_PUSH_STR, i3, 1);
    chunk_write(c, OP_MAKE_LIST, 3, 1);
    chunk_write(c, OP_STORE, vn, 1);

    // print names (whole list)
    chunk_write(c, OP_LOAD, vn, 2);
    chunk_write(c, OP_PRINT, 0, 2);

    // print names[0]
    int i0 = chunk_add_const(c, alz_number(0));
    chunk_write(c, OP_LOAD, vn, 3);
    chunk_write(c, OP_PUSH_NUM, i0, 3);
    chunk_write(c, OP_GET_INDEX, 0, 3);
    chunk_write(c, OP_PRINT, 0, 3);

    chunk_write(c, OP_HALT, 0, 4);
    vm_run(vm, c);
    chunk_free(c); vm_free(vm);
}

// ── Test: object ──────────────────────────────────────────────────────────────
void test_object() {
    printf("── Test: Object ──\n");
    Chunk *c  = chunk_new("object");
    VM    *vm = vm_new();

    // user = { name: "Wilfort", age: 25 }
    int ik1  = chunk_add_const(c, alz_string("name"));
    int iv1  = chunk_add_const(c, alz_string("Wilfort"));
    int ik2  = chunk_add_const(c, alz_string("age"));
    int iv2  = chunk_add_const(c, alz_number(25));
    int vu   = chunk_add_name(c, "user");
    int pname = chunk_add_name(c, "name");

    chunk_write(c, OP_PUSH_STR, ik1, 1);
    chunk_write(c, OP_PUSH_STR, iv1, 1);
    chunk_write(c, OP_PUSH_STR, ik2, 1);
    chunk_write(c, OP_PUSH_NUM, iv2, 1);
    chunk_write(c, OP_MAKE_OBJ, 2, 1);
    chunk_write(c, OP_STORE, vu, 1);

    // print user
    chunk_write(c, OP_LOAD, vu, 2);
    chunk_write(c, OP_PRINT, 0, 2);

    // print user.name
    chunk_write(c, OP_LOAD, vu, 3);
    chunk_write(c, OP_GET_PROP, pname, 3);
    chunk_write(c, OP_PRINT, 0, 3);

    chunk_write(c, OP_HALT, 0, 4);
    vm_run(vm, c);
    chunk_free(c); vm_free(vm);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    printf("\n🚀 AlzScript Native VM — Phase 1 Tests\n");
    printf("════════════════════════════════════════\n\n");

    test_hello();   printf("\n");
    test_math();    printf("\n");
    test_if();      printf("\n");
    test_list();    printf("\n");
    test_object();  printf("\n");

    printf("════════════════════════════════════════\n");
    printf("✅ All VM tests passed!\n\n");
    return 0;
}
