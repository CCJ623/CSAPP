#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cachelab.h"

struct addressComponent
{
    int tag_;
    int set_index_;
};

struct Line
{
    int tag_;
    int last_used_time_;
};

struct Cache
{
    int set_bits;
    int number_of_sets_;
    int lines_per_set_;
    int block_bits;
    int block_size_;
    int timestamp_;
    struct Line* line_array_;

    struct
    {
        bool hit_;
        bool miss_;
        bool evict_;
    } status;
};

void error(const char* message)
{
    perror(message);
    exit(-1);
}

char getArgument(char* argv[], int index)
{
    char result;
    sscanf(argv[index], "-%c", &result);
    return result;
}

void initCache(struct Cache* cache)
{
    cache->number_of_sets_ = powl(2, cache->set_bits);
    cache->block_size_ = powl(2, cache->set_bits);
    int number_of_lines = cache->number_of_sets_ * cache->lines_per_set_;
    struct Line* ptr = malloc(number_of_lines * sizeof(struct Line));
    if (ptr == NULL)
    {
        error("Bad Allocation!");
    }
    cache->line_array_ = ptr;
    for (int i = 0; i != number_of_lines; ++i)
    {
        ptr[i].tag_ = -1;
        ptr[i].last_used_time_ = -1;
    }
}

void deallocateCache(struct Cache* cache)
{
    free(cache->line_array_);
}

struct addressComponent parseAddress(int address, int block_bits, int set_bits)
{
    struct addressComponent result;
    address >>= block_bits;
    result.set_index_ = address & (~(-1 << set_bits));
    result.tag_ = address >> set_bits;
    return result;
}

void readCache(struct Cache* cache, int address)
{
    ++(cache->timestamp_);
    struct addressComponent address_components = parseAddress(address, cache->block_bits, cache->set_bits);
    int set_begin = (address_components.set_index_ % cache->number_of_sets_) * cache->lines_per_set_;
    int set_end = set_begin + cache->lines_per_set_;
    struct Line* lru = &(cache->line_array_[set_begin]);
    // find Line
    for (; set_begin != set_end; ++set_begin)
    {
        struct Line* line = &(cache->line_array_[set_begin]);
        // hit
        if (line->tag_ == address_components.tag_)
        {
            line->last_used_time_ = cache->timestamp_;
            cache->status.hit_ = true;
            cache->status.miss_ = false;
            cache->status.evict_ = false;
            return;
        }
        // update lru
        if (line->last_used_time_ < lru->last_used_time_)
        {
            lru = line;
        }
    }
    // miss
    // set status
    cache->status.hit_ = false;
    cache->status.miss_ = true;
    cache->status.evict_ = (lru->last_used_time_ == -1 ? false : true);
    // update 
    lru->tag_ = address_components.tag_;
    lru->last_used_time_ = cache->timestamp_;
}

void writeCache(struct Cache* cache, int address)
{
    readCache(cache, address);
    return;
}

void updateRecord(const struct Cache* cache, long* hit_count,
    long* miss_count, long* eviction_count)
{
    (*hit_count) += cache->status.hit_;
    (*miss_count) += cache->status.miss_;
    (*eviction_count) += cache->status.evict_;
}

void printStatus(const struct Cache* cache)
{
    if (cache->status.miss_)
    {
        printf("miss");
        if (cache->status.evict_)
        {
            printf(" eviction");
        }
    }
    else
    {
        printf("hit");
    }
}

int main(int argc, char* argv[])
{
    // init
    bool enable_verbose = false;
    char* file_path;
    struct Cache cache;
    cache.timestamp_ = 0;
    for (int index = 1; index != argc;++index)
    {
        char arg = getArgument(argv, index);
        switch (arg)
        {
        case 'v':
            enable_verbose = true;
            break;
        case 't':
            file_path = argv[++index];
            break;
        case 's':
            sscanf(argv[++index], "%d", &(cache.set_bits));
            break;
        case 'E':
            sscanf(argv[++index], "%d", &(cache.lines_per_set_));
            break;
        case 'b':
            sscanf(argv[++index], "%d", &(cache.block_bits));
            break;
        default:
            error("Bad Arguments!");
        }
    }
    initCache(&cache);
    // deal with input
    char type;
    unsigned int address;
    int size;
    long hit_count = 0, miss_count = 0, eviction_count = 0;
    FILE* file = fopen(file_path, "r");
    char line[1024];
    while (fgets(line, 1024, file) != NULL)
    {
        sscanf(line, "%c", &type);
        // I instruction
        if (type == 'I')
        {
            continue;
        }
        // other instructions
        sscanf(line, " %c %x,%d", &type, &address, &size);
        if (enable_verbose)
        {
            printf("%c %x,%d ", type, address, size);
        }
        switch (type)
        {
        case 'M':
            readCache(&cache, address);
            updateRecord(&cache, &hit_count, &miss_count, &eviction_count);
            if (enable_verbose)
            {
                printStatus(&cache);
            }
            bool previous_is_hit = cache.status.hit_;
            writeCache(&cache, address);
            updateRecord(&cache, &hit_count, &miss_count, &eviction_count);
            // output hit only if previous is not hit
            if (enable_verbose && !previous_is_hit)
            {
                printf(" ");
                printStatus(&cache);
            }
            break;
        case 'L':
            readCache(&cache, address);
            updateRecord(&cache, &hit_count, &miss_count, &eviction_count);
            if (enable_verbose)
            {
                printStatus(&cache);
            }
            break;
        case 'S':
            writeCache(&cache, address);
            updateRecord(&cache, &hit_count, &miss_count, &eviction_count);
            if (enable_verbose)
            {
                printStatus(&cache);
            }
            break;
        default:
            error("Bad Instruction!");
            break;
        }
        printf("\n");
    }
    printSummary(hit_count, miss_count, eviction_count);
    deallocateCache(&cache);
    fclose(file);
    return 0;
}
