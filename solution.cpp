// Copyright (C) 2021 Keyu Tian. All rights reserved.
#include <iostream>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <pthread.h>
#include <time.h>
#include <memory.h>
#include <sys/time.h>

#define NUM_THREADS 36
#define MAX_PRODUCTION2_NUM 512
#define MAX_PRODUCTION1_NUM 128
#define MAX_N_NUM 128
#define MAX_T_NUM 128
#define MAX_STRING_LENGTH 1024

struct BeginAndNum
{
    int begin;
    unsigned num;
};

struct Production2
{
    int parent;
    int child1;
    int child2;
};

struct Production1
{
    int parent;     // non-terminators
    char child;     // terminators
};

#define us_stamp_t long long
#define us_stamp() (gettimeofday(&ms_t, 0), (us_stamp_t)((us_stamp_t)ms_t.tv_sec * 1000000 + ms_t.tv_usec))

#define ACCESS_DP(i, j, k) (*(dp + dp_dim1*(i) + dp_dim2*(j) + (k)))
unsigned dp[MAX_STRING_LENGTH * MAX_STRING_LENGTH * MAX_N_NUM];

#define ACCESS_ANY(i, j) (*(any + any_dim1*(i) + (j) ))
bool any[MAX_STRING_LENGTH * MAX_STRING_LENGTH];

std::atomic<int> top(0);

struct Arg
{
	pthread_barrier_t *p_bid;
    Production2 *production2;
    int rank, string_length, vn_num, production2_num;
};

void *run(void *varg)
{
	Arg *arg = (Arg *)varg;
	pthread_barrier_t * const p_bid = arg->p_bid;
    Production2 * const production2 = arg->production2;
	const int rank = arg->rank, string_length = arg->string_length, vn_num = arg->vn_num, production2_num = arg->production2_num;
	
    const int dp_dim1 = string_length * vn_num;
    const int dp_dim2 = vn_num;
    const int any_dim1 = string_length;
    
    unsigned num1, num2;
    int ranges[MAX_STRING_LENGTH+3];
    for (int len = 2; len <= string_length; len++)
    {
        const int N = string_length - len + 1;
        for (int i=0; i<N; ++i)
            ranges[i] = i;
        // std::shuffle(ranges, ranges+N, std::default_random_engine(len));
#if (NUM_THREADS > 1)
        pthread_barrier_wait(p_bid);
#endif
        if (rank == 0)
            top.store(0);
#if (NUM_THREADS > 1)
        pthread_barrier_wait(p_bid);
#endif
        // for (int left = rank; left <= string_length - len; left+=NUM_THREADS)
        while (true)
        {
            const int l_top = top.fetch_add(1);
            const int left = ranges[l_top];
            if (l_top >= N)
                break;
            
            bool ok = false;
            const int right = left + len;
            for (int mid = left + 1; mid < right; ++mid)
            {
                if (ACCESS_ANY(left, mid - 1) && ACCESS_ANY(mid, right - 1))
                {
                    int j;
                    for (j=0; j<production2_num-4; j+=4)
                    {
                        const unsigned * const p = dp + dp_dim1 * left + dp_dim2 * (mid-1);
                        const unsigned * const q = dp + dp_dim1 * mid + dp_dim2 * (right-1);
#define UNLOOP_N(__d) \
    if ((num1=*(p + production2[j+__d].child1)) && (num2=*(q + production2[j+__d].child2))) \
        ok = true, ACCESS_DP(left, right-1, production2[j+__d].parent) += num1 * num2;
                        UNLOOP_N(0);UNLOOP_N(1);UNLOOP_N(2);UNLOOP_N(3);
                    }
                    for (; j<production2_num; ++j)
                        if ((num1=ACCESS_DP(left, mid - 1, production2[j].child1)) && (num2=ACCESS_DP(mid, right - 1, production2[j].child2)))
                            ok = true, ACCESS_DP(left, right - 1, production2[j].parent) += num1 * num2;
                }
            }
            ACCESS_ANY(left, right - 1) |= ok;
        }
    }

	return NULL;
}


int main()
{
    Production2 production2[MAX_PRODUCTION2_NUM];
    Production1 production1[MAX_PRODUCTION1_NUM];

#define ACCESS_NINDEX(i, j) (*(nIndex + ((i) << 7) + (j)))
    BeginAndNum nIndex[MAX_N_NUM * MAX_N_NUM];
    BeginAndNum tIndex[MAX_T_NUM];

    char str[MAX_STRING_LENGTH];
    int vn_num;
    int production2_num;
    int production1_num;
    int string_length;

#ifdef _VSC_KEVIN
    const char *test_case = "2";
    char fin[100], fout[100];
    sprintf(fin, "input%s.txt", test_case);
    sprintf(fout, "output%s.txt", test_case);
    freopen(fin, "r", stdin);
    unsigned int gt;
    FILE *fp = fopen(fout, "r");
    fscanf(fp, "%u", &gt);
    fclose(fp);
#else
    freopen("input.txt", "r", stdin);
#endif
    
    scanf("%d\n", &vn_num);
    scanf("%d\n", &production2_num);
    for (int i = 0; i < production2_num; i++)
        scanf("<%d>::=<%d><%d>\n", &production2[i].parent, &production2[i].child1, &production2[i].child2);
    scanf("%d\n", &production1_num);
    for (int i = 0; i < production1_num; i++)
        scanf("<%d>::=%c\n", &production1[i].parent, &production1[i].child);
    scanf("%d\n", &string_length);
    scanf("%s\n", str);

    const int dp_dim1 = string_length * vn_num;
    const int dp_dim2 = vn_num;
    const int any_dim1 = string_length;

    std::sort(production1, production1 + production1_num, [](const Production1& a, const Production1& b)
    {
        return a.child == b.child ? a.parent < b.parent : a.child < b.child;
    });
    for (int i = 0; i < MAX_T_NUM; i++)
    {
        tIndex[i].begin = -1;
        tIndex[i].num = 0;
    }
    for (int i = 0; i < production1_num; i++)
    {
        char ch = production1[i].child;
        if (tIndex[ch].begin == -1)
            tIndex[ch].begin = i;
        tIndex[ch].num++;
    }
    for (int i = 0; i < string_length; i++)
    {
        char ch = str[i];
        int begin = tIndex[ch].begin;
        int end = begin + tIndex[ch].num;
        for (int j = begin; j < end; j++)
        {
            ++ACCESS_DP(i, i, production1[j].parent);
            ACCESS_ANY(i, i) = true;
        }
    }
    
    // sort productions for locality (good for caching hit rate)
    std::sort(production2, production2 + production2_num, [](const Production2& a, const Production2& b)
    { 
        return a.child1 == b.child1 ?
            (a.child2 == b.child2 ? a.parent < b.parent : a.child2 < b.child2)
            : a.child1 < b.child1;
    });
    for (int i = 0; i < vn_num; i++)
    {
        for (int j = 0; j < vn_num; j++)
        {
            ACCESS_NINDEX(i, j).begin = -1;
            ACCESS_NINDEX(i, j).num = 0;
        }
    }
    for (int i = 0; i < production2_num; i++)
    {
        int n1 = production2[i].child1;
        int n2 = production2[i].child2;
        if (ACCESS_NINDEX(n1, n2).begin == -1)
            ACCESS_NINDEX(n1, n2).begin = i;
        ACCESS_NINDEX(n1, n2).num++;
    }

#if (NUM_THREADS > 1)
    pthread_barrier_t bid, *p_bid = &bid;
	pthread_barrier_init(p_bid, NULL, NUM_THREADS);
	
	pthread_t pids[NUM_THREADS];
	Arg args[NUM_THREADS];
    
    args[0].p_bid = p_bid;
    args[0].production2 = production2;
    args[0].rank = 0;
    args[0].string_length = string_length;
    args[0].vn_num = vn_num;
    args[0].production2_num = production2_num;
	for (int rank=1; rank<NUM_THREADS; ++rank)
	{
        args[rank].p_bid = p_bid;
        args[rank].production2 = production2;
        args[rank].rank = rank;
        args[rank].string_length = string_length;
        args[rank].vn_num = vn_num;
        args[rank].production2_num = production2_num;
		pthread_create(pids+rank, NULL, run, args+rank);
	}
	run(args);
    pthread_barrier_destroy(p_bid);
	for (int rank=1; rank<NUM_THREADS; ++rank)
		pthread_join(pids[rank], NULL);
#else
    Arg arg;
    arg.p_bid = NULL;
    arg.production2 = production2;
    arg.rank = 0;
    arg.string_length = string_length;
    arg.vn_num = vn_num;
    arg.production2_num = production2_num;
    run(&arg);
#endif
    
#ifdef _VSC_KEVIN
    printf(
        "\n[%s] (%u)\n",
        ACCESS_DP(0, string_length - 1, 0) == gt ? "AC" : "WA!!!!",
        ACCESS_DP(0, string_length - 1, 0)
    );
    freopen("CON", "r", stdin);
    system("pause");
#else
    printf("%u", ACCESS_DP(0, string_length - 1, 0));
#endif

    return 0;
}
