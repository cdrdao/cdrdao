/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 *
 * Toplevel file for the paranoia abstraction over the cdda lib 
 *
 ***/

/* immediate todo:: */
/* Allow disabling of root fixups? */ 
/* Dupe bytes are creeping into cases that require greater overlap
   *than a single fragment can provide.  We need to check against a
   *larger area* (+/-32 sectors of root?) to better eliminate
   *dupes. Of course this leads to other problems... Is it actually a
   *practically solvable problem? */
/* Bimodal overlap distributions break us. */
/* scratch detection/tolerance not implemented yet */

/***************************************************************

  Da new shtick: verification now a two-step assymetric process.
  
  A single 'verified/reconstructed' data segment cache, and then the
  multiple fragment cache 

  verify a newly read block against previous blocks; do it only this
  once. We maintain a list of 'verified sections' from these matches.

  We then glom these verified areas into a new data buffer.
  Defragmentation fixups are allowed here alone.

  We also now track where read boundaries actually happened; do not
  verify across matching boundaries.

  **************************************************************/

/***************************************************************

  Silence case continues to dog us; let's try the following to put the
  damned thing to rest (stage 2 mods only):

  a) at root fixup stage: past the rift is one silence and the other
     signal?  Believe the signal.
  b) if signal is in root, truncate the fragment.  if the signal is in
     the fragment, truncate root

  **************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../dao/cdda_interface.h"
#include "p_block.h"
#include "cdda_paranoia.h"
#include "overlap.h"
#include "gap.h"
#include "isort.h"

static inline long re(root_block *root){
  if(!root)return(-1);
  if(!root->vector)return(-1);
  return(ce(root->vector));
}

static inline long rb(root_block *root){
  if(!root)return(-1);
  if(!root->vector)return(-1);
  return(cb(root->vector));
}

static inline long rs(root_block *root){
  if(!root)return(-1);
  if(!root->vector)return(-1);
  return(cs(root->vector));
}

static inline size16 *rv(root_block *root){
  if(!root)return(NULL);
  if(!root->vector)return(NULL);
  return(cv(root->vector));
}

#define rc(r) (r->vector)

/**** matching and analysis code *****************************************/

static inline long i_paranoia_overlap(size16 *buffA,size16 *buffB,
			       long offsetA, long offsetB,
			       long sizeA,long sizeB,
			       long *ret_begin, long *ret_end){
  long beginA=offsetA,endA=offsetA;
  long beginB=offsetB,endB=offsetB;

  for(;beginA>=0 && beginB>=0;beginA--,beginB--)
    if(buffA[beginA]!=buffB[beginB])break;
  beginA++;
  beginB++;
  
  for(;endA<sizeA && endB<sizeB;endA++,endB++)
    if(buffA[endA]!=buffB[endB])break;
  
  if(ret_begin)*ret_begin=beginA;
  if(ret_end)*ret_end=endA;
  return(endA-beginA);
}

static inline long i_paranoia_overlap2(size16 *buffA,size16 *buffB,
				       char *flagsA,char *flagsB,
				       long offsetA, long offsetB,
				       long sizeA,long sizeB,
				       long *ret_begin, long *ret_end){
  long beginA=offsetA,endA=offsetA;
  long beginB=offsetB,endB=offsetB;
  
  for(;beginA>=0 && beginB>=0;beginA--,beginB--){
    if(buffA[beginA]!=buffB[beginB])break;
    /* don't allow matching across matching sector boundaries */
    /* don't allow matching through known missing data */
    if((flagsA[beginA]&flagsB[beginB]&1)){
      beginA--;
      beginB--;
      break;
    }
    if((flagsA[beginA]&2)|| (flagsB[beginB]&2))break;
  }
  beginA++;
  beginB++;
  
  for(;endA<sizeA && endB<sizeB;endA++,endB++){
    if(buffA[endA]!=buffB[endB])break;
    /* don't allow matching across matching sector boundaries */
    if((flagsA[endA]&flagsB[endB]&1) &&endA!=beginA){
      break;
    }

    /* don't allow matching through known missing data */
    if((flagsA[endA]&2)||(flagsB[endB]&2))break;
  }

  if(ret_begin)*ret_begin=beginA;
  if(ret_end)*ret_end=endA;
  return(endA-beginA);
}

/* Top level of the first stage matcher */

/* We match each analysis point of new to the preexisting blocks
recursively.  We can also optionally maintain a list of fragments of
the preexisting block that didn't match anything, and match them back
afterward. */

#define OVERLAP_ADJ (MIN_WORDS_OVERLAP/2-1)

static inline long do_const_sync(c_block *A,
				 sort_info *B,char *flagB,
				 long posA,long posB,
				 long *begin,long *end,long *offset){
  char *flagA=A->flags;
  long ret=0;

  if(flagB==NULL)
    ret=i_paranoia_overlap(cv(A),iv(B),posA,posB,
			   cs(A),is(B),begin,end);
  else
    if((flagB[posB]&2)==0)
      ret=i_paranoia_overlap2(cv(A),iv(B),flagA,flagB,posA,posB,cs(A),
			      is(B),begin,end);
	
  if(ret>MIN_WORDS_SEARCH){
    *offset=+(posA+cb(A))-(posB+ib(B));
    *begin+=cb(A);
    *end+=cb(A);
    return(ret);
  }
  
  return(0);
}

/* post is w.r.t. B.  9.3 is a bit different; in stage one, we post
   from new.  In stage 2 we post from root. Begin, end, offset count
   from B's frame of reference */

static inline long try_sort_sync(cdrom_paranoia *p,
				 sort_info *A,char *Aflags,
				 c_block *B,
				 long post,long *begin,long *end,
				 long *offset,void (*callback)(long,int)){
  
  long dynoverlap=p->dynoverlap;
  sort_link *ptr=NULL;
  char *Bflags=B->flags;

  /* block flag matches 0x02 (unmatchable) */
  if(Bflags==NULL || (Bflags[post-cb(B)]&2)==0){
    /* always try absolute offset zero first! */
    {
      long zeropos=post-ib(A);
      if(zeropos>=0 && zeropos<is(A)){
	if(cv(B)[post-cb(B)]==iv(A)[zeropos]){
	  if(do_const_sync(B,A,Aflags,
			   post-cb(B),zeropos,
			   begin,end,offset)){
	    
	    offset_add_value(p,&(p->stage1),*offset,callback);
	    
	    return(1);
	  }
	}
      }
    }
  }else
    return(0);
  
  ptr=sort_getmatch(A,post-ib(A),dynoverlap,cv(B)[post-cb(B)]);
  
  while(ptr){
    
    if(do_const_sync(B,A,Aflags,
		     post-cb(B),ipos(A,ptr),
		     begin,end,offset)){
      offset_add_value(p,&(p->stage1),*offset,callback);
      return(1);
    }
    ptr=sort_nextmatch(A,ptr);
  }
  
  *begin=-1;
  *end=-1;
  *offset=-1;
  return(0);
}

static inline void stage1_matched(c_block *old,c_block *new,
				 long matchbegin,long matchend,
				 long matchoffset,void (*callback)(long,int)){
  long i;
  long oldadjbegin=matchbegin-cb(old);
  long oldadjend=matchend-cb(old);
  long newadjbegin=matchbegin-matchoffset-cb(new);
  long newadjend=matchend-matchoffset-cb(new);
  
  if(matchbegin-matchoffset<=cb(new) ||
     matchbegin<=cb(old) ||
     (new->flags[newadjbegin]&1) ||
     (old->flags[oldadjbegin]&1)){
    if(matchoffset)
      (*callback)(matchbegin,PARANOIA_CB_FIXUP_EDGE);
  }else
    (*callback)(matchbegin,PARANOIA_CB_FIXUP_ATOM);
  
  if(matchend-matchoffset>=ce(new) ||
     (new->flags[newadjend]&1) ||
     matchend>=ce(old) ||
     (old->flags[oldadjend]&1)){
    if(matchoffset)
      (*callback)(matchend,PARANOIA_CB_FIXUP_EDGE);
  }else
    (*callback)(matchend,PARANOIA_CB_FIXUP_ATOM);
  
  /* Mark the verification flags.  Don't mark the first or
     last OVERLAP/2 elements so that overlapping fragments
     have to overlap by OVERLAP to actually merge. We also
     remove elements from the sort such that later sorts do
     not have to sift through already matched data */
  
  newadjbegin+=OVERLAP_ADJ;
  newadjend-=OVERLAP_ADJ;
  for(i=newadjbegin;i<newadjend;i++)
    new->flags[i]|=4; /* mark verified */

  oldadjbegin+=OVERLAP_ADJ;
  oldadjend-=OVERLAP_ADJ;
  for(i=oldadjbegin;i<oldadjend;i++)
    old->flags[i]|=4; /* mark verified */
    
}

static long i_iterate_stage1(cdrom_paranoia *p,c_block *old,c_block *new,
			     void(*callback)(long,int)){

  long matchbegin=-1,matchend=-1,matchoffset;

  /* we no longer try to spread the stage one search area by dynoverlap */
  long searchend=min(ce(old),ce(new));
  long searchbegin=max(cb(old),cb(new));
  long searchsize=searchend-searchbegin;
  sort_info *i=p->sortcache;
  long ret=0;
  long j;

  long tried=0,matched=0;

  if(searchsize<=0)return(0);
  
  /* match return values are in terms of the new vector, not old */

  for(j=searchbegin;j<searchend;j+=23){
    if((new->flags[j-cb(new)]&6)==0){      
      tried++;
      if(try_sort_sync(p,i,new->flags,old,j,&matchbegin,&matchend,&matchoffset,
		       callback)==1){
	
	matched+=matchend-matchbegin;
	stage1_matched(old,new,matchbegin,matchend,matchoffset,callback);
	ret++;
	if(matchend-1>j)j=matchend-1;
      }
    }
  }
#ifdef NOISY 
  fprintf(stderr,"iterate_stage1: search area=%ld[%ld-%ld] tried=%ld matched=%ld spans=%ld\n",
	  searchsize,searchbegin,searchend,tried,matched,ret);
#endif

  return(ret);
}

static long i_stage1(cdrom_paranoia *p,c_block *new,
		     void(*callback)(long,int)){

  long size=cs(new);
  c_block *ptr=c_last(p);
  int ret=0;
  long begin=0,end;
  
  if(ptr)sort_setup(p->sortcache,cv(new),&cb(new),cs(new),
		    cb(new),ce(new));

  while(ptr && ptr!=new){

    (*callback)(cb(new),PARANOIA_CB_VERIFY);
    i_iterate_stage1(p,ptr,new,callback);

    ptr=c_prev(ptr);
  }

  /* parse the verified areas of new into v_fragments */
  
  begin=0;
  while(begin<size){
    for(;begin<size;begin++)if(new->flags[begin]&4)break;
    for(end=begin;end<size;end++)if((new->flags[end]&4)==0)break;
    if(begin>=size)break;
    
    ret++;

    new_v_fragment(p,new,cb(new)+max(0,begin-OVERLAP_ADJ),
		   cb(new)+min(size,end+OVERLAP_ADJ),
		   (end+OVERLAP_ADJ>=size && new->lastsector));

    begin=end;
  }
  
  return(ret);
}

/* reconcile v_fragments to root buffer.  Free if matched, fragment/fixup root
   if necessary */

typedef struct sync_result {
  long offset;
  long begin;
  long end;
} sync_result;

static long i_iterate_stage2(cdrom_paranoia *p,
			     v_fragment *v,int multi,
			     sync_result *r,void(*callback)(long,int)){
  root_block *root=&(p->root);
  long matchbegin=-1,matchend=-1,offset;
  long fbv,fev;
  
#ifdef NOISY
      fprintf(stderr,"Stage 2 search: fbv=%ld fev=%ld\n",fb(v),fe(v));
#endif

  if(min(fe(v)+p->dynoverlap,re(root))-
    max(fb(v)-p->dynoverlap,rb(root))<=0)return(0);

  (*callback)(fb(v),PARANOIA_CB_VERIFY);

  /* just a bit of v unless multi; determine the correct area */
  fbv=max(fb(v),rb(root)-p->dynoverlap);
  /* stage 2 silence handling mod A,B */
  if(!multi){
    /* we want to avoid zeroes */
    while(fbv<fe(v) && fv(v)[fbv-fb(v)]==0)fbv++;
    if(fbv==fe(v))return(0);
    fev=min(min(fbv+256,re(root)+p->dynoverlap),fe(v));
  }else{
    fev=min(re(root)+p->dynoverlap,fe(v));
  }
  
  {
    /* spread the search area a bit.  We post from root, so containment
       must strictly adhere to root */
    long searchend=min(fev+p->dynoverlap,re(root));
    long searchbegin=max(fbv-p->dynoverlap,rb(root));
    sort_info *i=p->sortcache;
    long j;
    
    sort_setup(i,fv(v),&fb(v),fs(v),fbv,fev);
    
    for(j=searchbegin;j<searchend;j+=23){
      if(!multi)
	while(j<searchend && rv(root)[j-rb(root)]==0)j++;
      if(j==searchend)break;
     
      if(try_sort_sync(p,i,NULL,rc(root),j,
		       &matchbegin,&matchend,&offset,callback)){

	r->begin=matchbegin;
	r->end=matchend;
	r->offset=-offset;
	if(offset)(*callback)(r->begin,PARANOIA_CB_FIXUP_EDGE);
	return(1);
      }
    }
  }
  
  return(0);
}

static long i_stage2_each(root_block *root, v_fragment *v,
			  int freeit,int multi,
			  void(*callback)(long,int)){

  cdrom_paranoia *p=v->p;
  long dynoverlap=p->dynoverlap/2*2;
  
  if(!v || !v->one)return(0);

  if(!rv(root)){
    return(0);
  }else{
    sync_result r;

    if(i_iterate_stage2(p,v,multi,&r,callback)){

      long begin=r.begin-rb(root);
      long end=r.end-rb(root);
      long offset=r.begin+r.offset-fb(v)-begin;
      long temp;
      c_block *l=NULL;

      /* we have a match! We don't rematch off rift, we chase the
	 match all the way to both extremes doing rift analysis. */

#ifdef NOISY
      fprintf(stderr,"Stage 2 match\n");
#endif

      /* chase backward */
      /* note that we don't extend back right now, only forward. */
      while((begin+offset>0 && begin>0)){
	long matchA=0,matchB=0,matchC=0;
	long beginL=begin+offset;

	if(l==NULL){
	  size16 *buff=malloc(fs(v)*sizeof(size16));
	  l=c_alloc(buff,fb(v),fs(v));
	  memcpy(buff,fv(v),fs(v)*sizeof(size16));
	}

	i_analyze_rift_r(rv(root),cv(l),
			 rs(root),cs(l),
			 begin-1,beginL-1,
			 &matchA,&matchB,&matchC);
	
#ifdef NOISY
	fprintf(stderr,"matching rootR: matchA:%ld matchB:%ld matchC:%ld\n",
		matchA,matchB,matchC);
#endif		
	
	if(matchA){
	  /* a problem with root */
	  if(matchA>0){
	    /* dropped bytes; add back from v */
	    (*callback)(begin+rb(root)-1,PARANOIA_CB_FIXUP_DROPPED);
	    if(rb(root)+begin<p->root.returnedlimit)
	      break;
	    else{
	      c_insert(rc(root),begin,cv(l)+beginL-matchA,
			 matchA);
	      offset-=matchA;
	      begin+=matchA;
	      end+=matchA;
	    }
	  }else{
	    /* duplicate bytes; drop from root */
	    (*callback)(begin+rb(root)-1,PARANOIA_CB_FIXUP_DUPED);
	    if(rb(root)+begin+matchA<p->root.returnedlimit) 
	      break;
	    else{
	      c_remove(rc(root),begin+matchA,-matchA);
	      offset-=matchA;
	      begin+=matchA;
	      end+=matchA;
	    }
	  }
	}else if(matchB){
	  /* a problem with the fragment */
	  if(matchB>0){
	    /* dropped bytes */
	    (*callback)(begin+rb(root)-1,PARANOIA_CB_FIXUP_DROPPED);
	    c_insert(l,beginL,rv(root)+begin-matchB,
			 matchB);
	    offset+=matchB;
	  }else{
	    /* duplicate bytes */
	    (*callback)(begin+rb(root)-1,PARANOIA_CB_FIXUP_DUPED);
	    c_remove(l,beginL+matchB,-matchB);
	    offset+=matchB;
	  }
	}else if(matchC){
	  /* Uhh... problem with both */
	  
	  /* Set 'disagree' flags in root */
	  if(rb(root)+begin-matchC<p->root.returnedlimit)
	    break;
	  c_overwrite(rc(root),begin-matchC,
			cv(l)+beginL-matchC,matchC);
	  
	}else{
	  /* do we have a mismatch due to silence beginning/end case? */
	  /* in the 'chase back' case, we don't do anything. */

	  /* Did not determine nature of difficulty... 
	     report and bail */
	    
	  /*RRR(*callback)(post,PARANOIA_CB_XXX);*/
	  break;
	}
	/* not the most efficient way, but it will do for now */
	beginL=begin+offset;
	i_paranoia_overlap(rv(root),cv(l),
			   begin,beginL,
			   rs(root),cs(l),
			   &begin,&end);	
      }
      
      /* chase forward */
      temp=l?cs(l):fs(v);
      while(end+offset<temp && end<rs(root)){
	long matchA=0,matchB=0,matchC=0;
	long beginL=begin+offset;
	long endL=end+offset;
	
	if(l==NULL){
	  size16 *buff=malloc(fs(v)*sizeof(size16));
	  l=c_alloc(buff,fb(v),fs(v));
	  memcpy(buff,fv(v),fs(v)*sizeof(size16));
	}

	i_analyze_rift_f(rv(root),cv(l),
			 rs(root),cs(l),
			 end,endL,
			 &matchA,&matchB,&matchC);
	
#ifdef NOISY	
	fprintf(stderr,"matching rootF: matchA:%ld matchB:%ld matchC:%ld\n",
		matchA,matchB,matchC);
#endif
	
	if(matchA){
	  /* a problem with root */
	  if(matchA>0){
	    /* dropped bytes; add back from v */
	    (*callback)(end+rb(root),PARANOIA_CB_FIXUP_DROPPED);
	    if(end+rb(root)<p->root.returnedlimit)
	      break;
	    c_insert(rc(root),end,cv(l)+endL,matchA);
	  }else{
	    /* duplicate bytes; drop from root */
	    (*callback)(end+rb(root),PARANOIA_CB_FIXUP_DUPED);
	    if(end+rb(root)<p->root.returnedlimit)
	      break;
	    c_remove(rc(root),end,-matchA);
	  }
	}else if(matchB){
	  /* a problem with the fragment */
	  if(matchB>0){
	    /* dropped bytes */
	    (*callback)(end+rb(root),PARANOIA_CB_FIXUP_DROPPED);
	    c_insert(l,endL,rv(root)+end,matchB);
	  }else{
	    /* duplicate bytes */
	    (*callback)(end+rb(root),PARANOIA_CB_FIXUP_DUPED);
	    c_remove(l,endL,-matchB);
	  }
	}else if(matchC){
	  /* Uhh... problem with both */
	  
	  /* Set 'disagree' flags in root */
	  if(end+rb(root)<p->root.returnedlimit)
	    break;
	  c_overwrite(rc(root),end,cv(l)+endL,matchC);
	}else{
	  analyze_rift_silence_f(rv(root),cv(l),
				 rs(root),cs(l),
				 end,endL,
				 &matchA,&matchB);
	  if(matchA){
	    /* silence in root */
	    /* Can only do this if we haven't already returned data */
	    if(end+rb(root)>=p->root.returnedlimit){
	      c_remove(rc(root),end,-1);
	    }

	  }else if(matchB){
	    /* silence in fragment; lose it */
	    
	    if(l)i_cblock_destructor(l);
	    if(freeit)free_v_fragment(v);
	    return(1);

	  }else{
	    /* Could not determine nature of difficulty... 
	       report and bail */
	    
	    /*RRR(*callback)(post,PARANOIA_CB_XXX);*/
	  }
	  break;
	}
	/* not the most efficient way, but it will do for now */
	i_paranoia_overlap(rv(root),cv(l),
			   begin,beginL,
			   rs(root),cs(l),
			   NULL,&end);
      }

      /* if this extends our range, let's glom */
      {
	long sizeA=rs(root);
	long sizeB;
	long vecbegin;
	size16 *vector;
	  
	if(l){
	  sizeB=cs(l);
	  vector=cv(l);
	  vecbegin=cb(l);
	}else{
	  sizeB=fs(v);
	  vector=fv(v);
	  vecbegin=fb(v);
	}

	if(sizeB-offset>sizeA || v->lastsector){	  
	  if(v->lastsector){
	    root->lastsector=1;
	  }
	  
	  if(end<sizeA)c_remove(rc(root),end,-1);
	  
	  if(sizeB-offset-end)c_append(rc(root),vector+end+offset,
					 sizeB-offset-end);
	  
	  /* add offset into dynoverlap stats */
	  offset_add_value(p,&p->stage2,offset+vecbegin-rb(root),callback);
	}
      }
      if(l)i_cblock_destructor(l);
      if(freeit)free_v_fragment(v);
      return(1);
      
    }else{
      /* D'oh.  No match.  What to do with the fragment? */
      if(fe(v)+dynoverlap<re(root)){
	/* It *should* have matched.  No good; free it. */
	if(freeit)free_v_fragment(v);
      }
      /* otherwise, we likely want this for an upcoming match */
      /* we don't free the sort info (if it was collected) */
      return(0);
      
    }
  }
}

static int i_init_root(root_block *root, v_fragment *v,long begin,
		       void(*callback)(long,int)){
  if(fb(v)<=begin && fe(v)>begin){
    
    root->lastsector=v->lastsector;
    root->returnedlimit=begin;

    if(rv(root)){
      i_cblock_destructor(rc(root));
      rc(root)=NULL;
    }

    {
      size16 *buff=malloc(fs(v)*sizeof(size16));
      memcpy(buff,fv(v),fs(v)*sizeof(size16));
      root->vector=c_alloc(buff,fb(v),fs(v));
    }    
    return(1);
  }else
    return(0);
}

static int vsort(const void *a,const void *b){
  return((*(v_fragment **)a)->begin-(*(v_fragment **)b)->begin);
}

static int i_stage2(cdrom_paranoia *p,long beginword,long endword,
			  void(*callback)(long,int)){

  int flag=1,multi=0,ret=0;
  root_block *root=&(p->root);

#ifdef NOISY
  fprintf(stderr,"Fragments:%ld\n",p->fragments->active);
  fflush(stderr);
#endif

  while(flag){
    /* loop through all the current fragments */
    v_fragment *first=v_first(p);
    long active=p->fragments->active,count=0;
    v_fragment *list[active];

    while(first){
      v_fragment *next=v_next(first);
      list[count++]=first;
      first=next;
    }

    flag=0;
    if(count){
      qsort(list,active,sizeof(v_fragment *),&vsort);
      
      for(count=0;count<active;count++){
	first=list[count];
	if(first->one){
	  if(rv(root)==NULL){
	    if(i_init_root(&(p->root),first,beginword,callback)){
	      free_v_fragment(first);
	      flag=1;
	      ret++;
	    }
	  }else{
	    if(i_stage2_each(root,first,1,multi,callback)){
	      ret++;
	      flag=1;
	      multi=0;
	    }
	  }
	}
      }
    }
    if(!flag){
      if(!multi){
	multi=1;
	flag=1;
      }
    }else{
      multi=0;
    }
  }
  return(ret);
}

static void i_end_case(cdrom_paranoia *p,long endword,
			    void(*callback)(long,int)){

  root_block *root=&p->root;

  /* have an 'end' flag; if we've just read in the last sector in a
     session, set the flag.  If we verify to the end of a fragment
     which has the end flag set, we're done (set a done flag).  Pad
     zeroes to the end of the read */
  
  if(root->lastsector==0)return;
  if(endword<re(root))return;
  
  {
    long addto=endword-re(root);
    char *temp=calloc(addto,sizeof(char)*2);

    c_append(rc(root),(void *)temp,addto);
    free(temp);

    /* trash da cache */
    paranoia_resetcache(p);

  }
}

/* We want to add a sector. Look through the caches for something that
   spans.  Also look at the flags on the c_block... if this is an
   obliterated sector, get a bit of a chunk past the obliteration. */

/* Not terribly smart right now, actually.  We can probably find
   *some* match with a cache block somewhere.  Take it and continue it
   through the skip */

static void verify_skip_case(cdrom_paranoia *p,void(*callback)(long,int)){

  root_block *root=&(p->root);
  c_block *graft=NULL;
  int vflag=0;
  int gend=0;
  long post;
  
#ifdef NOISY
	fprintf(stderr,"\nskipping\n");
#endif

  if(rv(root)==NULL){
    post=0;
  }else{
    post=re(root);
  }
  if(post==-1)post=0;

  (*callback)(post,PARANOIA_CB_SKIP);
  
  /* We want to add a sector.  Look for a c_block that spans,
     preferrably a verified area */

  {
    c_block *c=c_first(p);
    while(c){
      long cbegin=cb(c);
      long cend=ce(c);
      if(cbegin<=post && cend>post){
	long vend=post;

	if(c->flags[post-cbegin]&4){
	  /* verified area! */
	  while(vend<cend && (c->flags[vend-cbegin]&4))vend++;
	  if(!vflag || vend>vflag){
	    graft=c;
	    gend=vend;
	  }
	  vflag=1;
	}else{
	  /* not a verified area */
	  if(!vflag){
	    while(vend<cend && (c->flags[vend-cbegin]&4)==0)vend++;
	    if(graft==NULL || gend>vend){
	      /* smallest unverified area */
	      graft=c;
	      gend=vend;
	    }
	  }
	}
      }
      c=c_next(c);
    }

    if(graft){
      long cbegin=cb(graft);
      long cend=ce(graft);

      while(gend<cend && (graft->flags[gend-cbegin]&4))gend++;
      gend=min(gend+OVERLAP_ADJ,cend);

      if(rv(root)==NULL){
	size16 *buff=malloc(cs(graft));
	memcpy(buff,cv(graft),cs(graft));
	rc(root)=c_alloc(buff,cb(graft),cs(graft));
      }else{
	c_append(rc(root),cv(graft)+post-cbegin,
		 gend-post);
      }

      root->returnedlimit=re(root);
      return;
    }
  }

  /* No?  Fine.  Great.  Write in some zeroes :-P */
  {
    void *temp=calloc(CD_FRAMESIZE_RAW,sizeof(size16));

    if(rv(root)==NULL){
      rc(root)=c_alloc(temp,post,CD_FRAMESIZE_RAW);
    }else{
      c_append(rc(root),temp,CD_FRAMESIZE_RAW);
      free(temp);
    }
    root->returnedlimit=re(root);
  }
}    

/**** toplevel ****************************************/

void paranoia_free(cdrom_paranoia *p){
  paranoia_resetall(p);
  free(p);
}

void paranoia_modeset(cdrom_paranoia *p,int enable){
  p->enable=enable;
}

#if 0
long paranoia_seek(cdrom_paranoia *p,long seek,int mode){
  long sector;
  long ret;
  switch(mode){
  case SEEK_SET:
    sector=seek;
    break;
  case SEEK_END:
    sector=cdda_disc_lastsector(p->d)+seek;
    break;
  default:
    sector=p->cursor+seek;
    break;
  }
  
  if(cdda_sector_gettrack(p->d,sector)==-1)return(-1);

  i_cblock_destructor(p->root.vector);
  p->root.vector=NULL;
  p->root.lastsector=0;
  p->root.returnedlimit=0;

  ret=p->cursor;
  p->cursor=sector;

  i_paranoia_firstlast(p);
  
  /* Evil hack to fix pregap patch for NEC drives! To be rooted out in a10 */
  p->current_firstsector=sector;

  return(ret);
}
#endif

/* returns last block read, -1 on error */
c_block *i_read_c_block(cdrom_paranoia *p,long beginword,long endword,
		     void(*callback)(long,int)){

/* why do it this way?  We need to read lots of sectors to kludge
   around stupid read ahead buffers on cheap drives, as well as avoid
   expensive back-seeking. We also want to 'jiggle' the start address
   to try to break borderline drives more noticeably (and make broken
   drives with unaddressable sectors behave more often). */
      
  long readat,firstread;
  long totaltoread=p->readahead;
  long sectatonce=p->d->nsectors;
  long driftcomp=(float)p->dyndrift/CD_FRAMEWORDS+.5;
  c_block *new=NULL;
  root_block *root=&p->root;
  size16 *buffer=NULL;
  char *flags=NULL;
  long sofar;
  long dynoverlap=(p->dynoverlap+CD_FRAMEWORDS-1)/CD_FRAMEWORDS; 
  long anyflag=0;

  /* What is the first sector to read?  want some pre-buffer if
     we're not at the extreme beginning of the disc */
  
  if(p->enable&(PARANOIA_MODE_VERIFY|PARANOIA_MODE_OVERLAP)){
    
    /* we want to jitter the read alignment boundary */
    long target;
    if(rv(root)==NULL || rb(root)>beginword)
      target=p->cursor-dynoverlap; 
    else
      target=re(root)/(CD_FRAMEWORDS)-dynoverlap;
	
    if(target+MIN_SECTOR_BACKUP>p->lastread && target<=p->lastread)
      target=p->lastread-MIN_SECTOR_BACKUP;
      
    /* we want to jitter the read alignment boundary, as some
       drives, beginning from a specific point, will tend to
       lose bytes between sectors in the same place.  Also, as
       our vectors are being made up of multiple reads, we want
       the overlap boundaries to move.... */
    
    readat=(target&(~((long)JIGGLE_MODULO-1)))+p->jitter;
    if(readat>target)readat-=JIGGLE_MODULO;
    p->jitter++;
    if(p->jitter>=JIGGLE_MODULO)p->jitter=0;
     
  }else{
    readat=p->cursor; 
  }
  
  readat+=driftcomp;
  
  if(p->enable&(PARANOIA_MODE_OVERLAP|PARANOIA_MODE_VERIFY)){
    flags=calloc(totaltoread*CD_FRAMEWORDS,1);
    new=new_c_block(p);
    recover_cache(p);
  }else{
    /* in the case of root it's just the buffer */
    paranoia_resetall(p);	
    new=new_c_block(p);
  }

  buffer=malloc(totaltoread*CD_FRAMESIZE_RAW);
  sofar=0;
  firstread=-1;
  
  /* actual read loop */

  while(sofar<totaltoread){
    long secread=sectatonce;
    long adjread=readat;
    long thisread;

    /* don't under/overflow the audio session */
    if(adjread<p->current_firstsector){
      secread-=p->current_firstsector-adjread;
      adjread=p->current_firstsector;
    }
    if(adjread+secread-1>p->current_lastsector)
      secread=p->current_lastsector-adjread+1;
    
    if(sofar+secread>totaltoread)secread=totaltoread-sofar;
    
    if(secread>0){
      
      if(firstread<0)firstread=adjread;
      if((thisread=cdda_read(p->d,buffer+sofar*CD_FRAMEWORDS,adjread,
			    secread))<secread){

	if(thisread<0)thisread=0;

	/* Uhhh... right.  Make something up. But don't make us seek
           backward! */

	(*callback)((adjread+thisread)*CD_FRAMEWORDS,PARANOIA_CB_READERR);  
	memset(buffer+(sofar+thisread)*CD_FRAMEWORDS,0,
	       CD_FRAMESIZE_RAW*(secread-thisread));
	if(flags)memset(flags+(sofar+thisread)*CD_FRAMEWORDS,2,
	       CD_FRAMEWORDS*(secread-thisread));
      }
      if(thisread!=0)anyflag=1;
      
      if(flags && sofar!=0){
	/* Don't verify across overlaps that are too close to one
           another */
	int i=0;
	for(i=-MIN_WORDS_OVERLAP/2;i<MIN_WORDS_OVERLAP/2;i++)
	  flags[sofar*CD_FRAMEWORDS+i]|=1;
      }

      p->lastread=adjread+secread;
      
      if(adjread+secread-1==p->current_lastsector)
	new->lastsector=-1;
      
      (*callback)((adjread+secread-1)*CD_FRAMEWORDS,PARANOIA_CB_READ);
      
      sofar+=secread;
      readat=adjread+secread; 
    }else
      if(readat<p->current_firstsector)
	readat+=sectatonce; /* due to being before the readable area */
      else
	break; /* due to being past the readable area */
  }

  if(anyflag){
    new->vector=buffer;
    new->begin=firstread*CD_FRAMEWORDS-p->dyndrift;
    new->size=sofar*CD_FRAMEWORDS;
    new->flags=flags;
  }else{
    if(new)free_c_block(new);
    free(buffer);
    free(flags);
    new=NULL;
  }
  return(new);
}

/* The returned buffer is *not* to be freed by the caller.  It will
   persist only until the next call to paranoia_read() for this p */

size16 *paranoia_read(cdrom_paranoia *p, void(*callback)(long,int)){

  long beginword=p->cursor*(CD_FRAMEWORDS);
  long endword=beginword+CD_FRAMEWORDS;
  long retry_count=0,lastend=-2;
  root_block *root=&p->root;

  if(beginword>p->root.returnedlimit)p->root.returnedlimit=beginword;
  lastend=re(root);
  
  /* First, is the sector we want already in the root? */
  while(rv(root)==NULL ||
	rb(root)>beginword || 
	(re(root)<endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS) &&
	 p->enable&(PARANOIA_MODE_VERIFY|PARANOIA_MODE_OVERLAP)) ||
	re(root)<endword){
    
    /* Nope; we need to build or extend the root verified range */
    
    if(p->enable&(PARANOIA_MODE_VERIFY|PARANOIA_MODE_OVERLAP)){
      i_paranoia_trim(p,beginword,endword);
      recover_cache(p);
      if(rb(root)!=-1 && p->root.lastsector)
	i_end_case(p,endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS),
			callback);
      else
	i_stage2(p,beginword,
		      endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS),
		      callback);
    }else
      i_end_case(p,endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS),
		 callback); /* only trips if we're already done */
    
    if(!(rb(root)==-1 || rb(root)>beginword || 
	 re(root)<endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS))) 
      break;
    
    /* Hmm, need more.  Read another block */

    {    
      c_block *new=i_read_c_block(p,beginword,endword,callback);
      
      if(new){
	if(p->enable&(PARANOIA_MODE_OVERLAP|PARANOIA_MODE_VERIFY)){
      
	  if(p->enable&PARANOIA_MODE_VERIFY)
	    i_stage1(p,new,callback);
	  else{
	    /* just make v_fragments from the boundary information. */
	    long begin=0,end=0;
	    
	    while(begin<cs(new)){
	      while(end<cs(new)&&(new->flags[begin]&1))begin++;
	      end=begin+1;
	      while(end<cs(new)&&(new->flags[end]&1)==0)end++;
	      {
		new_v_fragment(p,new,begin+cb(new),
			       end+cb(new),
			       (new->lastsector && cb(new)+end==ce(new)));
	      }
	      begin=end;
	    }
	  }
	  
	}else{

	  if(p->root.vector)i_cblock_destructor(p->root.vector);
	  free_elem(new->e,0);
	  p->root.vector=new;

	  i_end_case(p,endword+(MAX_SECTOR_OVERLAP*CD_FRAMEWORDS),
			  callback);
      
	}
      }
    }

    /* Are we doing lots of retries?  **************************************/
    
    /* Check unaddressable sectors first.  There's no backoff here; 
       jiggle and minimum backseek handle that for us */
    
    if(rb(root)!=-1 && lastend+588<re(root)){ /* If we've not grown
						 half a sector */
      lastend=re(root);
      retry_count=0;
    }else{
      /* increase overlap or bail */
      retry_count++;
      
      /* The better way to do this is to look at how many actual
	 matches we're getting and what kind of gap */

      if(retry_count%5==0){
	if(p->dynoverlap==MAX_SECTOR_OVERLAP*CD_FRAMEWORDS ||
	   retry_count==20){
	  if(!(p->enable&PARANOIA_MODE_NEVERSKIP))verify_skip_case(p,callback);
	  retry_count=0;
	}else{
	  if(p->stage1.offpoints!=-1){ /* hack */
	    p->dynoverlap*=1.5;
	    if(p->dynoverlap>MAX_SECTOR_OVERLAP*CD_FRAMEWORDS)
	      p->dynoverlap=MAX_SECTOR_OVERLAP*CD_FRAMEWORDS;
	    (*callback)(p->dynoverlap,PARANOIA_CB_OVERLAP);
	  }
	}
      }
    }
  }
  p->cursor++;

  return(rv(root)+(beginword-rb(root)));
}

/* a temporary hack */
void paranoia_overlapset(cdrom_paranoia *p, long overlap){
  p->dynoverlap=overlap*CD_FRAMEWORDS;
  p->stage1.offpoints=-1; 
}
