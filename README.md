# SmppGateway

## Description

_**`SmppGateway`**_ Exchanges SMPP's messages between Short Message Service Centers (SMSC) and/or External Short Messaging Entities (ESME).

It converts message between SMPP and ZMQ interfaces.

It also can query to PAPER for policies checking.

## License

This software is [licensed under Peykasa co](https://peykasa.ir).

## Requirement

* [C++11] It requires `C++11`.
* [[protobuf](https://github.com/google/protobuf.git)] for serializing datas. `minimum required version is 3.7.1`
* [[restbed](https://github.com/corvusoft/restbed.git)] the asynchronous RESTful functionality
* [[jwt](https://github.com/benmcollins/libjwt.git)] the JWT C Library
* [[zmq](https://github.com/zeromq/libzmq.git)] ZeroMQ core engine in C++

## Change Log

See the [change log](/CHANGELOG.md) for a detailed list of changes in each version.

## Author

* **Majid Darvishan** - <majid.darvishan@peykasa.ir>