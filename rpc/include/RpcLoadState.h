#pragma once

#include<iostream>

class RpcLoadState{
public:
  void EnLoad(){ count_++; }
  void DeLoad(){ count_--; }

  void AddLoad(long long rt);

  double rt(){ return rt_; }

private:
  int count_{0};
  double rt_{0};
  double alpha_{0.2};
};
