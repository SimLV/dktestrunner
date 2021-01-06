#include <stdio.h>
#include <time.h>
#include <string.h>

#include <stdlib.h>
#include <stdarg.h>

#include "CUnit.h"
#include "Basic.h"

#include "compat.h"

#if defined(USE_CURSES) && USE_CURSES
#include <curses.h>
#endif

const int MAX_CHECKS = 8;
const int BUF_SIZE = 2048;

int DELAY = 5;
int START_DELAY = 45;
const char *end_turn_buf = "12000";
const char *frameskip = "2";
char *skip_events = NULL;
int print_all = 0;

#define LOGD(x, ...)

#ifdef _WIN32
#include <winsock2.h>
#include <direct.h>

#define getcwd _getcwd
#define chdir _chdir

#else

#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int SOCKET;

#define closesocket close
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

#endif


#define RE_MAX_GROUPS   10

// from compat.c
extern int wstartup();

extern void wcleanup();

extern void for_each_file(const char *folder, void (*fn)(const char *file, void *context), void *context);

extern int checkblock(int rval);

extern void make_nonblocking(SOCKET s);

extern int exec(char *cmd, ...);

void wait_app(PID_TYPE pid);

// from regex.c
int multi_match_start(const char *regexp, const char *data);

int multi_match_next();

const char *re_groups[RE_MAX_GROUPS];

extern int simple_match(const char *regexp, const char *data);

extern void re_cleanup();


SOCKET sock = INVALID_SOCKET;

int init(int port)
{
  struct sockaddr_in server;
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == INVALID_SOCKET)
    return 1;

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  if (bind(sock, (struct sockaddr *) &server, sizeof(server)) == SOCKET_ERROR)
    return 1;

  make_nonblocking(sock);
  return 0;
}

//////////

PID_TYPE dk_pid = 0;

//////////

typedef int (*ProcessFn)(const char *data_buf, void *context, void *addr, unsigned int addr_size);

int process_print(const char *data_buf, void *context, void *addr, unsigned int addr_size);

void process(ProcessFn fn, void *context);

//////////
typedef void (*TestFn)();

extern TestFn jit_make_test(void *context);

extern void jit_done();

enum TestCheckKind
{
    None = 0,
    Simple,
    Regexp,
    Assignment,
    WithVariable
};

struct TestVar
{
    struct TestVar *next;
    char *name;
    char *value;
};

struct TestContext
{
    struct TestContext *next;

    int good; // 1 if test succeeded
    int run; // 1 if we need to start another process

    const char *file_name;
    FILE *F;
    int first_line;
    int last_line;

    int cur_line;
    struct TestVar *first_var;

    char *check[RE_MAX_GROUPS];
    enum TestCheckKind check_kind[RE_MAX_GROUPS];
    char *check_var[RE_MAX_GROUPS];
    char line_buf[256];

};
struct TestContext *first_context = NULL;

void tst_clear()
{
  struct TestContext *old_context;
  for (struct TestContext *context = first_context; context != NULL;)
  {
    for (int i = 0; i < RE_MAX_GROUPS; i++)
    {
      if (context->check[i] != NULL)
        free(context->check[i]);
      if (context->check_var[i] != NULL)
        free(context->check_var[i]);
    }

    struct TestVar *old_node;
    for (struct TestVar *node = context->first_var; node != NULL;)
    {
      old_node = node;
      node = node->next;
      free(old_node);
    }

    old_context = context;
    context = context->next;

    free(old_context);
  }
  first_context = NULL;
}

struct TestContext *tst_create_context(const char *file_name, int line)
{
  // List should be in order of creation
  struct TestContext *node;
  if (first_context == NULL)
  {
    node = malloc(sizeof(struct TestContext));
    first_context = node;
  }
  else
  {
    node = first_context;
    while (node->next != NULL)
      node = node->next;

    node->next = malloc(sizeof(struct TestContext));
    node = node->next;
  }
  memset(node, 0, sizeof(struct TestContext));
  node->file_name = file_name;
  node->first_line = line;

  return node;
}

int tst_next_line(struct TestContext *context)
{
  char copy_buf[256];
  while (fgets(context->line_buf, 255, context->F) != NULL)
  {
    if ((context->last_line != 0) && (context->cur_line >= context->last_line))
    {
      fclose(context->F);
      context->F = NULL;
      return 0;
    }
    context->cur_line++;
    if (simple_match("^REM\\s+\\*\\s+(\\S+.*?)\\s*$", context->line_buf))
    {
      strcpy(copy_buf, re_groups[1]);
      int i = 0;
      for (multi_match_start("(\\S+)\\s*", copy_buf); multi_match_next(); i++)
      {
        if (i >= MAX_CHECKS)
          break;
        context->check[i] = strdup(re_groups[1]);
      }
      for (; i < MAX_CHECKS; i++)
      {
        context->check[i] = NULL;
      }
      for (int i = 0; i < MAX_CHECKS; i++)
      {
        if (context->check[i] == NULL)
        {
          context->check_kind[i] = None;
        }
        else if (simple_match("^/.*/$", context->check[i]))
        {
          context->check[i][strlen(context->check[i]) - 1] = 0; //remove tail
          context->check_kind[i] = Regexp;
        }
        else if (simple_match("^([[:alnum:]]+)=/(.+)/$", context->check[i]))
        {
          context->check_var[i] = strdup(re_groups[1]);
          char *tmp = strdup(re_groups[2]);
          free(context->check[i]);
          context->check[i] = tmp;
          context->check_kind[i] = Assignment;
        }
        else if (simple_match("\\{([[:alnum:]]+)\\}", context->check[i]))
        {
          context->check_kind[i] = WithVariable;
          struct TestVar *node = malloc(sizeof(struct TestVar));
          node->next = context->first_var;
          node->name = strdup(re_groups[1]);
          node->value = NULL;
          context->first_var = node;
        }
        else
        {
          context->check_kind[i] = Simple;
        }
      }
      return 1;
    }
  }
  fclose(context->F);
  context->F = NULL;
  return 0;
}

int tst_process_init(struct TestContext *start_context)
{
  int line;
  FILE *F;
  char *P;
  for (struct TestContext *context = start_context; context != NULL; context = context->next)
  {
    if (context->run == 1)
      break;
    line = 1;
    F = fopen(context->file_name, "rt");
    if (F == NULL)
    {
      fprintf(stderr, "Unable to open file %s\n", context->file_name);
      return 0;
    }
    while ((P = fgets(context->line_buf, 255, F)) != NULL)
    {
      line++;
      if (line == context->first_line)
        break;
    }
    if (P == NULL)
    {
      fprintf(stderr, "Unable to open seek %s to line %d\n", context->file_name, context->first_line);
      return 0;
    }
    context->F = F;
    context->cur_line = line;
    tst_next_line(context);
  }
  return 1;
}

int tst_process(const char *data_buf, void *context_, void *addr, unsigned int addr_size)
{
  struct TestContext *start_context = context_;
  int ok;
  if (print_all)
    printf("%s\n", data_buf);

  for (struct TestContext *context = start_context; context != NULL; context = context->next)
  {
    if (context->run == 1)
      break;
    if (context->F == NULL)
      continue;
    ok = 1;
    for (int i = 0; i < MAX_CHECKS; i++)
    {
      const char *check = context->check[i];
      if (context->check_kind[i] == Simple)
      {
        if (strstr(data_buf, check) == NULL)
        {
          ok = 0;
          break;
        }
      }
      else if (context->check_kind[i] == Regexp)
      {
        if (!simple_match(check + 1, data_buf))
        {
          ok = 0;
          break;
        }
      }
      else if (context->check_kind[i] == WithVariable)
      {
      }
      else if (context->check_kind[i] == Assignment)
      {
        if (!simple_match(check, data_buf))
        {
          ok = 0;
          break;
        }
        if (re_groups[1] == 0)
        {
          re_groups[1] = "";
        }
        for (struct TestVar *node = context->first_var; node != NULL; node = node->next)
        {
          if (strcmp(node->name, context->check_var[i]) == 0)
          {
            node->value = strdup(re_groups[1]);
            break;
          }
        }
      }
      else if (context->check_kind[i] == None)
      {
      }
      else
      {
        printf("usupported\n");
      }
    }
    if (ok)
    {
      if (!tst_next_line(context))
      {
        // test passed
        context->good = 1;
      }
    }
  }

  return 0;
}

void testOne(struct TestContext *context)
{
  if (context->run == 1)
  {
    context->run = 2;
    START_DELAY = 20;
    if (tst_process_init(context))
    {
      dk_pid = exec(
#ifndef _WIN32
          "wine",
#endif
          "keeperfx.exe",
          "-nointro",
          "-level", "1",
          "-tests",
          "-alex",
          "-ai_player",
          "-fixed_seed",
          "-monitoring", "127.0.0.1",
          "-exit_at_turn", end_turn_buf,
          "-frameskip", frameskip,
          (char *) NULL);
      if (dk_pid)
      {
        process(tst_process, context);
        wait_app(dk_pid);
      }
      else
      {
        CU_FAIL("unable to exec keeperfx.exe");
        return;
      }
    }
  }
  if (context->good)
  {
    CU_PASS("good");
  }
  else
  {
    CU_FAIL("not passed");
  }
}

void process_file(const char *file_name, void *context)
{
  CU_pSuite pSuite = NULL;
  char text_buf[256];
  char line_buf[256] = {0};
  const char *file_tail;
  struct TestContext *node = NULL;
  FILE *F;
  int line = 1;
  if (simple_match("/map\\d{5}\\.txt$", file_name))
  {
    F = fopen(file_name, "rt");
    if (F == NULL)
      return;
    file_tail = strrchr(file_name, '/');
    if (file_tail == NULL)
      file_tail = file_name;
    else
      file_tail++;
    while (fgets(line_buf, 255, F) != NULL)
    {
      line++;
      if (simple_match("^REM\\s+\\*\\*TEST\\*\\*\\s*(.+?)\\s*$", line_buf))
      {
        if (node) // stop previous node
        {
          node->last_line = line - 1;
        }
        if (pSuite == NULL)
        {
          pSuite = CU_add_suite(file_name, NULL, NULL);
          node = tst_create_context(pSuite->pName, line);
          node->run = 1;
        }
        else
        {
          node = tst_create_context(pSuite->pName, line);
        }

        if (re_groups[1])
          sprintf(text_buf, "%s:%d   %s", file_tail, line, re_groups[1]);
        else
          sprintf(text_buf, "%s:%d   <unnamed>", file_tail, line);
        CU_add_test(pSuite, text_buf, jit_make_test(node));
      }
    }
    fclose(F);
  }
}

int main(int argc, char **argv)
{
  const char *tests_path = "tests/levels";
  int wait_events = 1;

  for (int idx = 1; idx < argc; idx++)
  {
    if (strcmp(argv[idx], "--skip_events") == 0)
    {
      idx++;
      if (idx >= argc)
      {
        fprintf(stderr, "%s: option required\n", argv[idx - 1] + 2);
        return 1;
      }
      skip_events = argv[idx];
    }
    else if (strcmp(argv[idx], "--tests") == 0)
    {
      wait_events = 0;
    }
    else if (strcmp(argv[idx], "--delay") == 0)
    {
      idx++;
      if (idx >= argc)
      {
        fprintf(stderr, "%s: option required\n", argv[idx - 1] + 2);
        return 1;
      }
      DELAY = atoi(argv[idx]);
      if (DELAY < 1)
        DELAY = 1;
      START_DELAY = DELAY;
    }
    else if (strcmp(argv[idx], "--frameskip") == 0)
    {
      idx++;
      if (idx >= argc)
      {
        fprintf(stderr, "%s: option required\n", argv[idx - 1] + 2);
        return 1;
      }
      frameskip = argv[idx];
    }
    else if (strcmp(argv[idx], "--show_events") == 0)
    {
      print_all = 1;
    }
    else if (strcmp(argv[idx], "--exit_at_turn") == 0)
    {
      idx++;
      if (idx >= argc)
      {
        fprintf(stderr, "%s: option required\n", argv[idx - 1] + 2);
        return 1;
      }
      end_turn_buf = argv[idx];
    }
    else
    {
      printf("usage: %s\n"
             "\t--tests\t\t\t- run tests\n"
             "\t--exit_at_turn <num>\t\t- exit level at specific time (default 12000 turns)\n"
             "\t--frameskip <num>\t\t- set frameskip (default: 2)\n"
             "\t--show_events\t\t- print events while running tests\n"
             "\t--skip_events <regexp>\t- ignore events that match regexp\n",
             argv[0]);
      return 1;
    }
  }

  if (skip_events)
  {
    fprintf(stderr, "skipping events like /%s/\n", skip_events);
  }
  fprintf(stderr, "waiting for messages\n");
////////

  if (wstartup())
    return 1;

  if (init(8089))
  {
    if (sock != INVALID_SOCKET)
      closesocket(sock);
    wcleanup();
    fprintf(stderr, "failed!\n");
    return 1;
  }

  if (wait_events)
  {   //Simply reading data
    process(&process_print, NULL);
  }
  else
  {
    if (CU_initialize_registry())
    {
      fprintf(stderr, "Unable to initialize registry\n");
    }

    CU_BasicRunMode mode = CU_BRM_VERBOSE;
    CU_ErrorAction error_action = CUEA_IGNORE;

    CU_basic_set_mode(mode);
    CU_set_error_action(error_action);

    for_each_file(tests_path, &process_file, NULL);

    CU_basic_run_tests();
    CU_cleanup_registry();
    jit_done();
    tst_clear();
  }

  re_cleanup();
  fprintf(stderr, "done\n");
  wcleanup();
  return 0;
}

struct SourceRecord
{
    struct sockaddr_in addr;
    int id;
};

struct SourceRecord sources[8];
int num_sources = 0;

int cmp_records(const void *a_ptr, const void *b_ptr)
{
  const struct SourceRecord *obj_a = a_ptr;
  const struct SourceRecord *obj_b = b_ptr;
  return memcmp(&obj_a->addr, &obj_b->addr, sizeof(obj_a->addr));
}

int process_print(const char *data_buf, void *unused, void *addr, unsigned int addr_size)
{
  if (*data_buf == 0)
  {
    fprintf(stderr, "no data!\n");
    return 0;
  }
  struct SourceRecord *rec = NULL;
  if (num_sources != 0)
  {
    rec = bsearch(addr, sources, num_sources, sizeof(struct SourceRecord), &cmp_records);
  }
  if (rec == NULL)
  {
    memcpy(&sources[num_sources].addr, addr, addr_size);
    sources[num_sources].id = num_sources;
    rec = &sources[num_sources];
    num_sources++;
  }

  fprintf(stdout, "%d, %s", rec->id, data_buf);
  fflush(stdout);
  return 0;
}

void process(ProcessFn process_fn, void *context)
{
  char data_buf_[BUF_SIZE];
  char buf_copy_[BUF_SIZE];
  char *data_buf = data_buf_;
  char *buf_copy = buf_copy_;
  struct sockaddr_in from_buf;
  char *main_axis = NULL;
  char *tags = NULL;
  char *values = NULL;
  time_t fut_t = {0}, now_t = {0};
  unsigned int from_size = sizeof(from_buf);
  char *data_ptr;
  char *next_ptr;

  time(&fut_t);
  fut_t += START_DELAY;
  while (now_t < fut_t)
  {
    int read = recvfrom(sock, data_buf, BUF_SIZE, 0, (struct sockaddr *) &from_buf, &from_size);
    if (read == 0)
      break;
    else if (read < 0)
    {
      if (checkblock(read))
        break;
    }
    else if (read > 0)
    {
      data_buf[read] = 0;
      time(&fut_t);
      fut_t += DELAY;

      strcpy(buf_copy, data_buf);
      main_axis = buf_copy;
      {
        data_buf[read] = 0;
        time(&fut_t);
        fut_t += DELAY;

        data_ptr = data_buf;
        while (data_ptr != NULL)
        {
          next_ptr = strchr(data_ptr, '\n');
          if (next_ptr)
          {
            *next_ptr = 0;
            next_ptr++;
          }
          strcpy(buf_copy, data_ptr);
          main_axis = buf_copy;
          tags = strchr(buf_copy, ',');
          values = strchr(buf_copy, ' ');

          if (tags != NULL)
          {
            tags[0] = 0;
            tags++;
          }
          if (values != NULL)
          {
            values[0] = 0;
            values++;
          }

          if (data_ptr[0] != 0)
          {
            if ((skip_events == NULL) || (simple_match(skip_events, main_axis) == 0))
            {
              if (process_fn(data_ptr, context, &from_buf, from_size) == 1)
              {
                return;
              }
            }
          }
          data_ptr = next_ptr;
        }
      }
    }
    time(&now_t);
  }
}
