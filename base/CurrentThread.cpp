#include"CurrentThread.h"
#include<unistd.h>
#include<stdio.h>
#include<sys/syscall.h>
namespace CurrentThread{

  __thread int t_cacheTid = 0;
  __thread char t_formattedTid[32];
  __thread int t_formattedTidLength;

  pid_t gettid(){
    return static_cast<int>(syscall(SYS_gettid));
  }

  void CacheTid(){
    if(t_cacheTid == 0){
      t_cacheTid = gettid();
      t_formattedTidLength = snprintf(t_formattedTid, sizeof t_formattedTid, "%05d", t_cacheTid);
    }
  }


}
