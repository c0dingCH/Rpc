#!/bin/bash
ZOOKEEPER_HOME=/usr/share/zookeeper
CONF=/etc/zookeeper/conf_example/zoo.cfg

if [ -f /tmp/zookeeper/zookeeper_server.pid ]; then
  echo "ZooKeeper appears to be already running (PID: $(cat /tmp/zookeeper/zookeeper_server.pid))"
  exit 0
fi

"$ZOOKEEPER_HOME/bin/zkServer.sh" start "$CONF"
