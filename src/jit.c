#include <stdlib.h>

#define SLJIT_SINGLE_THREADED 1
// Black magic
#if defined(__CET__)
#undef __CET__
#endif
#include <sljit/sljitLir.h>

typedef void (*TestFn)();
struct TestContext;

extern void testOne(struct TestContext *context);

static void SLJIT_FUNC testOneWrapped(struct TestContext *context)
{
    testOne(context);
}

struct JitCode
{
    void *code;
    struct JitCode *next;
};

struct JitCode *jit_allocated_first = NULL;

void jit_done()
{
    struct JitCode *curr = jit_allocated_first;
    struct JitCode *prev = NULL;
    while (curr != NULL)
    {
        sljit_free_code(curr->code);
        prev = curr;
        curr = curr->next;
        free(prev);
    }
    jit_allocated_first = NULL;
}

TestFn jit_make_test(void* context)
{
  struct sljit_compiler *jit_compiler = sljit_create_compiler(NULL);
  struct JitCode* node = malloc(sizeof(struct JitCode));
  jit_compiler->verbose = 0;
	
	sljit_emit_enter(jit_compiler, 0,  0,  1, 1, 0, 0, 0);
  sljit_emit_op1(jit_compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, (sljit_sw)context);
  sljit_emit_icall(jit_compiler, SLJIT_CALL, SLJIT_ARG1(UW), SLJIT_IMM, SLJIT_FUNC_OFFSET(testOneWrapped));
  sljit_emit_return(jit_compiler, SLJIT_MOV, SLJIT_IMM, 0);

  node->next = jit_allocated_first;
  node->code = sljit_generate_code(jit_compiler);

  sljit_free_compiler(jit_compiler);

  return (void (*)()) node->code;
}
/////
