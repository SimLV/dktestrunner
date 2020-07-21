#include <string.h>
#include <stdio.h>
#include <pcre2.h>

#define RE_CACHE_SIZE   5
#define RE_MAX_GROUPS   10

struct ReCache
{
  const char *regexp;
  pcre2_code *re;
  long counter;
};

struct ReCache re_cache[RE_CACHE_SIZE] = {0};
long re_usage = 0;
PCRE2_SIZE *re_ovector;
char re_buffer[1024];
const char *re_groups[RE_MAX_GROUPS] = { NULL };

const char *re_multi_regexp = NULL;
const char *re_multi_data = NULL;
PCRE2_SIZE re_multi_start = 0;
PCRE2_SIZE re_multi_subject_length = 0;
int re_multi_options = 0;

pcre2_code *re_compile(const char* regexp)
{
    int rec = 0;
    long min_rec = LONG_MAX;
    int err = 0;
    PCRE2_SIZE err_ofs = 0;

    for (int i = 0; i < (sizeof(re_cache)/sizeof(re_cache[0])); i++)
    {
        if ((re_cache[i].regexp == regexp) 
            || ((re_cache[i].regexp != NULL) && (strcmp(re_cache[i].regexp, regexp) == 0))
            )
        {
            re_cache[i].counter = re_usage + 1;
            re_usage ++;
            return re_cache[i].re;
        }
        else if (re_cache[i].counter < min_rec)
        {
            rec = i;
            min_rec = re_cache[i].counter;
        }
    }

    pcre2_code *re = pcre2_compile((const unsigned char*)regexp, PCRE2_ZERO_TERMINATED, 0, &err, &err_ofs, NULL);
    if (re == NULL)
    {
        fprintf(stderr, "PCRE2 compilation failed '%s' at %ld\n", regexp, err_ofs);
        return NULL;
    }
    
    if (re_cache[rec].re != NULL)
    {
        pcre2_code_free(re_cache[rec].re);
    }
    re_cache[rec].re = re;
    re_cache[rec].counter = re_usage + 1;
    re_usage++;
    
    return re;
}

int multi_match_start(const char* regexp, const char *data)
{
    re_multi_regexp = regexp;
    re_multi_data = data;
    re_multi_start = 0;
    re_multi_options = 0;
    re_multi_subject_length = strlen(data);
    return 1;
}

int multi_match_next()
{
    if (re_multi_start == re_multi_subject_length)
      return 0;
    char err_buf[200];
    pcre2_code *re = re_compile(re_multi_regexp);

    if (re == NULL)
        return 0;
    pcre2_match_data *re_mdata = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (unsigned char*)re_multi_data, re_multi_subject_length,
        re_multi_start, re_multi_options, re_mdata, NULL);

    if (rc < 0)
    {
      if (rc != PCRE2_ERROR_NOMATCH)
      {
          err_buf[0] = 0;
          pcre2_get_error_message(rc, (unsigned char*) err_buf, sizeof(err_buf));
          fprintf(stderr, "PCRE2 match failed '%s' at %d (%s)\n", re_multi_regexp, rc, err_buf);
          pcre2_match_data_free(re_mdata);

          return 0;
      }
      else
      {
          pcre2_match_data_free(re_mdata);
          return 0;
      }
    }

    char *next_ptr = re_buffer;

    re_ovector = pcre2_get_ovector_pointer(re_mdata);
    for (int i = 0; i < rc; i++)
    {
        int len = re_ovector[2 *i + 1] - re_ovector[2 *i];
        if (len > 0)
        {
            if ((next_ptr + len - re_buffer) >= sizeof(re_buffer))
            {
                fprintf(stderr, "overflow\n");
                pcre2_match_data_free(re_mdata);
                return 0;
            }
            strncpy(next_ptr, re_multi_data + re_ovector[2*i], len);
            re_groups[i] = next_ptr;
            next_ptr[len] = 0;
            next_ptr += len + 1;
        }
        else
        {
            re_groups[i] = NULL; // Empty strings are not needed
        }
    }
    pcre2_match_data_free(re_mdata);
    if (re_ovector[1] == re_ovector[0])
    {
        re_multi_options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
    }
    else
    {
        re_multi_options = 0;
    }
    re_multi_start = re_ovector[1];
    return 1;
}

int simple_match(const char* regexp, const char *data)
{
    char err_buf[200];
    pcre2_code *re = re_compile(regexp);

    if (re == NULL)
        return 0;
    pcre2_match_data *re_mdata = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (unsigned char*)data, strlen(data), 
        0, 0, re_mdata, NULL);

    if (rc < 0)
    {
      if (rc != PCRE2_ERROR_NOMATCH)
      {
          err_buf[0] = 0;
          pcre2_get_error_message(rc, (unsigned char*) err_buf, sizeof(err_buf));
          fprintf(stderr, "PCRE2 match failed '%s' at %d (%s)\n", re_multi_regexp, rc, err_buf);
          pcre2_match_data_free(re_mdata);

          return 0;
      }
      else
      {
          pcre2_match_data_free(re_mdata);
          return 0;
      }
    }

    char *next_ptr = re_buffer;

    re_ovector = pcre2_get_ovector_pointer(re_mdata);
    for (int i = 0; i < rc; i++)
    {
        int len = re_ovector[2 *i + 1] - re_ovector[2 *i];
        if (len > 0)
        {
            if ((next_ptr + len - re_buffer) >= sizeof(re_buffer))
            {
                fprintf(stderr, "overflow\n");
                pcre2_match_data_free(re_mdata);
                return 0;
            }
            strncpy(next_ptr, data + re_ovector[2*i], len);
            re_groups[i] = next_ptr;
            next_ptr[len] = 0;
            next_ptr += len + 1;
        }
        else
        {
            re_groups[i] = NULL;
        }
    }
    pcre2_match_data_free(re_mdata);
    return 1;
}

void re_cleanup()
{
    for (int i = 0; i < (sizeof(re_cache)/sizeof(re_cache[0])); i++)
    {
        if (re_cache[i].re != NULL)
        {
            pcre2_code_free(re_cache[i].re);
            re_cache[i].re = NULL;
        }
    }
}
