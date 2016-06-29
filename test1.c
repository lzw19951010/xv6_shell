#include "types.h"
#include "stat.h"
#include "user.h"

#define NPROC        64  
#define KEY_UP    0xE2
#define KEY_DN    0xE3
#define MAXCURSORPOS ((NPROC + 2)*18)
#define POSINSCREEN(cursorPos)   (((cursorPos / 80 - 2)%18 + 2) * 80)
#define POSINFILE(cursorPos)    (cursorPos / 80)
#define STARTPOSINFILE(cursorPos)  (((cursorPos / 80 - 2)/18 )* 18 + 2)
#define STARTPOSINSCREEN  (2 * 80)

int cursorPos = 2 * 80;
int realPos = 0;
int pageNum = 0;
char fileBuf[80][80];
char controlBuf[2];
char detailBuf[3 * 80];
char statusBuf[80];
char statusTempt[80];
char count1[2] = "\0\0";
int procId[NPROC][1];
char procName[NPROC][16];
int procState[NPROC][1];
uint procSize[NPROC][1];
//int countTempt = 0;

void printOtherInfo(char*, char*, int, int, ...);
void printProcInfo(char *fmt, int pos,  ...);
void printTaskManagerInfo();
void testChange();
void showProcInfo();
void createTestProc();
int getProcIndex();
void clearBuf();
void resetCursorPos();
void getControl() //��ȡ������������İ�������
{
   memset(controlBuf, 0, sizeof(char) * 2);
   gets(controlBuf, 2);
char* tempt =controlBuf;
//printf(1, "%d\n", *tempt);
  while(*tempt)
  {
	  switch((*tempt) & 0xff)
	  {
		  case KEY_UP:
			 if(cursorPos > 2 * 80)
			 {
				 cursorPos -= 80;
			 }
			 if((cursorPos/80 - 2)%18 == (18 - 1))
			 {
				 readProcMemory(fileBuf[cursorPos/ 80 - (18 - 1)],  18 * 80, 0, 0);
				pageNum = (cursorPos / 80 - 2)/18 + 1;
				printOtherInfo(statusTempt, statusBuf, 80, 24 * 80, pageNum);
			 }
			 else
			 {
				 readProcMemory(fileBuf[POSINFILE(cursorPos)], 80, 0, POSINSCREEN(cursorPos));
			 }
			 break;
		  case KEY_DN:
                          
			  if(cursorPos < realPos - 80)
			  {
				  cursorPos += 80;
			  }
			  if((cursorPos / 80 - 2)%18 ==0)
			  {
				  readProcMemory(fileBuf[cursorPos/ 80], 18 * 80, 1, 0);
				  pageNum = (cursorPos / 80 - 2)/18 + 1;
				  printOtherInfo(statusTempt,statusBuf, 80, 24 * 80, pageNum);
			  }
			  else
			  {
				   readProcMemory(fileBuf[POSINFILE(cursorPos)],  80, 1, POSINSCREEN(cursorPos));
			  }
			  break;
		case 'k':
			kill(procId[getProcIndex()][0]);
			printTaskManagerInfo();
			break;
		case 'f':
			printTaskManagerInfo();
			break;

	  }
          tempt++;
  }
} 

int getProcIndex() //��ȡ���̽ṹ����Ϣ�������
{
	if(cursorPos >= 2 * 80)
	{
    return (cursorPos / 80 - 2);
	}
	else
	{
		return -1;
	}
}
void createTestProc()
{
	int i;
	for (i = 0; i < 1; ++i)
	{
		fork();
	}
	printTaskManagerInfo();
}
int printint(int xx, int base, int sign, int* inputTempt, int max, char* fBuf) //���������ַ�����ʽ��ӡ���ڲ��������ǽӿ�
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
  {
	  if(*inputTempt == max)
	  {
		  return 0;
	  }
   fBuf[*inputTempt] = buf[i];
   (*inputTempt)++;
  }
  return 1;
}

void printTaskManagerInfo()
{
	clearBuf();
	int i;
	char* tagTempt = "page: %d\n";
	printProcInfo( "proc task manager\n" , 0);
	printProcInfo( "id  name  state  memory\n" , 80);
	//printProcInfo( "%d\n" , 160, result);
	getProcInfo((int**)(procId[0]),(char**)procName,(int**)procState,(uint**)procSize);
	for ( i = 0; i < 64; i++)
	{
		printProcInfo("%d  %s  %d      %dbytes\n", (i + 2)*80,procId[i][0],procName[i],procState[i][0],procSize[i][0]);
	}
	showProcInfo();
	for(i = 0; i < (80 - 10); i++) statusTempt[i] = ' ';
	i = 0;
	while(tagTempt[i])
	{
		statusTempt[80 - 10 + i] = tagTempt[i];
		i++;
	}
	statusTempt[80 - 10  + i] = 0;
	printOtherInfo(statusTempt, statusBuf, 80, 24 * 80, 1); //��ӡ״̬��Ϣ������
	printOtherInfo("this is a proc task manager\n", detailBuf, 3 * 80, 21 * 80); //��ӡ��ϸ��Ϣ������
	for(;;) getControl();
	resetCursorPos();
	showProcInfo();
}
/*void changeProcInfo() //���Ĺ������ҳ����ʾ��Ϣ
{
	readProcMemory( fileBuf[STARTPOSINFILE(cursorPos)], 18 * 80, 2, STARTPOSINSCREEN);
	readProcMemory( fileBuf[POSINFILE(cursorPos)], 80, 1, POSINSCREEN(cursorPos));
}*/
void clearBuf() //��մ洢�Ľ�����Ϣ
{
        
	memset(fileBuf[2], 0, sizeof(char) * 80 * NPROC);

	readProcMemory(fileBuf[2], 18 * 80, 2, 80 * 2);
        realPos = 160;
}

void resetCursorPos() //���¶�λ����λ��
{
	while(cursorPos > realPos)
	{
		if(cursorPos / 80 >= (2 + 18))
		{
			cursorPos -= 18 * 80;
		}
		else
		{
			cursorPos = 2 * 80;
			break;
		}
	}
        pageNum = (cursorPos / 80 - 2)/18 + 1;
	printOtherInfo(statusTempt, statusBuf, 80, 24 * 80, pageNum);
}

void testChange() //���´�ӡ��Ϣ������
{
	int i;
	clearBuf();
    for(i = 0; i < 5; i++)
	{
		printProcInfo("%d\n",(i +2)*80, i);
	}
	resetCursorPos();
	showProcInfo();
}
void printProcInfo(char *fmt, int pos,  ...) //��ӡ������Ϣ����cprintfһ����ʽ
{

	int i, c;

	memset(fileBuf[pos / 80], 0 ,sizeof(char) * 80); 

  uint *argp;
  char *s;
  if (fmt == 0)
    printf(1, "null fmt\n");
  int inputTempt = 0;
   argp = (uint*)(void*)(&fmt + 2);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
	  if(inputTempt == 80)
	  {
		goto bad;
	  }
    if(c != '%'){
		if(c != '\n')
		{
           fileBuf[pos / 80][inputTempt++] = c;
		}
		else
		{
			fileBuf[pos / 80][inputTempt++] = 0;
			inputTempt += 80 - inputTempt % 80;
		}
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      if(!printint(*argp++, 10, 1, &inputTempt, 80, fileBuf[pos / 80]))
	  {
		  goto bad;
	  }
      break;
    case 'x':
    case 'p':
      if(!printint(*argp++, 16, 0, &inputTempt, 80, fileBuf[pos / 80]))
	  {
		  goto bad;
	  }
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
	  {
		if(inputTempt >= 80)
		{
			goto bad;
		}
        fileBuf[pos / 80][inputTempt++] = (*s);
	  }
      break;
    case '%':
      fileBuf[pos / 80][inputTempt++] = c;
      break;
    default:
      // Print unknown % sequence to draw attention.
		fileBuf[pos / 80][inputTempt++] = '%';
	  if(inputTempt >= 80)
	  {
		goto bad;
	  }
      fileBuf[pos / 80][inputTempt++] = c;
      break;
    }
  }
if(realPos < (pos +80))
{
	realPos = (pos +80) ;
}
  return;
bad:
   printf(1,"the proc info is too long! invalid\n");
		 while(--inputTempt >= 0)
		 {
			 fileBuf[pos / 80][inputTempt] = '\0';
		 }
		return;
}

void showProcInfo() //����Ļ��ʾ���̵���Ϣ
{
	readProcMemory(fileBuf[0], 2 * 80, 2, 0);
	readProcMemory(fileBuf[STARTPOSINFILE(cursorPos)], 18 * 80, 2, STARTPOSINSCREEN);
	readProcMemory(fileBuf[cursorPos / 80], 80, 1, POSINSCREEN(cursorPos));
}

void printOtherInfo(char *fmt, char* buf, int maxNum,int outPos,  ...) //��ӡ������Ϣ�����������ӡ�ַ������洢�Ļ�����buf����󳤶Ⱥ������Ļλ�ã������һЩ��������
	                                                                   // �磺printOtherInfo("try to count %d %d %d\n", detailBuf, 240, 1, 2,3)
{	
	int i, c;
	memset(buf, 0, maxNum * sizeof(char));
  uint *argp;
  char *s;
  if (fmt == 0)
    printf(1, "null fmt\n");
  int inputTempt = 0;
   argp = (uint*)(void*)(&fmt + 4);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
	  if(inputTempt == maxNum)
	  {
		goto bad;
	  }
    if(c != '%'){
		if(c != '\n')
		{
           buf[inputTempt++] = c;
		}
		else
		{
			buf[inputTempt++] = 0;
			inputTempt += 80 - inputTempt % 80;
		}
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      if(!printint(*argp++, 10, 1, &inputTempt, maxNum, buf))
	  {
		  goto bad;
	  }
      break;
    case 'x':
    case 'p':
      if(!printint(*argp++, 16, 0, &inputTempt, maxNum, buf))
	  {
		  goto bad;
	  }
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
	  {
		if(inputTempt >= maxNum)
		{
			goto bad;
		}
        buf[inputTempt++] = (*s);
	  }
      break;
    case '%':
      buf[inputTempt++] = c;
      break;
    default:
      // Print unknown % sequence to draw attention.
		buf[inputTempt++] = '%';
	  if(inputTempt >= maxNum)
	  {
		goto bad;
	  }
      buf[inputTempt++] = c;
      break;
    }
  }
  readProcMemory(buf, maxNum, 2, outPos);
  return;
  bad:
   printf(1,"the  info is too long! invalid\n");
		 while(--inputTempt >= 0)
		 {
			 buf[inputTempt] = '\0';
		 }
		return;
}





int main()
{
	
	/*int result =*/ 
	setconsole(1);
	createTestProc();
	//printTaskManagerInfo();
	return 0;

}
