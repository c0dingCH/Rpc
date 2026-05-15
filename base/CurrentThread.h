#pragma once


#include<stdint.h>
#include<stdio.h>
#include<pthread.h>

namespace CurrentThread{
  extern __thread int t_cacheTid;
  extern __thread char t_formattedTid[32];
  extern __thread int t_formattedTidLength;

  void CacheTid();
  pid_t gettid();

  inline int tid(){
    if(__builtin_expect(t_cacheTid == 0, 0)){
      CacheTid();
    }
    return t_cacheTid;
  }

  inline const char * tidString(){ return t_formattedTid; }
  inline int tidStringLength(){ return t_formattedTidLength; }
}
