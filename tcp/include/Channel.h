#pragma once
#include<sys/epoll.h>
#include<functional>
#include"Common.h"
#include<memory>
class EventLoop;

class Channel{
public:
  Channel(EventLoop * loop, int fd);
  ~Channel();

  DISALLOW_COPY_AND_MOVE(Channel);

  void EnableRead();
  void EnableWrite();
  void EnableET();
  void DisableWrite();
  void DisableAll();

  void HandleEvent();
  void HandleEventWithGaurd();

  int GetFd() const;
  short GetListenEvents() const;
  short GetReadyEvents() const;
  
  bool GetExist() const;
  void SetExist(bool exist);

  void SetReadyEvents(short ev);  
  void SetReadCallback(const std::function<void()> & cb);
  void SetWriteCallback(const std::function<void()>& cb);

  void Tie(const std::shared_ptr<void> &ptr);
  bool IsWriting();

private:
  EventLoop* loop_{nullptr};
  int fd_{-1};
  short listen_events_;//epoll监听事件的集合
  short ready_events_;//epoll得到的发生事件的集合
  bool exist_{false};

  std::function<void()>read_;
  std::function<void()>write_;

  bool tied_{false};
  std::weak_ptr<void>tie_;

};  

