#include "buddy.h"

#define NULL ((void *)0)
#define PAGESIZE 4096
int   max_rank; // rank=0~ max_rank-1
void* start_place; // 空间分配起始地址
int* page_start;
int* page_rank;
int  current_pgcount;
struct free_list{
    int    block_start_place; // 起始位置
    struct free_list* next;
};
//空间大小为 2^(max_rank-1)
struct free_area{
    struct free_list list_head; // head_next是第一个大小为2^(rank-1)的空闲块
    int free_num; //空块的大小
    int* buddy_area_map; //由于每个块大小为2^(rank-1）一对伙伴块大小为2^rank 共有2^(max_rank-rank-1)对   状态为1则其中1个忙，用于合并
    int rank; //存储大小为2^（rank-1） 的页面
};
// free_a
struct free_area* zone; // 每个rank相关信息


void insert_area(struct free_area* block_list, int sp) {
    struct free_list* new_block = (struct free_list*)malloc(sizeof(struct free_list));
    new_block->block_start_place = sp;
    new_block->next = block_list->list_head.next;
    block_list->list_head.next = new_block;
    block_list->free_num++;
    block_list->buddy_area_map[sp >> (block_list->rank)] ^= 1;
}

void erase_area(struct free_area* block_list, int sp){
    struct free_list* new_block = &block_list->list_head;
    while(new_block->next->block_start_place != sp) new_block=new_block->next;
    struct free_list* tmp=new_block->next->next;
    free(new_block->next);
    new_block->next=tmp;
    block_list->free_num--;
    block_list->buddy_area_map[sp >> (block_list->rank)] ^= 1;
}


int init_page(void *p, int pgcount){
    start_place=p; 
    current_pgcount=pgcount;
    max_rank=1; while((1<<(max_rank-1))<pgcount) max_rank++;
    zone=(struct free_area*)malloc(sizeof(struct free_area)*max_rank);
    page_start=(int*)malloc(sizeof(int)*pgcount);
    page_rank=(int*)malloc(sizeof(int)*pgcount);
    //最初的时候都是free的
    for (int i=0;i<pgcount;i++) { page_start[i] = -1; page_rank[i]=max_rank; }
    for (int i=1;i<=max_rank;i++) {
        zone[i-1].rank=i;
        zone[i-1].free_num = 0;
        int t=max_rank-i-1;
        if (t<0) t=0;
        zone[i-1].buddy_area_map=malloc(sizeof(int)*(1<<t));
        for (int j = 0; j <(1<<(t)); j++) zone[i-1].buddy_area_map[j] = 0;
        zone[i-1].list_head.next = NULL;
    }
 /* printf("-----------------------------------------------------------------------------------------------------------------------\n");
    for (int j = 0; j <(1<<(max_rank-2)); j++) printf("%d ",zone[0].buddy_area_map[j]);
    printf("\n");
    
    printf("-----------------------------------------------------------------------------------------------------------------------\n");
    printf("%d\n",(1<<(max_rank-1))-1);*/
  //  p_array_map();
    struct free_list* tmp = (struct free_list*)malloc(sizeof(struct free_list));
    tmp->block_start_place=0;
    zone[max_rank-1].list_head.next=tmp;
    zone[max_rank-1].free_num=1;
    zone[max_rank-1].buddy_area_map[0]=1;
    return OK;
}

//未被分配的页按照最大的rank进行查询
void *alloc_pages(int rank){
    //printf("%d\n",1);
    if (rank < 1 || rank > max_rank) return (void*)-EINVAL;
    //printf("%d\n",max_rank);
    for (int i = rank; i <= max_rank; i++) 
    if (zone[i-1].free_num > 0) {
        //printf("%d\n",i);
        int current_start_place= zone[i-1].list_head.next->block_start_place;
        //printf("%d\n",current_start_place);
        void* rank_place=start_place + current_start_place * PAGESIZE;
        for (int j = 0; j < (1 << (rank-1)); j++) {
            page_start[j+current_start_place]=current_start_place;
            page_rank[j+current_start_place]=rank;
        }
        erase_area(&zone[i-1],current_start_place);
        current_start_place+=1<<(i-1);
        //因为最大的地址是pgcount（不一定是2的倍数？） 所以地址只能从前往后分配
        for (int j = i - 1; j >= rank; j--){
            current_start_place-=(1<<(j-1));
            insert_area(&zone[j-1], current_start_place);
        }
        return rank_place;
    }
    return (void*)-ENOSPC;
}

int return_pages(void *p){
    if (p == NULL) return -EINVAL;
    int offset=p - start_place;
    //printf("%d %d\n",offset, page_start[offset/PAGESIZE]);
    if (offset<0 || offset % PAGESIZE != 0 || offset >= current_pgcount*PAGESIZE || page_start[offset/PAGESIZE]==-1) return -EINVAL;
    int sp=page_start[offset/PAGESIZE];
    int rank=page_rank[sp];
    for (int j = 0; j < (1 <<(rank-1)); j++) {
        //printf("%d\n",j+sp);
        page_start[j+sp]=-1;
        page_rank[j+sp]=max_rank;
    }
    for (int i = rank; i <=max_rank; i++) {
        if (zone[i-1].buddy_area_map[sp>>i] == 1) {
            int next_sp=sp^(1<<(i-1));
            erase_area(&zone[i-1],next_sp);
            if (sp>next_sp) sp=next_sp;
        } else { insert_area(&zone[i-1], sp); break; }
    }
   // printf("%d\n",query_page_counts(1));
   // if (query_page_counts(1)>=16382) {  p_array_map();}
    return OK;
}
/*
void p_array_map() {
    for (int i=1;i<=2;i++) {
        int t=max_rank-i-1;
        if (t<0) t=0;
        for (int j=0;j<(1<<t);j++) printf("%lld ",zone[i-1].buddy_area_map[j]);
        printf("\n");
    }
}
*/
int query_ranks(void *p){
    if (p == NULL) return -EINVAL;
    int offset = p - start_place;
    if (offset < 0 || offset % PAGESIZE != 0 || offset >= current_pgcount*PAGESIZE) return -EINVAL;
    return page_rank[offset/PAGESIZE];
}

int query_page_counts(int rank){
    if (rank <1 || rank>max_rank) return -EINVAL;
    return zone[rank-1].free_num;
}
