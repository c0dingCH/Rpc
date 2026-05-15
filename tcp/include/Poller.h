#pragma once

#include<sys/epoll.h>
#include<vector>
#include"Common.h"


class Channel;

class Poller{
public:
  Poller();
  ~Poller();
  DISALLOW_COPY_AND_MOVE(Poller);

  std::vector<Channel*> Poll(int timeout = -1);
  void UpdateChannel(Channel * channel);
  void DeleteChannel(Channel * channel);


private:
  int fd_{-1};
  std::vector<epoll_event> events_;
};

