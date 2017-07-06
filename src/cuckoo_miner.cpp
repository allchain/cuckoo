// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2016 John Tromp

#include "cuckoo_miner.hpp"
#include <unistd.h>

#define MAXSOLS 8

extern "C" int cuckoo_call(char* header_data, 
                           int header_length,
                           int nthreads,
                           int ntrims, 
                           u32* sol_nonces){
  
  char header[HEADERLEN];
  int c;
  int nonce = 0;
  int range = 1;

  if (ntrims==0) ntrims = 1 + (PART_BITS+3)*(PART_BITS+4)/2;

  assert(nthreads>0);

  assert(header_length <= sizeof(header));

  memset(header, 0, sizeof(header));

  memcpy(header, header_data, header_length);

  print_buf("Coming in is: ", (const unsigned char*) &header, header_length);

  //memset(header, 0, sizeof(header));
  /*while ((c = getopt (argc, argv, "h:m:n:r:t:")) != -1) {
    switch (c) {
      case 'h':
        len = strlen(optarg);
        assert(len <= sizeof(header));
        memcpy(header, optarg, len);
        break;
      case 'n':
        nonce = atoi(optarg);
        break;
      case 'r':
        range = atoi(optarg);
        break;
      case 'm':
        ntrims = atoi(optarg);
        break;
      case 't':
        nthreads = atoi(optarg);
        break;
    }
  }*/

  printf("Looking for %d-cycle on cuckoo%d(\"%s\",%d", PROOFSIZE, EDGEBITS+1, header, nonce);
  if (range > 1)
    printf("-%d", nonce+range-1);
  printf(") with 50%% edges, %d trims, %d threads\n", ntrims, nthreads);

  u64 edgeBytes = NEDGES/8, nodeBytes = TWICE_ATOMS*sizeof(atwice);
  int edgeUnit, nodeUnit;
  for (edgeUnit=0; edgeBytes >= 1024; edgeBytes>>=10,edgeUnit++) ;
  for (nodeUnit=0; nodeBytes >= 1024; nodeBytes>>=10,nodeUnit++) ;
  printf("Using %d%cB edge and %d%cB node memory, %d-way siphash, and %d-byte counters\n",
     (int)edgeBytes, " KMGT"[edgeUnit], (int)nodeBytes, " KMGT"[nodeUnit], NSIPHASH, SIZEOF_TWICE_ATOM);

  thread_ctx *threads = (thread_ctx *)calloc(nthreads, sizeof(thread_ctx));
  assert(threads);
  cuckoo_ctx ctx(nthreads, ntrims, MAXSOLS);

  u32 sumnsols = 0;
  for (int r = 0; r < range; r++) {
    //ctx.setheadernonce(header, sizeof(header), nonce + r);
    ctx.setheadergrin(header, sizeof(header), nonce + r);
    printf("k0 %lx k1 %lx\n", ctx.sip_keys.k0, ctx.sip_keys.k1);
    for (int t = 0; t < nthreads; t++) {
      threads[t].id = t;
      threads[t].ctx = &ctx;
      int err = pthread_create(&threads[t].thread, NULL, worker, (void *)&threads[t]);
      assert(err == 0);
    }
    for (int t = 0; t < nthreads; t++) {
      int err = pthread_join(threads[t].thread, NULL);
      assert(err == 0);
    }
    for (unsigned s = 0; s < ctx.nsols; s++) {
      printf("Solution");
      //just return with the first solution we get
      for (int i = 0; i < PROOFSIZE; i++) {
        printf(" %jx", (uintmax_t)ctx.sols[s][i]);
        sol_nonces[i] = ctx.sols[s][i]; 
      }
      free(threads);
      printf("\n");
      return 1;
    }
    sumnsols += ctx.nsols;
  }
  free(threads);
  printf("%d total solutions\n", sumnsols);
  return 0;
}

extern "C" void cuckoo_description(char * name_buf,
                              int* name_buf_len,
                              char *description_buf,
                              int* description_buf_len){

  int ntrims   = 1 + (PART_BITS+3)*(PART_BITS+4)/2;
  
  //TODO: check we don't exceed lengths.. just keep it under 256 for now
  int name_buf_len_in = *name_buf_len;
  const char* name = "cuckoo_basic_%d\0";
  sprintf(name_buf, name, EDGEBITS+1);
  *name_buf_len = strlen(name);
  
  const char* desc1 = "Looks for a %d-cycle on cuckoo%d with 50%% edges using basic algorithm.\n \
Uses %d%cB edge and %d%cB node memory, %d-way siphash, and %d-byte counters.\0";

  u64 edgeBytes = NEDGES/8, nodeBytes = TWICE_ATOMS*sizeof(atwice);
  int edgeUnit, nodeUnit;
  for (edgeUnit=0; edgeBytes >= 1024; edgeBytes>>=10,edgeUnit++) ;
  for (nodeUnit=0; nodeBytes >= 1024; nodeBytes>>=10,nodeUnit++) ;
  sprintf(description_buf, desc1,     
  PROOFSIZE, EDGEBITS+1, (int)edgeBytes, " KMGT"[edgeUnit], (int)nodeBytes, " KMGT"[nodeUnit], NSIPHASH, SIZEOF_TWICE_ATOM);
  *description_buf_len = strlen(description_buf);
 
}

