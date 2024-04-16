#include  <stdio.h>
#include  <signal.h>
#include <stdlib.h>

void     INThandler(int);

void  main(void)
{
     signal(SIGINT, INThandler);
     while (1)
          pause();
}

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf("OUCH, did you hit Ctrl-C?\n"
            "Do you really want to quit? [y/n] ");
     c = getchar();
     if (c == 'y' || c == 'Y')
          exit(0);
     else
          signal(SIGINT, INThandler);
}