sofa-pbrpc
==========
[![Build Status](https://travis-ci.org/baidu/sofa-pbrpc.svg)](https://travis-ci.org/baidu/sofa-pbrpc)

A light-weight RPC implement of google protobuf RPC framework.

Wiki: https://github.com/baidu/sofa-pbrpc/wiki

### Features
* High performace.
* Easy to use. Refer to sample codes in './sample'.
* Support sync call and async call. Refer to './sample/echo'.
* Support three level (service/method/request) timeout. Refer to './sample/timeout_sample'.
* Support transparent compression. Refer to './sample/compress_sample'.
* Support mock test. Refer to './sample/mock_sample'.
* Support network flow control.
* Support auto connecting and reconnecting.
* Support keep alive time of idle connections.
* Support statistics for profiling.
* Support multi-server load balance and fault tolerance.
* Support http protocol.
* Provide web monitor.
* Provide python client library.

### Dependings
This lib depends on boost-1.53.0 (only need header), protobuf-2.4.1, snappy and zlib:
* boost - http://www.boost.org/
* protobuf - http://code.google.com/p/protobuf/
* snappy - http://code.google.com/p/snappy/
* zlib - http://zlib.net/

ATTENTION: boost header is only needed when compiling the lib, but no need for user code.

Extrally, './unit-test' and './sample/mock_sample' also depends on gtest:
* gtest - http://code.google.com/p/googletest/

### Build
1. Modify the file './depends.mk' to specify depending libs.<br>
  The necessary libs is boost, protobuf, snappy, and zlib.
2. Run 'make' to build sofa-pbrpc.<br>
  The default optimization level is 'O2'.<br>
  To change it, modify the 'OPT' variable in file './Makefile'.
3. Run 'make install' to install sofa-pbrpc.<br>
  The default install directory is './output'.<br>
  To change it, modify the 'PREFIX' variable in file './Makefile'.

For more details, please refer to the wiki.

### Sample
For sample codes, please refer to './sample' and the wiki.

### Performance
For performace details, please refer to the wiki.

### Support
opensearch@baidu.com

