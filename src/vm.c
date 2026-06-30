#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/vm.h"
#include "../include/alz_stdlib.h"

VM *vm_new() {
    VM *vm = calloc(1, sizeof(VM));
    return vm;
}

void vm_free(VM *vm) {
    for (size_t i = 0; i < vm->stack_top; i++)
        if (vm->stack[i]) alz_free(vm->stack[i]);
    for (size_t i = 0; i < vm->var_count; i++) {
        free(vm->vars[i].name);
        alz_free(vm->vars[i].val);
    }
    for (size_t i = 0; i < vm->global_count; i++) {
        free(vm->globals[i].name);
        alz_free(vm->globals[i].val);
    }
    free(vm);
}

void vm_push(VM *vm, AlzValue *val) {
    if (vm->stack_top >= ALZ_STACK_MAX) {
        fprintf(stderr, "alz: stack overflow\n");
        exit(1);
    }
    vm->stack[vm->stack_top++] = val;
}

AlzValue *vm_pop(VM *vm) {
    if (vm->stack_top == 0) return alz_null();
    return vm->stack[--vm->stack_top];
}

AlzValue *vm_peek(VM *vm, int offset) {
    if ((int)vm->stack_top - 1 - offset < 0) return alz_null();
    return vm->stack[vm->stack_top - 1 - offset];
}

void vm_set_var(VM *vm, const char *name, AlzValue *val) {
    for (size_t i = 0; i < vm->var_count; i++) {
        if (strcmp(vm->vars[i].name, name) == 0) {
            alz_free(vm->vars[i].val);
            vm->vars[i].val = val;
            return;
        }
    }
    if (vm->var_count >= ALZ_VARS_MAX) { fprintf(stderr, "alz: too many variables\n"); exit(1); }
    vm->vars[vm->var_count].name = strdup(name);
    vm->vars[vm->var_count].val  = val;
    vm->var_count++;
}

AlzValue *vm_get_var(VM *vm, const char *name) {
    for (size_t i = 0; i < vm->var_count; i++)
        if (strcmp(vm->vars[i].name, name) == 0)
            return vm->vars[i].val;
    return vm_get_global(vm, name);
}

void vm_set_global(VM *vm, const char *name, AlzValue *val) {
    for (size_t i = 0; i < vm->global_count; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            alz_free(vm->globals[i].val);
            vm->globals[i].val = val;
            return;
        }
    }
    if (vm->global_count >= ALZ_VARS_MAX) { fprintf(stderr, "alz: too many globals\n"); exit(1); }
    vm->globals[vm->global_count].name = strdup(name);
    vm->globals[vm->global_count].val  = val;
    vm->global_count++;
}

AlzValue *vm_get_global(VM *vm, const char *name) {
    for (size_t i = 0; i < vm->global_count; i++)
        if (strcmp(vm->globals[i].name, name) == 0)
            return vm->globals[i].val;
    return alz_null();
}

static VMResult vm_error(VM *vm, const char *msg) {
    snprintf(vm->error, sizeof(vm->error), "%s", msg);
    vm->had_error = 1;
    fprintf(stderr, "\n🔴  AlzScript Error\n────────────────────────────\n  %s\n────────────────────────────\n\n", msg);
    return VM_ERROR;
}

static char *interpolate(VM *vm, const char *fmt) {
    size_t out_cap = strlen(fmt) * 2 + 64;
    char  *out     = malloc(out_cap);
    size_t out_len = 0;
    out[0] = '\0';
    const char *p = fmt;
    while (*p) {
        if (*p == '{') {
            const char *end = strchr(p + 1, '}');
            if (!end) { out[out_len++] = *p++; continue; }
            size_t name_len = end - (p + 1);
            char  *var_name = strndup(p + 1, name_len);
            AlzValue *val   = vm_get_var(vm, var_name);
            free(var_name);
            char *s = alz_to_string(val);
            size_t slen = strlen(s);
            while (out_len + slen + 2 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
            memcpy(out + out_len, s, slen);
            out_len += slen;
            out[out_len] = '\0';
            free(s);
            p = end + 1;
        } else {
            if (out_len + 2 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
            out[out_len++] = *p++;
            out[out_len]   = '\0';
        }
    }
    return out;
}

VMResult vm_run(VM *vm, Chunk *chunk) {
    size_t ip = 0;

#define READ_INST()  (chunk->code[ip++])
#define CONST(idx)   (chunk->constants[idx])
#define NAME(idx)    (chunk->names[idx])
#define PUSH(v)      vm_push(vm, v)
#define POP()        vm_pop(vm)
#define PEEK(n)      vm_peek(vm, n)

    while (ip < chunk->count) {
        Instruction inst = READ_INST();

        switch (inst.op) {

            case OP_PUSH_NULL:   PUSH(alz_null());              break;
            case OP_PUSH_BOOL:   PUSH(alz_bool(inst.arg));      break;
            case OP_PUSH_NUM:    PUSH(alz_copy(CONST(inst.arg))); break;
            case OP_PUSH_STR: {
                int is_interp = (inst.arg >> 24) & 1;
                int idx       = inst.arg & 0xFFFFFF;
                if (is_interp) {
                    char *out = interpolate(vm, CONST(idx)->as.string);
                    PUSH(alz_string(out));
                    free(out);
                } else {
                    PUSH(alz_copy(CONST(idx)));
                }
                break;
            }
            case OP_POP: alz_free(POP()); break;

            case OP_STORE:
                vm_set_var(vm, NAME(inst.arg), POP());
                break;
            case OP_LOAD: {
                AlzValue *v = vm_get_var(vm, NAME(inst.arg));
                PUSH(alz_copy(v));
                break;
            }
            case OP_STORE_GLOBAL:
                vm_set_global(vm, NAME(inst.arg), POP());
                break;
            case OP_LOAD_GLOBAL: {
                AlzValue *v = vm_get_global(vm, NAME(inst.arg));
                PUSH(alz_copy(v));
                break;
            }

            case OP_ADD: {
                AlzValue *b = POP(), *a = POP();
                if (a->type == VAL_NUMBER && b->type == VAL_NUMBER) {
                    PUSH(alz_number(a->as.number + b->as.number));
                } else {
                    char *sa = alz_to_string(a), *sb = alz_to_string(b);
                    size_t len = strlen(sa) + strlen(sb) + 1;
                    char *res = malloc(len);
                    strcpy(res, sa); strcat(res, sb);
                    PUSH(alz_string(res));
                    free(sa); free(sb); free(res);
                }
                alz_free(a); alz_free(b);
                break;
            }
            case OP_SUB: { AlzValue *b=POP(),*a=POP(); PUSH(alz_number(a->as.number-b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_MUL: { AlzValue *b=POP(),*a=POP(); PUSH(alz_number(a->as.number*b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_DIV: { AlzValue *b=POP(),*a=POP(); if(b->as.number==0) return vm_error(vm,"Division by zero"); PUSH(alz_number(a->as.number/b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_MOD: { AlzValue *b=POP(),*a=POP(); PUSH(alz_number(fmod(a->as.number,b->as.number))); alz_free(a);alz_free(b); break; }
            case OP_POW: { AlzValue *b=POP(),*a=POP(); PUSH(alz_number(pow(a->as.number,b->as.number))); alz_free(a);alz_free(b); break; }
            case OP_NEG: { AlzValue *a=POP(); PUSH(alz_number(-a->as.number)); alz_free(a); break; }

            case OP_EQ: {
                AlzValue *b=POP(),*a=POP();
                int eq=0;
                if(a->type==VAL_NULL&&b->type==VAL_NULL) eq=1;
                else if(a->type==VAL_BOOL&&b->type==VAL_BOOL) eq=a->as.boolean==b->as.boolean;
                else if(a->type==VAL_NUMBER&&b->type==VAL_NUMBER) eq=a->as.number==b->as.number;
                else if(a->type==VAL_STRING&&b->type==VAL_STRING) eq=strcmp(a->as.string,b->as.string)==0;
                PUSH(alz_bool(eq)); alz_free(a);alz_free(b); break;
            }
            case OP_NEQ: { AlzValue *b=POP(),*a=POP(); int eq=(a->type==b->type)&&(a->type==VAL_NUMBER?a->as.number==b->as.number:a->type==VAL_STRING?strcmp(a->as.string,b->as.string)==0:0); PUSH(alz_bool(!eq)); alz_free(a);alz_free(b); break; }
            case OP_LT:  { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(a->as.number< b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_LTE: { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(a->as.number<=b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_GT:  { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(a->as.number> b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_GTE: { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(a->as.number>=b->as.number)); alz_free(a);alz_free(b); break; }
            case OP_AND: { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(alz_is_truthy(a)&&alz_is_truthy(b))); alz_free(a);alz_free(b); break; }
            case OP_OR:  { AlzValue *b=POP(),*a=POP(); PUSH(alz_bool(alz_is_truthy(a)||alz_is_truthy(b))); alz_free(a);alz_free(b); break; }
            case OP_NOT: { AlzValue *a=POP(); PUSH(alz_bool(!alz_is_truthy(a))); alz_free(a); break; }

            case OP_CONCAT: {
                AlzValue *b=POP(),*a=POP();
                char *sa=alz_to_string(a),*sb=alz_to_string(b);
                size_t len=strlen(sa)+strlen(sb)+1;
                char *res=malloc(len); strcpy(res,sa); strcat(res,sb);
                PUSH(alz_string(res));
                free(sa);free(sb);free(res);alz_free(a);alz_free(b);
                break;
            }

            case OP_MAKE_LIST: {
                AlzValue *list = alz_list();
                AlzValue **tmp = malloc(inst.arg * sizeof(AlzValue *));
                for (int i = inst.arg-1; i >= 0; i--) tmp[i] = POP();
                for (int i = 0; i < inst.arg; i++) alz_list_push(list->as.list, tmp[i]);
                free(tmp);
                PUSH(list);
                break;
            }
            case OP_MAKE_OBJ: {
                AlzValue *obj = alz_object();
                AlzValue **tmp = malloc(inst.arg * 2 * sizeof(AlzValue *));
                for (int i = inst.arg*2-1; i >= 0; i--) tmp[i] = POP();
                for (int i = 0; i < inst.arg; i++) {
                    char *key = alz_to_string(tmp[i*2]);
                    alz_obj_set(obj->as.object, key, tmp[i*2+1]);
                    free(key); alz_free(tmp[i*2]);
                }
                free(tmp);
                PUSH(obj);
                break;
            }
            case OP_LIST_PUSH: { AlzValue *val=POP(),*list=POP(); if(list->type==VAL_LIST) alz_list_push(list->as.list,val); PUSH(list); break; }
            case OP_LIST_LEN: {
                AlzValue *list=POP();
                if(list->type==VAL_LIST) PUSH(alz_number((double)list->as.list->count));
                else if(list->type==VAL_STRING) PUSH(alz_number((double)strlen(list->as.string)));
                else PUSH(alz_number(0));
                alz_free(list); break;
            }
            case OP_GET_INDEX: {
                AlzValue *idx=POP(),*obj=POP();
                if(obj->type==VAL_LIST&&idx->type==VAL_NUMBER) PUSH(alz_copy(alz_list_get(obj->as.list,(int)idx->as.number)));
                else if(obj->type==VAL_OBJECT){char *k=alz_to_string(idx);PUSH(alz_copy(alz_obj_get(obj->as.object,k)));free(k);}
                else PUSH(alz_null());
                alz_free(idx);alz_free(obj); break;
            }
            case OP_GET_PROP: {
                AlzValue *obj = POP();
                const char *key = NAME(inst.arg);
                if (obj->type == VAL_MODULE) {
                    char tag[256];
                    snprintf(tag, sizeof(tag), "__alzcall__%s__%s", obj->as.string, key);
                    alz_free(obj);
                    PUSH(alz_string(tag));
                } else if (obj->type == VAL_OBJECT) {
                    PUSH(alz_copy(alz_obj_get(obj->as.object, key)));
                    alz_free(obj);
                } else if (obj->type == VAL_LIST && strcmp(key,"length")==0) {
                    PUSH(alz_number((double)obj->as.list->count));
                    alz_free(obj);
                } else if (obj->type == VAL_STRING && strcmp(key,"length")==0) {
                    PUSH(alz_number((double)strlen(obj->as.string)));
                    alz_free(obj);
                } else {
                    alz_free(obj);
                    PUSH(alz_null());
                }
                break;
            }
            case OP_SET_PROP: {
                AlzValue *val=POP(),*obj=POP();
                if(obj->type==VAL_OBJECT) alz_obj_set(obj->as.object,NAME(inst.arg),val);
                PUSH(obj); break;
            }

            case OP_JUMP:          ip=(size_t)inst.arg; break;
            case OP_JUMP_IF_FALSE: { AlzValue *c=POP(); if(!alz_is_truthy(c)) ip=(size_t)inst.arg; alz_free(c); break; }
            case OP_JUMP_IF_TRUE:  { AlzValue *c=POP(); if(alz_is_truthy(c))  ip=(size_t)inst.arg; alz_free(c); break; }
            case OP_LOOP:          ip=(size_t)inst.arg; break;

            case OP_PRINT: {
                AlzValue *v = POP();
                alz_print(v);
                printf("\n");
                alz_free(v);
                break;
            }
            case OP_PRINT_FMT: {
                const char *fmt = CONST(inst.arg)->as.string;
                char *out = interpolate(vm, fmt);
                printf("%s\n", out);
                free(out);
                break;
            }
            case OP_ASK: {
                AlzValue *prompt = POP();
                char *p = alz_to_string(prompt);
                printf("%s", p);
                free(p); alz_free(prompt);
                char buf[1024];
                if (fgets(buf, sizeof(buf), stdin)) {
                    size_t len = strlen(buf);
                    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
                    PUSH(alz_string(buf));
                } else {
                    PUSH(alz_string(""));
                }
                break;
            }

            case OP_TO_NUM: { AlzValue *v=POP(); char *s=alz_to_string(v); PUSH(alz_number(atof(s))); free(s);alz_free(v); break; }
            case OP_TO_STR: { AlzValue *v=POP(); char *s=alz_to_string(v); PUSH(alz_string(s)); free(s);alz_free(v); break; }
            case OP_TO_BOOL: { AlzValue *v=POP(); PUSH(alz_bool(alz_is_truthy(v))); alz_free(v); break; }

            case OP_CALL: {
                int argc = inst.arg;
                AlzValue **args = malloc(argc * sizeof(AlzValue *));
                for (int i = argc-1; i >= 0; i--) args[i] = POP();
                AlzValue *callee = POP();

                if (callee && callee->type == VAL_STRING &&
                    strncmp(callee->as.string, "__alzcall__", 11) == 0) {
                    char tag[256];
                    strncpy(tag, callee->as.string + 11, sizeof(tag)-1);
                    tag[sizeof(tag)-1] = '\0';
                    char *sep = strstr(tag, "__");
                    if (sep) {
                        *sep = '\0';
                        AlzValue *result = stdlib_dispatch(vm, tag, sep+2, args, argc);
                        alz_free(callee);
                        for (int i = 0; i < argc; i++) alz_free(args[i]);
                        free(args);
                        PUSH(result ? result : alz_null());
                        break;
                    }
                }
                /* ── user-defined function ── */
                if (callee && callee->type == VAL_FUNCTION) {
                    AlzFunc *fn       = callee->as.func;
                    Chunk   *fn_chunk = (Chunk *)fn->chunk;

                    /* Save scope boundary */
                    size_t saved_vars = vm->var_count;

                    /* Bind args to params */
                    int n = argc < fn->param_count ? argc : fn->param_count;
                    for (int i = 0; i < n; i++)
                        vm_set_var(vm, fn->params[i], alz_copy(args[i]));
                    for (int i = n; i < fn->param_count; i++)
                        vm_set_var(vm, fn->params[i], alz_null());

                    /* Clear any previous return value */
                    vm_set_global(vm, "__return__", alz_null());

                    /* Run the function body */
                    vm_run(vm, fn_chunk);

                    /* Collect return value */
                    AlzValue *raw = vm_get_global(vm, "__return__");
                    AlzValue *ret = (raw && raw->type != VAL_NULL)
                                    ? alz_copy(raw) : alz_null();
                    vm_set_global(vm, "__return__", alz_null());

                    /* Restore scope */
                    while (vm->var_count > saved_vars) {
                        vm->var_count--;
                        free(vm->vars[vm->var_count].name);
                        alz_free(vm->vars[vm->var_count].val);
                    }

                    for (int i = 0; i < argc; i++) alz_free(args[i]);
                    free(args);
                    /* callee is VAL_FUNCTION shared with const pool — do not free */
                    PUSH(ret);
                    break;
                }

                /* Unknown callee */
                for (int i = 0; i < argc; i++) alz_free(args[i]);
                free(args);
                alz_free(callee);
                PUSH(alz_null());
                break;
            }
            case OP_RETURN: {
                /* Pop return value, store in __return__ global */
                AlzValue *ret = POP();
                vm_set_global(vm, "__return__", ret);
                return VM_OK;
            }
            case OP_RETURN_NULL:
                vm_set_global(vm, "__return__", alz_null());
                return VM_OK;

            case OP_HALT:
                return VM_HALT;

            case OP_NOP:
                break;

            default:
                fprintf(stderr, "alz: unknown opcode %d at ip=%zu\n", inst.op, ip-1);
                return VM_ERROR;
        }
    }

    return VM_OK;

#undef READ_INST
#undef CONST
#undef NAME
#undef PUSH
#undef POP
#undef PEEK
}
