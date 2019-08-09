#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

//#define DEBUG

#define ASSERTION_ENABLED

#define toggle(i) if(i==1)i=0;else{i=1;}

FILE *output_file;

//#define L 6
#define BIN_SIZE 32
#define HASHNO 999983
//#define LA_DIST 20
#define LA_DIST 100000

typedef long long hrtime_t;

/* get the number of CPU cycles per microsecond - from Linux /proc filesystem return<0 on error
 */
static double getMHZ_x86(void) {
  double mhz = -1;
  char line[1024], *s, search_str[] = "cpu MHz";
  FILE *fp; 
  
  /* open proc/cpuinfo */
  if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
    return -1;
  
  /* ignore all lines until we reach MHz information */
  while (fgets(line, 1024, fp) != NULL) { 
    if (strstr(line, search_str) != NULL) {
      /* ignore all characters in line up to : */
      for (s = line; *s && (*s != ':'); ++s);
      /* get MHz number */
      if (*s && (sscanf(s+1, "%lf", &mhz) == 1)) 
	break;
    }
  }
  
  if (fp!=NULL) fclose(fp);
  
  return mhz;
}

/* get the number of CPU cycles since startup */
static hrtime_t gethrcycle_x86(void) {
  unsigned int tmp[2];
  
  __asm__ ("rdtsc"
	   : "=a" (tmp[1]), "=d" (tmp[0])
	   : "c" (0x10) );
  
  return ( ((hrtime_t)tmp[0] << 32 | tmp[1]) );
}

static hrtime_t total_time;
static hrtime_t analysis_time;
static long long processing_times;

#define ADDR void*

typedef struct _access_entry {
  ADDR addr;
  ADDR ip;
} access_entry;
static access_entry buffer[LA_DIST];
static int buf_size;

static ADDR ip_array[2*LA_DIST];

static int hist[BIN_SIZE];
static int upper_boundary[BIN_SIZE];

static char* output_file_name = NULL;

struct hash_table {
        ADDR addr;
        long long grptime;
	long long prty;
        struct hash_table *nxt;
 };

/***************************************************************
Free list handlers
***************************************************************/

static struct hash_table *head_free_list;

UHT_Add_to_free_list (free_ptr)
struct hash_table *free_ptr;

{
   free_ptr->nxt = head_free_list;
   head_free_list = free_ptr;
}

struct hash_table
*UHT_Get_from_free_list ()

{  struct hash_table *free_ptr;

   if (head_free_list == NULL)
     return(NULL);
   else {
     free_ptr = head_free_list;
     head_free_list = head_free_list->nxt;
     return (free_ptr);
   }
}

 struct tree_node {
        ADDR addr;
	long long grpno;
	long long prty;
        long long rtwt;
        struct tree_node *lft, *rt;
 };  

/******************************************************************
A left rotation. Adapted from the Sleator and Tarjan paper on Splay trees
Makes use of p_stack, setup during the lookup.

Input: Index to entry at which rotation is to be done in p_stack.
Output: None
Side effects: Does a left rotation at the entry
******************************************************************/

rotate_left(y, p_stack)
int y;
struct tree_node **p_stack;

{   int x,z;

    
    z = y-1;
    x = y+1;
    if (z > 0)
         if (p_stack[z]->lft == p_stack[y])
              p_stack[z]->lft = p_stack[x];
         else
              p_stack[z]->rt = p_stack[x];
    p_stack[y]->rt = p_stack[x]->lft;
    p_stack[y]->rtwt -= p_stack[x]->rtwt + 1;
    p_stack[x]->lft = p_stack[y];
    p_stack[y] = p_stack[x];
    p_stack[x] = p_stack[x+1];
}


/******************************************************************
A right rotation. Adapted from the Sleator and Tarjan paper on Splay trees
Makes use of p_stack, setup during the lookup.

Input: Index to entry at which rotation is to be done in p_stack.
Output: None
Side effects: Does a right rotation at the entry
******************************************************************/

rotate_right(y, p_stack)
int y;
struct tree_node **p_stack;

{   int x,z;
    register struct tree_node *t1, *t2, *t3;

    z = y-1;
    x = y+1;
    t1 = p_stack[x];
    t2 = p_stack[y];
    t3 = p_stack[z];
    if (z>0)
         if (t3->lft == t2)
              t3->lft = t1;
         else
              t3->rt = t1;
    t2->lft = t1->rt;
    t1->rt = t2;
    t1->rtwt += t2->rtwt + 1;
    p_stack[y] = t1;
    p_stack[x] = p_stack[x+1];
}


/*********************************************************************
Adapted from the Sleator and Tarjan paper on Splay trees.
Splay the input entry to the top of the stack.
Makes use of p_stack setup during the lookup.

Input: Index to entry to be splayed to root in p_stack.
Output: None
Side effects: Does a spay on the tree
*********************************************************************/


splay(at, p_stack)
int at;
struct tree_node **p_stack;

{   int x, px, gx;
  
    x = at;
    px = at-1;
    gx = at-2;
 
 /* 'at' is a left child */

    if (p_stack[x] == p_stack[px]->lft)
         if (gx == 0)   /* zig */
              rotate_right(1, p_stack);
         else if (p_stack[px] == p_stack[gx]->lft){   /* zig-zig */
                   rotate_right(gx, p_stack);
                   rotate_right(gx, p_stack);
              }
              else{                               /* zig-zag */
                   rotate_right(px, p_stack);
                   rotate_left(gx, p_stack);
              }

 /* 'at' is a right child */

      else if (gx == 0)                              /* zig */
                rotate_left(1, p_stack);
      else if (p_stack[px] == p_stack[gx]->rt){         /* zig-zig */
               rotate_left(gx, p_stack);
               rotate_left(gx, p_stack);
          }
          else{                                   /* zig-zag */
               rotate_left(px, p_stack);
               rotate_right(gx, p_stack);
          }
}

struct ft_hash_table {
	ADDR addr;
	long long pt;
	struct ft_hash_table *nxt;
} ft_slot[HASHNO];

static short ft_slot_flag[HASHNO];	/* Hash table */

static ADDR addr_array[2*LA_DIST];	/* Trace input to stack_proc */
static long long time_array[2*LA_DIST];	/* Next time of arrival of addresses */

static long long la_limit,	/* Look-ahead limit in current iteration */
	       base,	/* Base time in current iteration */
	  oe_flag=1;   /* Whether top or bottom half of array */

static long long p_inum;	/* Address count */

/***************************************************************
Hashing routine. Adds addr to the hash table

Input: address
Output: -1 if addr is not found
        previous time of arrival if it is
Side effects: An entry for address is added to the hash table if address
	      is not found.
***************************************************************/

ft_hash(addr)
ADDR addr;	/* Address to be looked up */

{  int loc;
   int prev_time=0;
   struct ft_hash_table *ptr, *oldptr,
		*LA_Get_from_free_list();


   loc = ((unsigned long)addr) % HASHNO;
   if (ft_slot_flag[loc] == 0){
        /* ++tot_addrs;*/
	ft_slot_flag[loc] = 1;
        ft_slot[loc].addr = addr;
        ft_slot[loc].nxt = NULL;
	ft_slot[loc].pt = p_inum;
        return(-1);
   }


   else{
        ptr = &ft_slot[loc];
        while (ptr) {
            oldptr = ptr;
            if (ptr->addr == addr)
                 break;
            ptr = ptr->nxt;
        }
        if (ptr){
             prev_time = ptr->pt;
	     ptr->pt = p_inum;
             return(prev_time);
        }
        else{
             /*++tot_addrs;*/
	     if ((oldptr->nxt = LA_Get_from_free_list()) == NULL)
               oldptr->nxt = (struct ft_hash_table *) malloc(sizeof(struct ft_hash_table));
             oldptr->nxt->addr = addr;
             oldptr->nxt->nxt = NULL;
	     oldptr->nxt->pt = p_inum;
             return(-1);
        }
    }
}



/************************************************************************
Deletes addresses from the hash table. It is used when entries are deleted
from the stack due to stack overflow.

Input: Address
Output: None
Side effects: The entry for the address is deleted from the hash table
	      (The address need not necessarily be found, (e.g.) when
	       an address is referenced multiple times in the same window
	       and both references overflow).
*************************************************************************/

ft_hash_del (addr)
ADDR addr;

{  int loc;             /* Scratch variables */
   struct ft_hash_table *ptr, *oldptr;


   loc = ((unsigned long)addr) % HASHNO;
   if (ft_slot_flag[loc] == 0){
     printf ("Error: addr not found in hash_table\n");
   }
   else if (ft_slot[loc].addr == addr) {
     if (ft_slot[loc].nxt == NULL) {
       ft_slot_flag[loc] = 0;
     }
     else {
       ft_slot[loc].addr = ft_slot[loc].nxt->addr;
       ft_slot[loc].pt = ft_slot[loc].nxt->pt;
       ptr = ft_slot[loc].nxt;
       ft_slot[loc].nxt = ft_slot[loc].nxt->nxt;
       LA_Add_to_free_list (ptr);
     }
   }
   else{
        ptr = &ft_slot[loc];
        while (ptr) {
          if (ptr->addr == addr)
            break;
          oldptr = ptr;
          ptr = ptr->nxt;
        }
        if (ptr){
          oldptr->nxt = ptr->nxt;
          LA_Add_to_free_list (ptr);
        }
        else {
          printf ("Error: addr not found in hash_table\n");
        }
    }
}


/***************************************************************
Free list handlers
***************************************************************/

static struct ft_hash_table *head_free_list2;

LA_Add_to_free_list (free_ptr)
struct ft_hash_table *free_ptr;

{
   free_ptr->nxt = head_free_list2;
   head_free_list2 = free_ptr;
}

struct ft_hash_table
*LA_Get_from_free_list ()

{  struct ft_hash_table *free_ptr;

   if (head_free_list2 == NULL)
     return(NULL);
   else {
     free_ptr = head_free_list2;
     head_free_list2 = head_free_list2->nxt;
     return (free_ptr);
   }
}

static void record_distance(int dis) {
  int i = 0;
  for (;i<BIN_SIZE-1;i++) {
    if (dis <= upper_boundary[i]) {
      ++hist[i];
      return;
    }
  }
  ++hist[BIN_SIZE-1];
}

#define min(a,b) ((a<b) ? a : b)

#define ONE 1
#define MAX_PHYSICAL_MEM 2097152  /* Physical mem available on machine */
#define BYTES_PER_LINE 35  /* Storage requirement per line */

#define TREE_HEIGHT 50000  /* maximum height of tree */
#define CACHE_SIZE 50000  /* no of lines in cache */

 struct group_desc {
	struct tree_node *first;
	struct tree_node *last;
	int grpno;
	int wt;
	struct group_desc *nxt;
 };

static struct group_desc headgrp;	/* Head of group list */
static struct tree_node *root,	/* Root of tree */
		     **p_stack;	/* Stack used in tree operations */
static struct hash_table slot[HASHNO];	/* Hash table */
static short slot_flag [HASHNO];	/* Flag for hash slots */
static unsigned *out_stack;	/* Stack depth hits array */
static int comp=0, comp_del=0, comp_ins=0, comp_lid=0,
           no_splay_steps, tot_addrs, grps_passd, grps_coalesced;
static int t_entries;		/* Count of addresses processed */
static int max_priority; /* Priority number of max priority line */
static int ihcount;  /* To count no of time inf_handler called */
static int grp_ct = INT_MAX;  /* To number groups */
static short size_exceeded=0;		/* Stack overflow flag */
static int log_tot_addrs;
static unsigned next_two_pwr;
static unsigned unknowns;
static int CLEAN_INTERVAL,
           next_clean_time;
struct tree_node *Delete (struct tree_node *entry, struct tree_node **prev_entry);

unsigned MAX_CACHE_SIZE = 8388608;	/* Maximum cache size of interest */
unsigned MAX_LINES = 8388608;	/* Calculated from max cache size and line size */
int MISS_RATIO_INTERVAL = 512,	/* Output interval */
           SAVE_INTERVAL = 100000000,      	/* Interval at which results are saved */
           P_INTERVAL = 10000000;  /* Intervals at which progress output is done */

static int next_save_time;
FILE *out;

/**********************************************************************
Main simulation routine. Processes the addr_array and time_array obtained
from the pre-processing routine.

Input: The range in the addr_array (time_array) to be processed
Output: -1 if limit on the addresses to be processed is reached
	 0 otherwise
Side effects: The stack and group list are updated as the addresses are
	      preocessed.
**********************************************************************/

stack_proc_fa (start, end)
int start,	/* index of starting array location */
    end;	/* index of ending array location */

{
  int i, l;
   ADDR addr;
   long long priority, new_addrs;
   struct tree_node *nnode;

   //printf("start: %d\tend: %d\n", start, end);
   if (t_entries == 0) {
     root = (struct tree_node *) malloc(sizeof(struct tree_node));
     root->rt = root->lft = NULL;
     root->prty = Priority_fa (0);
     root->addr = addr_array [0];
     root->grpno = 0;

     l = 0;
     while ((addr_array [l] == addr_array [0]) && (l < end)) {
#ifdef DEBUG
       printf("-----No.%d-----\n", l);
       printf("addr: %p\n", (void*) addr_array[l]);
       if (l != 0)
	 printf("\tdepth: 1\n");
       else
	 printf("\tdepth: INF\n");
#endif
       if (l != 0) {
	 fprintf(output_file, "%p    %p    1\n", addr_array[l], ip_array[l]);
	 //record_distance(1);
       } else {
	 fprintf(output_file, "%p    %p    INF\n", addr_array[l], ip_array[l]);
	 //printf("addr: %p  ip: %p  dis: INF\n", addr_array[l], ip_array[l]);
	 //record_distance(INT_MAX);
       }
       ++l;
     }
     nnode = (struct tree_node *) malloc(sizeof(struct tree_node));
     nnode->rt = nnode->lft = NULL;
     priority = Priority_fa (l-1);
     nnode->prty = priority;
     nnode->addr = root->addr;
     if (nnode->prty < 0)
       unk_hash_add_fa (addr_array[l-1], INT_MAX, priority);
     nnode->grpno = INT_MAX;
     priority = Priority_fa (l);
     root->prty = priority;
     root->addr = addr_array [l];
     root->lft = nnode;
     headgrp.first = headgrp.last = root;
     headgrp.grpno = 0;
     headgrp.nxt = (struct group_desc *) malloc (sizeof (struct group_desc));
     headgrp.nxt->first = nnode;
     headgrp.nxt->last = nnode;
     headgrp.nxt->grpno = INT_MAX;
     headgrp.nxt->wt = 1;
     headgrp.nxt->nxt = NULL;
#ifdef DEBUG
     printf("-----No.%d-----\n", l);
     printf("addr: %p\n", (void*) addr_array[l]);
     printf("\tdepth: INF\n");
#endif
     fprintf(output_file, "%p    %p    INF\n", addr_array[l], ip_array[l]);
     //printf("addr: %p  ip: %p  dis: INF\n", addr_array[l], ip_array[l]);
     //record_distance(INT_MAX);

     out_stack [1] += l-1;
     tot_addrs = 2;
     log_tot_addrs = 1;
     next_two_pwr = 4;
     for (i=0; i<=l; i++) {
       addr_array [i] = 0;
       time_array [i] = 0;
     }
     start = l+1;
     t_entries = l+1;
   }

   //printf("start: %d\tend: %d\n", start, end);
   for (l=start; l<end; l++) {
#ifdef DEBUG
     printf("-----No.%d-----\n", l);
     printf("addr: %p\n", (void*) addr_array[l]);
#endif
	++t_entries;
	addr = addr_array [l];

	if (t_entries > next_save_time) {
	  outpr_facopt ();
	  next_save_time += SAVE_INTERVAL;
	}
	if (t_entries > next_clean_time) {
	  if (size_exceeded)
	    hash_clean_fa ();
	  next_clean_time += CLEAN_INTERVAL;
	}

	if ((t_entries % P_INTERVAL) == 0)
          printf("Addresses processed  %d\n", t_entries);

	priority = Priority_fa (l);

        if ((new_addrs = process_groups (addr, priority, ip_array[l])) == 1) {
	  ++tot_addrs;
	  if (tot_addrs > next_two_pwr) {
	    ++log_tot_addrs;
	    next_two_pwr *= 2;
	  }
	  if (size_exceeded == 0)
	    if (tot_addrs > MAX_LINES) {
	      toggle(size_exceeded);
              printf ("tot_addrs limit exceeded  t_entries=%d\n", t_entries);
	    }
        }
	addr_array [l] = 0;
	time_array [l] = 0;
        if (t_entries > INT_MAX) {
	  return(1);
	}
   } /* for */
   return (0);
}

/*********************************************************************
Initialization routine. Creates a root for the splay tree
and allocates space for the arrays.

Input: None
Output: None
Side effects: Creates a root for the tree. Adds the address to the splay tree.
*********************************************************************/

void init_facopt ()

{

      CLEAN_INTERVAL = 10000000;
      next_save_time = SAVE_INTERVAL;

      out_stack = (unsigned *) calloc (MAX_LINES, sizeof (unsigned));
                        /* Stack is not cut off precisely at MAX_LINES */
      p_stack = (struct tree_node **) calloc ((MAX_LINES+1000), sizeof (struct tree_node *));

      if ((out_stack == NULL) || (p_stack == NULL))
        goto error;

      next_clean_time = CLEAN_INTERVAL;

      return;
error:
      fprintf (stderr, "Problem allocating space (in init)\n");
      exit (1);
}





/**********************************************************
  Does the stack processing for each address in the trace for
  OPT replacement.

  The stack is maintained as a tree. A list of groups constituting
  the stack is maintained. This list is traversed by the procedure.
  It calls special tree handling routines to perform some stack operations
  and to maintain the group list.

  Input: address referenced and its priority(a fn of its time of next refn)
  Output: none
  Side effects: Updates the stack depth hit array
		Updates the group list and the tree.
**********************************************************/

process_groups (addr, priority, ip)
ADDR addr;	/* Address referenced */
long long priority;	/* New priority of address */
ADDR ip;
{  int depth;		/* Stack depth counter */
   struct group_desc *grpptr,		/* Scratch pointer */
		  *oldgrpptr,
		  *newgrpptr;
   struct tree_node *Lookup_next (),		/* Tree handling functions */
		    *Lookup_prev (),
	   *Lookup_Delete_Insert (),
		         *Delete (),
			*prev_entry,
		         *grpsecond;
   static struct tree_node dummy_for_del_line;
	/* One extra node is allocated for del_line which changes */
   static struct tree_node *del_line = &dummy_for_del_line;

    /* Hit at top */
  if (headgrp.first->addr == addr) {
#ifdef DEBUG 
    printf("\tdepth: 1\n");
#endif
    fprintf(output_file, "%p    %p    1\n", addr, ip);
    //printf("addr: %p  ip: %p  dis: 1\n", addr, ip);
    //record_distance(1);
    ++ out_stack [1];
    headgrp.first->prty = priority;
    headgrp.first->addr = addr;
  }
  else {
     /* Depth two hit */
    grpptr = headgrp.nxt;
    ++grps_passd;

	/* For inf_handler */
    if (headgrp.first->prty < 0) {
	/* A kludge to set grptime correctly on a depth two hit */
      if (headgrp.nxt->first->addr == addr)
        unk_hash_add_fa (headgrp.first->addr, (headgrp.nxt->grpno-1), headgrp.first->prty);
      else
        unk_hash_add_fa (headgrp.first->addr, headgrp.nxt->grpno, headgrp.first->prty);
    }

    if (grpptr->first->addr == addr) {
#ifdef DEBUG
      printf("\tdepth: 2\n");
#endif
      fprintf(output_file, "%p    %p    2\n", addr, ip);
      //printf("addr: %p  ip: %p  dis: 2\n", addr, ip);
      //record_distance(2);
      ++out_stack [2];

      if (grpptr->wt == 1) {
	/* One entry in first group */
        grpptr->first->addr = headgrp.first->addr;
	grpptr->first->prty = headgrp.first->prty;
      }
      else {
	grpsecond = Lookup_next (grpptr->first);
	if (grpsecond->grpno != grpptr->grpno)
	  printf ("Error: Inconsistent next entry\n");
	if (grpsecond->prty < headgrp.first->prty) {
	/* Coalescing. new group not needed */
          grpptr->first->addr = headgrp.first->addr;
          grpptr->first->prty = headgrp.first->prty;
	}
	else {

	  --grp_ct;

       /* A new group is created */
	  newgrpptr = (struct group_desc *) malloc (sizeof (struct group_desc)) ;
	  newgrpptr->first = grpptr->first;
	  newgrpptr->last = grpptr->first;
	  newgrpptr->grpno = grp_ct;
	  newgrpptr->wt = 1;
	  newgrpptr->first->addr = headgrp.first->addr;
	  newgrpptr->first->prty = headgrp.first->prty;
	  newgrpptr->first->grpno = grp_ct;
	  newgrpptr->nxt = grpptr;
	  headgrp.nxt = newgrpptr;

	/* adjust next group */
          grpptr->first = grpsecond;
	  grpptr->wt -= 1;
        }
      }
      headgrp.first->addr = addr;
      headgrp.first->prty = priority;
    }
    else {
	/* Hit at second or higher group */
      del_line->prty = headgrp.first->prty;
      del_line->addr = headgrp.first->addr;
      headgrp.first->addr = addr;
      headgrp.first->prty = priority;

      if (del_line->prty > grpptr->last->prty) {
	/* del_line enters group. last entry bcomes del_line */
	del_line->grpno = grpptr->grpno;
	Insert (del_line);
	if (del_line->prty > grpptr->first->prty)
	  grpptr->first = del_line;
	del_line = Delete (grpptr->last, &prev_entry);
	grpptr->last = prev_entry;
	if (grpptr->last->grpno != grpptr->grpno)
	  printf ("Error: Inconsistent prev entry\n");
      }

      oldgrpptr = grpptr;
      depth = grpptr->wt + 1;
      grpptr = grpptr->nxt;
   
      while (grpptr != NULL) {
	++grps_passd;
        if (grpptr->first->addr == addr) { /* Hit */
	  if ((depth+1) <= MAX_LINES) {
#ifdef DEBUG
	    printf("\tdepth: %d\n", depth+1);
#endif
	    fprintf(output_file, "%p    %p    %llu\n", addr, ip, depth+1);
	    //printf("addr: %p  ip: %p  dis: %llu\n", addr, ip, depth+1);
	    //record_distance(depth+1);
	    ++out_stack[depth+1];
	  }
	  del_line->grpno = oldgrpptr->grpno;
	  oldgrpptr->last = Lookup_Delete_Insert (grpptr->first, del_line);
	  oldgrpptr->wt += 1;

          if (grpptr->first == grpptr->last) {
	/* delete group if it had only the refned entry */
	    oldgrpptr->nxt = grpptr->nxt;
            free (grpptr);
	    grpptr = oldgrpptr; /* grpptr should not be NULL */
	  }
          else {
	/* else set group-first to next entry */
            grpptr->first  = Lookup_next(grpptr->first);
	    grpptr->wt -= 1;
	    if (grpptr->first->grpno != grpptr->grpno)
	      printf ("Error: Inconsistent next entry\n");
	  }
	  break;
  
        }
	else if (del_line->prty > grpptr->last->prty) {
	/* del_line enters group. last entry bcomes del_line */
	  del_line->grpno = grpptr->grpno;
	  Insert (del_line);
	  if (del_line->prty > grpptr->first->prty)
	    grpptr->first = del_line;
	  del_line = Delete (grpptr->last, &prev_entry);

	  grpptr->last = prev_entry;
	  if (grpptr->last->grpno != grpptr->grpno)
	    printf ("Error: Inconsistent prev entry\n");
	}
	oldgrpptr = grpptr;
	depth += grpptr->wt;
	grpptr = grpptr->nxt;
      } /* while */
     } /* else */

     if (grpptr == NULL) {
	/* refned address is new (compulsory miss)  */
	/* If stack has overflowed, del_line is not added to the end
	   of the stack. Also, if del_line is an unknown, it is deleted
	   from the ft_hash_table as well. Unknowns are deleted from
	   the unknown hash table only during the cleaning step */
       if (size_exceeded) {
	 if (del_line->prty < 0) {
	   ft_hash_del (del_line->addr);
	 }
       }
       else {
         del_line->grpno = oldgrpptr->grpno;
         Insert (del_line);
         oldgrpptr->last = del_line;
         oldgrpptr->wt += 1;
         del_line = (struct tree_node *) malloc(sizeof(struct tree_node));
       }
#ifdef DEBUG
       printf("\tdepth: INF\n");
#endif
       fprintf(output_file, "%p    %p    INF\n", addr, ip);
       //printf("addr: %p  ip: %p  dis: INF\n", addr, ip);
       //record_distance(INT_MAX);
       //printf("hist[31]=%d\n", hist[BIN_SIZE-1]);
       return (1);
     }
   } /* else */
   return (0);
} /*fn*/



/********************************************************************
Given the index of the address in the address array it determines the
priority of the address using the time_array.

Input: Index of current address in the addr_array.
Output: The priority of the current address
Side effects: None
********************************************************************/

Priority_fa (i)
int i;

{  static int inf_count=0;

   if (time_array [i] > 0)
     return (INT_MAX - time_array [i]); /* inf_handler assumes this fn.
			 inf_handler has to be changed if this is changed */
   else if (time_array [i] == 0)
     return (--inf_count);
   else {
     printf ("Error in Priority function\n");
     return (0);
   }
}

/********************************************************************
When an unknown is referenced this routine moves it to its right position
in the stack and rearranges the other unknowns as required

Input: Address and the current time of the pre-processing routine
Output: None
Side effects: The stack is rearranged and the group list is updated
	      if required.
*********************************************************************/

void inf_handler_fa (addr, cur_time)
ADDR addr;
  long long cur_time;
{
  struct tree_node *line_to_be_ins, /* scratch pointers */
		    *line_to_be_del,
			*prev_entry,
	      *Get_first_unknown_wobacking ();
   struct group_desc *grpptr;	/* scratch */
   long long  addr_grptime,		/* group time marker for unknown */
          addr_prty;
   long long priority,	/* fn of cur_time */
       del_prty;	/* prty of deleted entry (during rearrangement) */
   static struct tree_node dummy_for_del_line;
	/* One extra node is allocated for del_line which changes */
   static struct tree_node *del_line = &dummy_for_del_line;


   ++ihcount;

   priority = INT_MAX - cur_time; /* assumes a priority function */

   if ((headgrp.first != NULL) && (headgrp.first->addr == addr)) {
     if (headgrp.first->prty > 0)
       printf ("Error: unknown has > 0 prty\n");
     else
       headgrp.first->prty = priority;
     return;
   }

   addr_grptime = unk_hash_del_fa (addr, &addr_prty);
   if (addr_grptime == 0)
     return;

   --unknowns;

   grpptr = headgrp.nxt; /*assumed that a unknown is not fixed
			  when it is at the top */

/*printf ("inf_handler addr: %d %d\n", addr, addr_grptime);
traverse(root);*/

   while (grpptr)
     if (grpptr->grpno < addr_grptime)
       grpptr = grpptr->nxt;
     else
       break;

/*   if (grpptr == NULL)
     printf ("Error in inf_handler: addr_grptime not found\n"); */

   while (grpptr)
     if (grpptr->last->prty > 0)
       grpptr  = grpptr->nxt;
     else
       break;

/*   if (grpptr == NULL)
     printf ("Error in inf_handler: no unknowns found\n"); */

   line_to_be_ins = del_line;
   line_to_be_ins->prty = priority;
   line_to_be_ins->addr = addr;

   del_prty = 0;
   while (grpptr) {
     line_to_be_del = Get_first_unknown_wobacking (grpptr->grpno, addr_prty, del_prty);
     if (line_to_be_del != NULL) {
       line_to_be_ins->grpno = grpptr->grpno;
       Insert (line_to_be_ins);
       if (line_to_be_ins->prty > grpptr->first->prty)
	 grpptr->first = line_to_be_ins;
       line_to_be_ins = Delete (line_to_be_del, &prev_entry);
       if (grpptr->last == line_to_be_del)
	 grpptr->last = prev_entry;
       if (line_to_be_del->prty == addr_prty)
         break;
       del_prty = line_to_be_ins->prty;
     }
     grpptr = grpptr->nxt;
   }
   del_line = line_to_be_ins;

/*   if (grpptr == NULL)
     printf ("Error in inf_handler: unknown not found\n"); */
}

/**********************************************************************
Used to add infinities to a hash table. The hash table is used by inf_handler
to get the dummy priority of the unknown and its grptime.

Input: The address, the current group number of the first group and
       the dummy prty of the address.
Output: None
Side effects: The address is added to the hash table
**************************************************************************/

unk_hash_add_fa (addr, grpno, prty)
ADDR addr;
long long grpno,
    prty;

{  int loc;
   struct hash_table *ptr, *oldptr;
   struct hash_table *UHT_Get_from_free_list ();

   ++unknowns;

   loc = ((unsigned long)addr) % HASHNO;
   if (slot_flag [loc] == 0){
	slot_flag [loc] = 1;
        slot[loc].addr = addr;
        slot[loc].grptime = grpno;
	slot[loc].prty = prty;
        slot[loc].nxt = NULL;
        return(0);
   }


   else{
        ptr = &slot[loc];
        while (ptr) {
            oldptr = ptr;
            if (ptr->addr == addr) {
              printf ("Error: addr already in hash table\n"); 
printf ("addr: %d; t_entries: %d; prty: %d\n", addr, t_entries, ptr->prty);
printf ("slot addr: %d; prty: %d\n", slot[loc].addr, slot[loc].prty);
}
            ptr = ptr->nxt;
        }
	if ((oldptr->nxt = UHT_Get_from_free_list ()) == NULL)
          oldptr->nxt = (struct hash_table *) malloc(sizeof(struct hash_table));
        oldptr->nxt->addr = addr;
        oldptr->nxt->nxt = NULL;
        oldptr->nxt->grptime = grpno;
        oldptr->nxt->prty = prty;
        return(0);
    }
}




/*************************************************************************
Deletes unknowns from the hash table. Returns the grptime and the dummy
priority of the address.

Input: Address
Outpur: The grptime and the dummy priority
Side effects: The entry is deleted from the hash table
*************************************************************************/

unk_hash_del_fa (addr, prty_ptr)
ADDR addr;
long long *prty_ptr;

{  int loc,
     grpno;
   struct hash_table *ptr, *oldptr;


   loc = ((unsigned long)addr) % HASHNO;
   if (slot_flag[loc] == 0){
     return (0);
   }
   else if (slot[loc].addr == addr) {
     if (slot[loc].nxt == NULL) {
       slot_flag[loc] = 0;
       *prty_ptr = slot[loc].prty;
       return (slot[loc].grptime);
     }
     else {
       grpno = slot[loc].grptime;
       *prty_ptr = slot[loc].prty;
       slot[loc].addr = slot[loc].nxt->addr;
       slot[loc].grptime = slot[loc].nxt->grptime;
       slot[loc].prty = slot[loc].nxt->prty;
       ptr = slot[loc].nxt;
       slot[loc].nxt = slot[loc].nxt->nxt;
       UHT_Add_to_free_list (ptr);
       return (grpno);
     }
   }
   else{
        ptr = &slot[loc];
        while (ptr) {
          if (ptr->addr == addr)
            break;
          oldptr = ptr;
          ptr = ptr->nxt;
        }
        if (ptr){
          grpno = ptr->grptime;
	  *prty_ptr = ptr->prty;
	  oldptr->nxt = ptr->nxt;
	  UHT_Add_to_free_list (ptr);
	  return (grpno);
        }
        else {
     	  return (0);
        }
    }
}


/*********************************************************************
Inorder traversal using a stack.
Adapted from Harry Smith (page 214)

Starts at the bottom most entry of a group and searches upward for
the first node such that
1. It is that of the referenced address
2. It has a priority less than the del_prty but greater than search_prty.
   search_prty is the -ve old prty of the referenced address and the second
   second condition ensures that only entries that have interacted with
   the referenced address are rearranged.
   del_prty is the priority of the address deleted from
   the previous group and the first condition ensures that it displaces
   only entries of lower priority. For the first group del_prty is set
   to 0 which ensures that the entry deleted is of negative priority.

Input: last entry of group, dummy priority of refned unknown and
       dummy priority of deleted unknown
Output: Returns pointer to node matching the above condition
Side effects: None
************************************************************************/

struct tree_node
*Get_first_unknown (entry, search_prty, del_prty)
struct tree_node *entry;
int search_prty,
       del_prty;

{  int grpno,
        prty,
         top;
   struct tree_node *ptr,
	         *oldptr;

   grpno = entry->grpno;
   prty = entry->prty;
   ptr = root;
   top = 0;
   while (ptr){
     if ((ptr->grpno < grpno) ||
		((ptr->grpno == grpno) && (ptr->prty > prty))) {
        ++top;
        p_stack [top] = ptr;
        ptr = ptr->lft;
     }
     else {
       if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
          ++top;
          p_stack [top] = ptr;
	  break;
       }
       ptr = ptr->rt;
     }
   }

   while (top > 0) {
     ptr = p_stack[top];
     --top;
     /* visit node */
/*printf("%d  %d   %d\n", ptr->addr, ptr->grpno, ptr->prty);*/
     if ((ptr->prty > 0) || (ptr->grpno != grpno))
       return (NULL);
     else if ((ptr->prty == search_prty) ||
	((ptr->prty < del_prty) && (ptr->prty > search_prty)))
       return(ptr);

       ptr = ptr->rt;
       while (ptr) {
	 ++top;
	 p_stack[top] = ptr;
	 ptr = ptr->lft;
       }
   }
}                                                            



/*********************************************************************
Given an entry in the stack returns the previous entry

Input: Pointer to entry
Output: Pointer to previous entry
Side effects: None
*********************************************************************/

struct tree_node
*Get_first_unknown_wobacking (grpno, search_prty, del_prty)
int grpno,
    search_prty,
    del_prty;

{  struct tree_node *ptr,
	    *lstlft_node;

        ptr = root;
        while (ptr){
           ++comp;
           if ((ptr->grpno < grpno) ||
			((ptr->grpno == grpno) && (ptr->prty > search_prty))) {
		lstlft_node = ptr;
                ptr = ptr->lft;
           }
           else{
                if ((ptr->grpno == grpno) && (ptr->prty == search_prty)) {
		  return (ptr);
                }
		ptr = ptr->rt;
	   }
        }                                                            

	if ((lstlft_node->grpno == grpno) && (lstlft_node->prty < del_prty))
	  return (lstlft_node);
	else
	  return (NULL);
}        



/********************************************************************
Output routine.

Input: None
Output: None
Side effects: None
********************************************************************/

outpr_facopt ()

{  int sum=0,
       i;
  int stack_end;
  out =  fopen("cheetah-opt.out","w");
   fprintf (out, "Addresses processed : %d\n", t_entries);
   //fprintf (out, "Line size : %d bytes \n", (ONE << L));
   fprintf (out, "Line size : %d bytes \n", (ONE));
   fprintf(out, "Number of distinct addresses  %d\n", tot_addrs);
#ifdef PERF
   fprintf(out, "Total Number of groups passed\t%d\n\n", grps_passd);
   fprintf(out, "Total Number of splay steps\t%d\n\n", no_splay_steps);
   fprintf(out, "Number of insert comparisons\t%d\n", comp_ins);
   fprintf(out, "Number of delete comparisons\t%d\n", comp_del);
   fprintf(out, "Number of times inf_handler called\t%d\n", ihcount);
#endif

   fprintf(out, "Cache size\tMiss Ratio\n");
   stack_end = min (tot_addrs, MAX_LINES);
   
   for (i = 1; i <= stack_end; i++) {
     sum += out_stack[i];
     if ((i % MISS_RATIO_INTERVAL) == 0)
       //fprintf(out, "%d\t\t\t%1.6lf\n", (i * (ONE << L)),
       fprintf(out, "%d\t\t\t%1.6lf\n", (i),
	       (1.0 - ((double)sum/(double)t_entries)));
   }
   if (stack_end == tot_addrs)
     fprintf (out, "Miss ratio is %lf for all bigger caches\n", (1.0 - ((double)sum/(double)t_entries)));
   fprintf (out, "\n\n\n");
 }


/**********************************************************************
Locates the referenced address, deletes it and inserts the address deleted
from the previous group in its place.

Input: Referenced address and address deleted from earlier group
Output: Pointer to node at which insertion occurred.
Side effects.: The deletion and insertion are done and the inserted
node is splayed to the root of the tree.
***********************************************************************/

struct tree_node
*Lookup_Delete_Insert (del_entry, ins_entry)
struct tree_node *del_entry,		/* Entry to be deleted. Refned entry */
		 *ins_entry;		/* Deleted line. To be inserted. */


{  struct tree_node *ptr,
	  *inserted_node;
   int top, pos, at;
   int grpno,
        prty;


   grpno = del_entry->grpno;
   prty = del_entry->prty;
   if (root->grpno == grpno){
       printf ("Error: Hit at root\n");
       return (0);
   }
   else{
        top = 0;
        ptr = root->lft;	/* Starts at root->lft to avoid involving
				   root in balance operations */
        while (ptr){
           ++comp;
           ++top;
           p_stack[top] = ptr;
           if ((ptr->grpno < grpno) ||
			((ptr->grpno == grpno) && (ptr->prty > prty))) {
                ptr = ptr->lft;
           }
           else{
	     if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
                     pos = top;
		     break;
             }
             ptr = ptr->rt;
	   }
        }                                                            

        p_stack[pos]->grpno = ins_entry->grpno;
	p_stack[pos]->prty  = ins_entry->prty;
	p_stack[pos]->addr = ins_entry->addr;
	inserted_node = p_stack[pos];

        at = pos;
        while (at > 1){
            ++no_splay_steps;   /* Counts the number of basic operations */
            splay(at, p_stack);
            at = at - 2;
        }
        root->lft = p_stack[1];
        return (inserted_node);
   } /* else */
} /* fn */



/*********************************************************************
Given an entry in the stack returns the previous entry

Input: Pointer to entry
Output: Pointer to previous entry
Side effects: None
*********************************************************************/

struct tree_node
*Lookup_prev(entry)
struct tree_node *entry;


{  struct tree_node *ptr,
	    *lstlft_node;
   int grpno,
	prty;

	grpno = entry->grpno;
	prty = entry->prty;
        ptr = root;
        while (ptr){
           ++comp;
           if ((ptr->grpno < grpno) ||
			((ptr->grpno == grpno) && (ptr->prty > prty))) {
		lstlft_node = ptr;
                ptr = ptr->lft;
           }
           else{
                if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
		  break;
                }
		ptr = ptr->rt;
	   }
        }                                                            

	if ((ptr == NULL) || (ptr->rt == NULL)) 
	  return (lstlft_node);
	else {
	  ptr = ptr->rt;
	  while (ptr->lft != NULL)
	    ptr = ptr->lft;
	  return (ptr);
	}
}        




/*********************************************************************
Given an entry in the stack returns the next entry

Input: Pointer to entry
Output: Pointer to next entry
Side effects: None
*********************************************************************/

struct tree_node
*Lookup_next(entry)
struct tree_node *entry;


{  struct tree_node *ptr,
	     *lstrt_node;
   int grpno,
	prty;

	grpno = entry->grpno;
	prty = entry->prty;
        ptr = root;
        while (ptr){
           ++comp;
           if ((ptr->grpno < grpno) ||
			((ptr->grpno == grpno) && (ptr->prty > prty))) {
                ptr = ptr->lft;
           }
           else{
                if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
		  break;
                }
		lstrt_node = ptr;
		ptr = ptr->rt;
	   }
        }                                                            

	if ((ptr == NULL) || (ptr->lft == NULL))
	  return (lstrt_node);
	else {
	  ptr = ptr->lft;
	  while (ptr->rt != NULL)
	    ptr = ptr->rt;
	  return (ptr);
	}
}        



/******************************************************************
Routine to decide whether to call Insert_no_Balance() or
Insert_and_Balance(). Currently calls the routines alternately.

Input: Node to be inserted
Output: None
Side effects: Calls an insertion rutine which inserts ins_node
and balances stack if required.
******************************************************************/

Insert (ins_node)
struct tree_node *ins_node;

{ static short insert_flag=0;

  if (insert_flag)
    Insert_and_Balance (ins_node);
  else
    Insert_no_Balance (ins_node);
  toggle (insert_flag);
}



/**************************************************************
Inserts node in the tree

Input: Node to be inserted
Output: None
Side effects: The node is inserted in its proper position
**************************************************************/

Insert_no_Balance (ins_node)
struct tree_node *ins_node;

{  struct tree_node *ptr,	    /* Scratch pointers */
		 *oldptr;
   register int grpno,		    /* Group no. to be searched */
		 prty;		    /* Priority of entry to be searched */
   int lstlft;                      /* Flag for type of last branch */

   ins_node->lft = ins_node->rt = NULL;
   if (root == NULL)
     root = ins_node;
   else {
     grpno = ins_node->grpno;
     prty = ins_node->prty;
     ptr = root;
     while (ptr){
       oldptr = ptr;
       ++comp_ins;
       if (ptr->grpno < grpno) {
         ptr = ptr->lft;
	 lstlft = 1;
       }
       else
         if (ptr->grpno == grpno)
	   if (ptr->prty > prty) {
             ptr = ptr->lft;
	     lstlft = 1;
           }
	   else if (ptr->prty < prty) {
         	ptr = ptr->rt;
	 	lstlft = 0;
       	   }
	   else {
	     printf ("Error in Insert\n");
             break;
	   }
       else { 
         ptr = ptr->rt;
	 lstlft = 0;
       }
     }
     if (lstlft)
       oldptr->lft = ins_node;
     else
       oldptr->rt = ins_node;
   }
}



/********************************************************************
Same as above but the inserted node is splayed to the top

Input: Node to be inserted
Output: None
Side effects: The node is added to the tree and splayed to the top
********************************************************************/

Insert_and_Balance (ins_node)
struct tree_node *ins_node;

{  struct tree_node *ptr,	    /* Scratch pointers */
		 *oldptr;
   int lstlft,                      /* Flag for type of last branch */
        grpno,			    /* Group no. to be searched */
	 prty,			    /* Priority of entry to be searched */
	  top,
	   at;

   ins_node->lft = ins_node->rt = NULL;
   if (root == NULL)
     root = ins_node;
   else
     if (root->lft == NULL)
       root->lft = ins_node;
     else {
       grpno = ins_node->grpno;
       prty = ins_node->prty;
       ptr = root->lft;
       top = 0;
       while (ptr){
	 ++top;
	 p_stack [top] = ptr;
         oldptr = ptr;
         ++comp_ins;
         if ((ptr->grpno < grpno) ||
		  ((ptr->grpno == grpno) && (ptr->prty > prty))) {
           ptr = ptr->lft;
	   lstlft = 1;
         }
         else{
           if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
	     printf ("Error in Insert\n");
             break;
           }
           ptr = ptr->rt;
	   lstlft = 0;
         }
       }
       ++top;
       p_stack [top] = ins_node;
       if (lstlft)
         oldptr->lft = ins_node;
       else
         oldptr->rt = ins_node;

	/* Splay only if path > log(tot_addrs) */
       if (top > log_tot_addrs) {
         at = top;
         while (at > 1){
	   ++no_splay_steps;   /* Counts the number of basic operations */
           splay(at, p_stack);
           at = at - 2;
         }
         root->lft = p_stack[1];
       }
     }
}



/********************************************************************
Deletes node and returns a pointer to the previous node in the stack

Input: Entry to be deleted
Output: Pointer to entry deleted and
        pointer to prev entry (returned using prev_entry
Side effects: An entry is deleted from the stack
********************************************************************/

struct tree_node
*Delete (entry, prev_entry)
struct tree_node *entry,
	   **prev_entry;

{  register int grpno,
	         prty;
   struct tree_node *ptr,     /* Scratch pointers */
	      *oldptr_dn,
	      *oldptr_mn,
	        *dn=NULL,     /* Node deleted */
		     *mn,	/* Node moved */
	    *lstlft_node;
   int lstlft_dn,		/* Flags indicating type of last branch */
       lstlft_mn;

	grpno = entry->grpno;
	prty = entry->prty;
        ptr = root;
        while (ptr){
           ++comp_del;
           if ((ptr->grpno < grpno) ||
			((ptr->grpno == grpno ) && (ptr->prty > prty))){
		oldptr_dn = ptr;
		lstlft_node = ptr;
                ptr = ptr->lft;
		lstlft_dn = 1;
           }
           else{
                if ((ptr->grpno == grpno) && (ptr->prty == prty)) {
		  dn = ptr;
		  break;
                }
	   	oldptr_dn = ptr;
                ptr = ptr->rt;
		lstlft_dn = 0;
           }
        } /* while */
        if (dn == NULL) {
	  printf("Error in Delete\n");
	  return (0);
	}

        else {
	  if (ptr->rt == NULL) {
	    if (ptr == root) root = ptr->lft;
	    else if (lstlft_dn) oldptr_dn->lft = ptr->lft;
	    else             oldptr_dn->rt = ptr->lft;
	    *prev_entry = lstlft_node;
	  }
	  else if (ptr->lft == NULL) {
	    if (ptr == root) root = ptr->rt;
	    else if (lstlft_dn) oldptr_dn->lft = ptr->rt;
	    else             oldptr_dn->rt = ptr->rt;
	    ptr = ptr->rt;
	    while (ptr->lft != NULL)
	      ptr = ptr->lft;
	    *prev_entry = ptr;
	  }
	  else {
	    oldptr_mn = ptr;
	    ptr = ptr->rt;
	    lstlft_mn = 0;
            while (ptr->lft != NULL) {
	      oldptr_mn = ptr;
	      ptr = ptr->lft;
	      lstlft_mn = 1;
	    }
	    mn = ptr;
	    *prev_entry = mn;
	    if (lstlft_mn) oldptr_mn->lft = mn->rt;
	    else	   oldptr_mn->rt = mn->rt;
	    mn->lft = dn->lft;
	    mn->rt = dn->rt;
	    if (dn == root) root = mn;
	    else if (lstlft_dn) oldptr_dn->lft = mn;
	    else 		oldptr_dn->rt = mn;
	  }
	  return (dn);
	}
}




/***********************************************************************
Cleans the unknown hash table. Get the maximum unknown priority in the
stack by calling the routine Get_max_prty_unknown

Input: None
Output: None
Side effects: Removes inessential unknowns from the hash table
***********************************************************************/

hash_clean_fa ()

{  int loc,		/* Scratch */
       max_prty;	/* Max unknown priority in stack */
   struct hash_table *ptr, *oldptr; 	/* Scratch */


   max_prty = Get_max_prty_unknown ();
   if (max_prty >= 0)
     printf ("Error: Max unknown priority non-negative\n");

   for (loc=0; loc<HASHNO; loc++) {
     while ((slot_flag[loc] != 0) && (slot[loc].prty > max_prty)) {
         --unknowns;
         if (slot[loc].nxt == NULL) {
           slot_flag[loc] = 0;
	   continue;
         }
         else {
           slot[loc].addr = slot[loc].nxt->addr;
           slot[loc].grptime = slot[loc].nxt->grptime;
           slot[loc].prty = slot[loc].nxt->prty;
           ptr = slot[loc].nxt;
           slot[loc].nxt = slot[loc].nxt->nxt;
           UHT_Add_to_free_list (ptr);
         }
     } /*while*/
     if (slot_flag[loc] != 0) {
          ptr = &slot[loc];
          while (ptr) {
            if (ptr->prty > max_prty) {
	      --unknowns;
	      oldptr->nxt = ptr->nxt;
	      UHT_Add_to_free_list  (ptr);
	      ptr = oldptr;
            }
            oldptr = ptr;
            ptr = ptr->nxt;
          }
      }
   } /* for */
}



/******************************************************************
Determines the maximum priority of an unknown in the stack by doing an
inorder traversal of the tree.
Adapted from Harry Smith "Data structures: Form and Function"

Input: None
Output: Maximum unknown priority in stack
Side effects: None
******************************************************************/

Get_max_prty_unknown ()

{  int top;
   struct tree_node *ptr;
   int max_prty=(5-INT_MAX);

   ptr = root;
   top = 0;
   while (ptr != NULL) {
     ++top;
     p_stack[top] = ptr;
     ptr = ptr->lft;
   }
   while (top > 0) {
     ptr = p_stack[top];
     --top;
     if ((ptr->prty < 0) && (ptr->prty > max_prty))
       max_prty = ptr->prty;
     if (ptr->rt != NULL) {
       ptr = ptr->rt;
       while (ptr != NULL) {
         ++top;
         p_stack[top] = ptr;
         ptr = ptr->lft;
       }
     }
   }
   return (max_prty);
}

static void output_results() {
  int i;
  FILE *output_file = fopen(output_file_name, "w");  
  //comp_opt_stack_dis();
  for (i=0;i<BIN_SIZE;i++) {
    fprintf(stderr, "**BIN %d** => %d\n", i, hist[i]);
    fprintf(output_file, "%d\t%d\n", i, hist[i]);
  }
  total_time = gethrcycle_x86() - total_time;
  fprintf(stderr, "total memeory accesses: %d\n", p_inum);
  //fprintf(stderr, "total buffer processing times: %llu\n", processing_times);
  fprintf(stderr, "total time: %f sec\n", total_time*1.0/(getMHZ_x86()*1000000));
  //fprintf(stderr, "analysis time: %f sec\n", analysis_time*1.0/(getMHZ_x86()*1000000));
}

static void init() {
  int i;
  processing_times = 0;
  p_inum = 0;
  buf_size = 0;
  for (i=0;i<BIN_SIZE;i++)
    hist[i] = 0;
  upper_boundary[0] = 1;
  upper_boundary[BIN_SIZE-1] = INT_MAX;
  for (i=1;i<BIN_SIZE-1;i++) {
    upper_boundary[i] = upper_boundary[i-1]*2;
    //fprintf(stderr, "%d: %d\n", i, upper_boundary[i]);
  }
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "USAGE: ./LRUReuseDisAnalyzer inputFileName outputFileName\n");
    exit(1);
  }

  total_time = gethrcycle_x86();
  analysis_time = 0;
  init();

  char *file_name = argv[1];
  FILE *file = fopen(file_name, "r");
  char line[256];

  //output_file_name = strtok(file_name, "_");
  //fprintf(stderr, "output_file_name: %s\n", output_file_name);
  //strcat(output_file_name, "_OPT-dis_trace.txt");

  output_file_name = argv[2];
  output_file = fopen(output_file_name, "w");
  //fprintf(stderr, "output_file_name: %s\n", output_file_name);
  //fprintf(stderr, "file_name: %s\n", file_name);

  long long start, end, prev_time, sf;
  long long l;
  la_limit = 2*LA_DIST;
  base = 0;
  init_facopt();

  ADDR addr;
  ADDR ip;

  while (fgets(line, 256, file) != NULL) {
    //fprintf(stderr, "line: %s", line);
    sscanf(line, "%ld\t%p", &addr, &ip);
    //fprintf(stderr, "addr: %ld\tid: %d\n", addr, ip);
    //fprintf(stderr, "addr: %u\tid: %u\n", addr, id);
    //fprintf(stderr, "------------\n");
    buffer[buf_size].addr = addr;
    buffer[buf_size].ip = ip;
    buf_size++;
    //fprintf(stderr, "buf_size=%d\n", buf_size);
    if (buf_size == LA_DIST) {
      for(l=0; l < buf_size; l++) {
	    //fprintf(stderr, "p_inum=%d\tla_limit=%d\n", p_inum, la_limit);
	    if (p_inum == la_limit-1) {
	        start = (la_limit % (2*LA_DIST));
	        end = start + LA_DIST;
	        //fprintf(stderr, "111start=%d\tend=%d\n", start, end);
	        if ((sf = stack_proc_fa(start, end)) == 1) {
	            //fprintf(stderr, "goto END\n");
	            goto END;
	        }
	        la_limit += LA_DIST;
	        toggle (oe_flag);		
	        if (oe_flag)
                base += 2*LA_DIST;
	    }
	    ip = buffer[l].ip;
	    ip_array [(p_inum - base) % (2*LA_DIST)] = ip;
	    addr = buffer[l].addr;
    
        // add by lpc
	    //addr = (ADDR)((((unsigned long)addr)>>L)<<L);
        // end lpc

	    //printf("NO.%d: %u\n", l, addr);
	    addr_array [(p_inum - base) % (2*LA_DIST)] = addr;
	    if ((prev_time = ft_hash(addr)) >= 0)
	        if ((la_limit - prev_time) <= 2*LA_DIST) {
	            //printf("base=%d\n",base);
	            time_array[(prev_time - base) % (2*LA_DIST)] = p_inum;
	            //fprintf(stderr, "time_array[%d]=%d\n", (prev_time - base) % (2*LA_DIST), p_inum);
	        }
	        else
	            inf_handler_fa(addr, p_inum);
	    else
	        inf_handler_fa(addr, p_inum);
	    ++p_inum;
      } /*for*/
      //--p_inum;
      buf_size = 0;
    }
  } /*while*/

  for(l=0; l < buf_size; l++) {
    //fprintf(stderr, "p_inum=%d\tla_limit=%d\n", p_inum, la_limit);
    ip = buffer[l].ip;
    ip_array [(p_inum - base) % (2*LA_DIST)] = ip;
    addr = buffer[l].addr;
    // add by lpc
    // addr = (ADDR)((((unsigned long)addr)>>L)<<L);
    // end lpc
    //printf("NO.%d: %u\n", l, addr);
    addr_array [(p_inum - base) % (2*LA_DIST)] = addr;
    if ((prev_time = ft_hash(addr)) >= 0)
      if ((la_limit - prev_time) <= 2*LA_DIST) {
	    //printf("base=%d\n",base);
	    time_array[(prev_time - base) % (2*LA_DIST)] = p_inum;
	    //fprintf(stderr, "time_array[%d]=%d\n", (prev_time - base) % (2*LA_DIST), p_inum);
      }
      else
	    inf_handler_fa(addr, p_inum);
    else
      inf_handler_fa(addr, p_inum);
    ++p_inum;
  }
  //--p_inum;

  /* Residual processing */
  //fprintf(stderr,"Residual processing\n");
  //fprintf(stderr,"p_inum=%d\n",p_inum);
  start = la_limit % (2*LA_DIST);
  end = start + LA_DIST;
  if (p_inum < LA_DIST) end = p_inum; /* For small traces */
  //fprintf(stderr, "222start=%d\tend=%d\n", start, end);
  stack_proc_fa(start, end);
  if (p_inum >= LA_DIST) {
    toggle (oe_flag);
    if (oe_flag)
      base += 2*LA_DIST;
    la_limit += LA_DIST;
    start = la_limit % (2*LA_DIST);
    end = start + (((p_inum % LA_DIST) == 0) ? LA_DIST : (p_inum % LA_DIST));
    //fprintf(stderr, "333start=%d\tend=%d\n", start, end);
    stack_proc_fa(start, end);
  }
 END:
  printf ("\n");


  //output_results();
  return 0;
}
