/*
 * Source lru.c simulate LRU replacement algorithm.
 *
 *                                            Revised by : Song Jiang
 *					           sjiang@cs.wm.edu
 *                                            Revised at Nov 15, 2002
 */

#include "sim.h"


//main()
main(int argc, char* argv[])
{
  FILE *trc_fp, *cuv_fp, *para_fp;
  unsigned long i;
  char trc_file_name[100]; 
  char para_file_name[100];
  char cuv_file_name[100];
   
  
  /*int argc = 3;
  char argv[50][50];
  strcpy(argv[1], "2");
  strcpy(argv[2], "ps2"); 
  */
  
  if (argc != 2){
    printf("%s: file_name_prefix[.trc] \n", argv[0]);
    return;
  }
  

  strcpy(para_file_name, argv[1]);
  strcat(para_file_name, ".par");
  para_fp = openReadFile(para_file_name);
 

  strcpy(trc_file_name, argv[1]);
  strcat(trc_file_name, ".trc");
  trc_fp = openReadFile(trc_file_name);

  strcpy(cuv_file_name, argv[1]);
  strcat(cuv_file_name, "_LRU.cuv");
  cuv_fp = fopen(cuv_file_name, "w");

  get_range(trc_fp, &vm_size, &total_ref_pg);

  page_tbl = (page_struct *)calloc(vm_size+1, sizeof(page_struct));

  while (fscanf(para_fp, "%lu", &mem_size) != EOF){
    if (mem_size <= 0)
      break;

    printf("\nmem_size = %ld", mem_size);

    total_ref_pg = 0;
    num_pg_flt = 0;

    fseek(trc_fp, 0, SEEK_SET);

    free_mem_size = mem_size;

    /* initialize the page table */
    for (i = 0; i <= vm_size; i++){
      page_tbl[i].ref_times = 0;
      page_tbl[i].pf_times = 0; 

      page_tbl[i].page_num = i;
      page_tbl[i].isResident = 0; 

      page_tbl[i].LRU_next = NULL;
      page_tbl[i].LRU_prev = NULL;

      page_tbl[i].recency = vm_size+10;
    }

    LRU_list_head = NULL;
    LRU_list_tail = NULL;

    LRU_Repl(trc_fp);    


    printf("total_ref_pg = %d  num_pg_flt = %d \npf ratio = %2.1f\%, mem shortage ratio = %2.1f\% \n", total_ref_pg, num_pg_flt, (float)num_pg_flt/total_ref_pg*100, (float)(vm_size-mem_size)/vm_size*100);

    fprintf(cuv_fp, "%5d  %2.1f\n", mem_size, 100-(float)num_pg_flt/total_ref_pg*100);

  }

  return;
}


LRU_Repl(FILE *trc_fp)
{
  unsigned long ref_page;
  long last_ref_pg = -1;
  long num = 0;

  page_struct *temp_LRU_ptr; 

  fscanf(trc_fp, "%lu", &ref_page);

  while (!feof(trc_fp)){
    page_tbl[ref_page].ref_times++;

    total_ref_pg++;
 
    if (ref_page > vm_size){
      printf("Wrong ref page number found: %d.\n", ref_page);
      return FALSE;
    }

    if (ref_page == last_ref_pg){
      fscanf(trc_fp, "%lu", &ref_page);
      continue;
    }
    else
      last_ref_pg = ref_page;

    if (!page_tbl[ref_page].isResident) {  /* page fault */
      page_tbl[ref_page].pf_times++;
      num_pg_flt++;
      if (free_mem_size == 0){             /* free the LRU page */

	LRU_list_tail->isResident = 0;  

	temp_LRU_ptr = LRU_list_tail;
	
	LRU_list_tail = LRU_list_tail->LRU_prev;
	LRU_list_tail->LRU_next = NULL;

	temp_LRU_ptr->LRU_prev = NULL;
	temp_LRU_ptr->LRU_next = NULL;
	free_mem_size++;

      }
      page_tbl[ref_page].isResident = 1;
      free_mem_size--;
    }
    else {                                /* hit in memroy */
      if (page_tbl[ref_page].LRU_prev)
	page_tbl[ref_page].LRU_prev->LRU_next = page_tbl[ref_page].LRU_next;
      else /* hit at the stack top */
	LRU_list_head = page_tbl[ref_page].LRU_next; 
	

      if (page_tbl[ref_page].LRU_next)
	page_tbl[ref_page].LRU_next->LRU_prev = page_tbl[ref_page].LRU_prev;
      else /* hit at the stack bottom */
	LRU_list_tail = page_tbl[ref_page].LRU_prev;
    }

    page_tbl[ref_page].LRU_next = LRU_list_head;  /* place newly referenced page at head */

    page_tbl[ref_page].LRU_prev = NULL;

    LRU_list_head = (page_struct *)&page_tbl[ref_page]; 

    if (LRU_list_head->LRU_next)
      LRU_list_head->LRU_next->LRU_prev = LRU_list_head;

    if (!LRU_list_tail)
      LRU_list_tail = LRU_list_head;

    fscanf(trc_fp, "%lu", &ref_page);
    if (++num % 10000 == 0)
      fprintf(stderr, "%d samples processed\r", num);

  }

  printf("\n");
  return;
}


