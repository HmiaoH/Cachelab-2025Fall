// student id: 2024202848
// please change the above line to your student id

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

void printSummary(int hits, int misses, int evictions)
{
  printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
  FILE *output_fp = fopen(".csim_results", "w");
  assert(output_fp);
  fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
  fclose(output_fp);
}

void printHelp(const char *name)
{
  printf(
      "Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
      "Options:\n"
      "  -h         Print this help message.\n"
      "  -v         Optional verbose flag.\n"
      "  -s <num>   Number of set index bits.\n"
      "  -E <num>   Number of lines per set.\n"
      "  -b <num>   Number of block offset bits.\n"
      "  -t <file>  Trace file.\n\n"
      "Examples:\n"
      "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
      "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
      name, name, name);
}

typedef struct
{
  int valid;
  unsigned long long tag;
  unsigned long long lru; 
  int dirty;
} line_t;

int main(int argc, char *argv[])
{
  int s = -1, E = -1, b = -1;
  char *trace_file = NULL;
  int verbose = 0;
  int opt;

  while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
  {
    switch (opt)
    {
    case 'h':
      printHelp(argv[0]);
      return 0;
    case 'v':
      verbose = 1;
      break;
    case 's':
      s = atoi(optarg);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 'b':
      b = atoi(optarg);
      break;
    case 't':
      trace_file = optarg;
      break;
    default:
      printHelp(argv[0]);
      return 1;
    }
  }

  if (s < 0 || E <= 0 || b < 0 || trace_file == NULL)
  {
    printHelp(argv[0]);
    return 1;
  }

  unsigned long long S = 1ULL << s;
  line_t *cache = (line_t *)malloc(sizeof(line_t) * S * E);
  if (!cache)
  {
    fprintf(stderr, "malloc failed\n");
    return 2;
  }
  for (unsigned long long i = 0; i < S * (unsigned long long)E; ++i)
  {
    cache[i].valid = 0;
    cache[i].tag = 0;
    cache[i].lru = 0;
    cache[i].dirty = 0;
  }

  FILE *fp = fopen(trace_file, "r");
  if (!fp)
  {
    fprintf(stderr, "Cannot open trace file: %s\n", trace_file);
    free(cache);
    return 1;
  }

  char linebuf[256];
  int hits = 0, misses = 0, evictions = 0;
  unsigned long long use_clock = 1;

  while (fgets(linebuf, sizeof(linebuf), fp) != NULL)
  {
    char op;
    unsigned long long addr;
    int size;
    if (sscanf(linebuf, " %c %llx,%d", &op, &addr, &size) < 1)
      continue;
    if (op == 'I')
      continue;

    int accesses = (op == 'M') ? 2 : 1;
    for (int a = 0; a < accesses; ++a)
    {
      unsigned long long set_idx = (addr >> b) & ((1ULL << s) - 1);
      unsigned long long tag = addr >> (s + b);
      unsigned long long base = set_idx * (unsigned long long)E;
      int hit_idx = -1;
      int empty_idx = -1;
      unsigned long long lru_min = (unsigned long long)(-1);
      int lru_idx = -1;
      for (int i = 0; i < E; ++i)
      {
        line_t *ln = &cache[base + i];
        if (ln->valid)
        {
          if (ln->tag == tag)
          {
            hit_idx = i;
            break;
          }
          if (ln->lru < lru_min)
          {
            lru_min = ln->lru;
            lru_idx = i;
          }
        }
        else
        {
          if (empty_idx == -1)
            empty_idx = i;
        }
      }

      if (hit_idx != -1)
      {
        hits++;
        cache[base + hit_idx].lru = use_clock++;
        if (op == 'S' || op == 'M')
          cache[base + hit_idx].dirty = 1;
        if (verbose)
        {
          printf("%c %llx,%d hit\n", op, addr, size);
        }
      }
      else
      {
        misses++;
        if (verbose)
        {
          printf("%c %llx,%d miss\n", op, addr, size);
        }
        int place = -1;
        if (empty_idx != -1)
        {
          place = empty_idx;
        }
        else
        {
          evictions++;
          place = lru_idx;
        }
        line_t *ln = &cache[base + place];
        ln->valid = 1;
        ln->tag = tag;
        ln->lru = use_clock++;
        ln->dirty = (op == 'S' || op == 'M') ? 1 : 0;
      }
    } 
  }

  fclose(fp);
  free(cache);

  printSummary(hits, misses, evictions);
  return 0;
}
