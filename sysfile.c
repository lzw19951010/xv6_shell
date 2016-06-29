//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "console.h"


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
extern int consoleMode;
extern int changeFlag;
extern char* bufMemoryPoint;
extern int memorySize;
extern ushort* crt;
struct consLock;
extern struct consLock cons;
extern int changeFlag;
struct inputStruct;
extern struct inputStruct input;
extern int realPos;
#define UP 0
#define DOWN 1
#define CHANGE 2
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=proc->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;

  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd] == 0){
      proc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;
  
  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;
  
  if(argfd(0, &fd, &f) < 0)
    return -1;
  proc->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;
  
  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;
  if((ip = namei(old)) == 0)
    return -1;

  begin_trans();

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    commit_trans();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  commit_trans();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  commit_trans();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;
  if((dp = nameiparent(path, name)) == 0)
    return -1;

  begin_trans();

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  commit_trans();

  return 0;

bad:
  iunlockput(dp);
  commit_trans();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  uint off;
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, &off)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;
  if(omode & O_CREATE){
    begin_trans();
    ip = create(path, T_FILE, 0, 0);
    commit_trans();
    if(ip == 0)
      return -1;
  } else {
    if((ip = namei(path)) == 0)
      return -1;
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    return -1;
  }
  iunlock(ip);

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_trans();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    commit_trans();
    return -1;
  }
  iunlockput(ip);
  commit_trans();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int len;
  int major, minor;
  
  begin_trans();
  if((len=argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    commit_trans();
    return -1;
  }
  iunlockput(ip);
  commit_trans();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;

  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0)
    return -1;
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    return -1;
  }
  iunlock(ip);
  iput(proc->cwd);
  proc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      proc->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int 
sys_setconsole(void)
{
    int mode;
int pos;
int i;
    if (argint(0, &mode) < 0)
        mode = 0;
	if(consoleMode == 0 && mode == 1)
	{
		changeFlag = 1;
              acquire(&cons.lock);
	     while(i != realPos)  //清空输出屏
	     {
	        crt[i++] = '\0';
             }
	     changeFlag = 0;
             pos = 80 * 25;   //消失鼠标
	     outb(CRTPORT, 14);
             outb(CRTPORT+1, pos>>8);
             outb(CRTPORT, 15);
             outb(CRTPORT+1, pos);
          release(&cons.lock);
	  acquire(&input.lock);
	  input.r = input.w = input.e = 0;
	  release(&input.lock);
	}
    consoleMode = mode;
    return 0;
}

int sys_readProcMemory(void)
{
	int i;
	int mode = 0;
	int pos = 0;
	if(argint(1, &memorySize) < 0)
	{
		memorySize = 0;
		return -1;
	}
	if(argptr(0, &bufMemoryPoint, memorySize) < 0)
	{
		bufMemoryPoint = 0;
		return -1;
	}
	if(argint(2, &mode) < 0 )
	{
		mode = 0;
		return -1;
	}
	if(argint(3, &pos) < 0 )
	{
		pos = 0;
		return -1;
	}
          acquire(&cons.lock);
	if(mode != CHANGE && pos == 0) //上下键翻页
	{
            pos = 2 * 80;
	    for(i = 0; i < memorySize ; i++) //输入文件缓存区的内容
	    {
		    crt[pos + i] = (bufMemoryPoint[i]&0xff)|0x700;
	    }
	    for(i = memorySize; i < 18 * 80; i++)
	    {
		   crt[pos + i] = '\0'|0x700;
	    }
	  if(mode == DOWN) //向下翻，将最上行高亮
	  {
	   for(i = 160; i < 240; i++)
	  {
		  crt[i] = crt[i] & 0xff;
		  crt[i] = crt[i] | 0x900;
	  }
	  }
	  else if(mode == UP) //向上翻，将最下行高亮
	  {
		  for(i = 19 * 80; i < 20 * 80; i++)
		  {
			  crt[i] = crt[i] & 0xff;
		      crt[i] = crt[i] | 0x900;
		  }
	  }
	}
	else if(mode != CHANGE && pos!= 0) //上下键不翻页，更改高亮位置
	{
                if(mode == UP)
               {
		for(i = 0; i <  memorySize; i++)
		{
			crt[pos + i + 80] &=  0xff;
			crt[pos + i + 80] |= 0x700;
		}
               }
               else if(mode == DOWN)
               {
		for(i = 0; i <  memorySize; i++)
		{
			crt[pos + i - 80] &=  0xff;
			crt[pos + i - 80] |= 0x700;
		}
               }
		for(i = 0; i <  memorySize; i++)
		{
			crt[pos + i] = crt[pos + i] & 0xff;
			crt[pos + i] = crt[pos + i]| 0x900;
		}
	}

	else if(mode == CHANGE) //仅进行文本替换，不改变高亮位置
	{
            
	    for(i = 0; i <  memorySize; i++)
		{
			crt[pos + i] = (bufMemoryPoint[i]& 0xff)| 0x700;
		}
	}
        release(&cons.lock);
  return 0;
}
int sys_getProcInfo(void){
  struct proc *p;
  char m_procId[64][1];
  char m_procName[64][16];
  char m_procState[64][1];
  char m_procSize[64][1];
  int i = 0;
  //int j = 0;

  if(argptr(0,(char**)m_procId,64*sizeof(int)) < 0) return -1;
  if(argptr(1,(char**)m_procName,64*16*sizeof(char)) < 0) return -1;
  if(argptr(2,(char**)m_procState,64*sizeof(int)) < 0) return -1;
  if(argptr(3,(char**)m_procSize,64*sizeof(uint)) < 0) return -1;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    **(int**)m_procId = p->pid;
    *(int**)m_procId += 1;
    for (i = 0; i < 16; ++i)
    {
      **(char**)m_procName= p->name[i];
      *(char**)m_procName += 1;

    }
    **(int**)m_procState = p->state;
    *(int**)m_procState += 1;
    **(int**)m_procSize = p->sz;
    *(int**)m_procSize += 1;
  }
  return 0;
}