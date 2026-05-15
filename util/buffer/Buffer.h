#pragma once
#include<string>
#include<vector>
#include"Common.h"

static const size_t kInitialSize = 1024;
static const size_t kPrePendIndex = 8;

class Buffer{
public:
  Buffer();
  ~Buffer();

  DISALLOW_COPY_AND_MOVE(Buffer);

  char *begin();
  const char *begin() const;

  char *beginread();
  const char *beginread() const;

  char *beginwrite();
  const char *beginwrite() const;


  void Append(const char *message);
  void Append(const char *message, size_t len);
  void Append(const std::string &message);

  size_t readablebytes() const;
  size_t writablebytes() const;
  size_t prependablebytes() const;


  char *Peek();
  const char *Peek() const;
  std::string PeekAsString(size_t len);
  std::string PeekAllAsString();

  void Retrieve(size_t len);
  std::string RetrieveAsString(size_t len);

  void RetrieveAll();
  std::string RetrieveAllAsString();

  void RetrieveUntil(const char *end);
  std::string RetrieveUntilAsString(const char *end);


  void EnsureWritableBytes(size_t len);

private:
  std::vector<char> buffer_;
  size_t read_index_;
  size_t write_index_;
  
};

