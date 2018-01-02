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

struct mem_arg {
  void *offset;
  void *patch;
  off_t patch_size;
  const char *fname;
  volatile int stop;
  volatile int success;
};

int pid;

int ptrace_memcpy(pid_t pid, void *dest, const void *src, size_t n) {
  const unsigned char *s;
  unsigned long value;
  unsigned char *d;

  d = dest;
  s = src;
  printf("GNAK!\n");
  //printf("%ld",(long)d);

  while (n >= sizeof(long)) {
    memcpy(&value, s, sizeof(value));

    if (ptrace(PTRACE_POKETEXT, pid, dest, value) == -1) {
      printf("%d\n",pid);
      return -1;
    }

    n -= sizeof(long);
    d += sizeof(long);
    s += sizeof(long);

  }

  if (n > 0) {
    d -= sizeof(long) -n;

    errno = 0;
    value = ptrace(PTRACE_PEEKTEXT, pid, d, NULL);
    if (value = -1 && errno != 0) {
      return -1;
    }

    memcpy ((unsigned char*)&value+sizeof(value) - n, s, n);
    if (ptrace(PTRACE_POKETEXT, pid, d, value) == -1) {
      return -1;
    }
  }
  return 0;
}

void *ptracethread(void *arg) {
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

  printf("KYKELIKYYYY\n");
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

  size_t size = st2.st_size;

  (*marg).patch = malloc(size);
  (*marg).patch_size = size;

  (*marg).fname = argv[2];

  read(f2,(*marg).patch, size);
  close(f2);

  void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, f, 0);

  if (map == MAP_FAILED) {
    printf("GOGLEA");
    return(-1);
  }

  //printf("%ld\n", (long)map);

  (*marg).offset = map;
  
  marg->stop = 0;
  marg->success = 0;

  pid = fork();
  pthread_t pt1,pt2;
  if (pid) {
    pthread_create(&pt1, NULL, ptracethread, marg);
    //printf("%ld\n",ptrace(PTRACE_ATTACH,pid));
    waitpid(pid,NULL,0);
    printf("GONGGGGG\n");
    pthread_join(pt1,NULL);
  } else {
    pthread_create(&pt2, NULL, madvthread, marg);
    ptrace(PTRACE_TRACEME);
    kill(getpid(),SIGSTOP);
    pthread_join(pt2,NULL);
  }
}
