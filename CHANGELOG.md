# Changelog

## [1.5.8] - February 4, 2025

### Fixed
- Fix runtime configuration for ext_client

## [1.5.5] - January 25, 2025

### Fixed
- Fix segmentation fault on send

### Libraries
- pinex-1.2.1

## [1.5.0] - January 3, 2025

### Changed
- Update paper's proto

### Libraries
- pinex-1.2.0

## [1.4.8] - December 31, 2024

### Fixed
- Fix part number in multipart submit
- Fix deliver multipart's header

## [1.4.8] - December 12, 2024

### Enhanced
- Add support for changing ext_client's configuration at runtime

## [1.4.6] - November 25, 2024

### Libraries
- pinex-1.1.0

## [1.4.4] - November 11, 2024

### Fixed
- Fix name of `timeout` in config

## [1.4.3] - November 10, 2024

### Fixed
- Fix message body decoding

## [1.4.0] - October 10, 2024

### Added
- Add send and receive flow_control for external_client
- Add timeout for external_client

## [1.3.9] - October 10, 2024

### Fixed
- close connection before deleting an external_client at runtime

## [1.3.7] - September 30, 2024

### Changed

- set message_id's mode in external_client

## [1.3.1] - August 15, 2024

### Changed

- clean paper in submit_sm

## [1.3.0] - September 18, 2022

### Changed

- Replace ZMQ connection of policy with asio


## Version 1.2.13
- Release date: May 07, 2022

### Fixed
- Fix creating new client at runtime

### Libraries
- pa-thread-pool-2.3.1

## Version 1.2.12
- Release date: April 15, 2022

### Libraries
- pa-zmqp-3.1.4

## Version 1.2.11
- Release date: April 05, 2022

### Fixed
- Fix message trace log

## Version 1.2.10
- Release date: March 09, 2022

### Fixed
- Fix delay on sending packet

### Libraries
- smsc-network-interface-1.2.1

## Version 1.2.9
- Release date: February 12, 2022

### Libraries
- pa-managed-object-web-server-1.7.0

## Version 1.2.7
- Release date: January 24, 2022

### Enhanced:
- Add comma seperated ip checking

### Libraries
- smsc-network-interface-1.2.0
- pa-core-2.2.0
- pa-routing-2.3.0
- pa-thread-pool-2.3.0
- pa-managed-object-web-server-1.6.0

## Version 1.2.6
- Release date: January 11, 2022

### Added
- Add new connection info to monitoring

## Version 1.2.5
- Release date: December 13, 2021

### Fixed
- Fix destination in CP checking

## Version 1.2.4
- Release date: Octoberr 31, 2021

### Fixed
- Fix monitoring on adding new external client

## Version 1.2.3
- Release date: Octoberr 25, 2021

### Fixed
- Fix adding new external client

## Version 1.2.2
- Release date: September 22, 2021

### Fixed
- Fix removing connection on disconnect

### Libraries
- smsc-network-interface-1.1.8
- pa-core-2.1.2

## Version 1.2.1
- Release date: August 23, 2021

### Fixed
- Fix black/white list command

### Libraries
- smsc-network-interface-1.1.4

## Version 1.2.0
- Release date: August 17, 2021

### Enhanced
- Add policy per connection
- Log rejected packet in POLAR

### Added
- Add BlackWhite list checking

## Version 1.1.7
- Release date: August 17, 2021

### Added
- Add Boninet connection monitoring

### Fixed
- Fix config file writing on reconfiguration

### Libraries
- smsc-network-interface-1.1.3
- pa-core-2.1.1

## Version 1.1.6
- Release date: July 31, 2021

### Added
- Add deliver report's status to monitoring

## Version 1.1.5
- Release date: July 28, 2021

### Changed
- Change monitoring name

## Version 1.1.4
- Release date: July 26, 2021

### Enhanced
- Generate core file on segmentation fault

## Version 1.1.3
- Release date: July 26, 2021

### Fixed
- Fix unnecessary logging

## Version 1.1.2
- Release date: July 25, 2021

### Fixed
- Fix segmentation fault on routing

### Libraries
- smsc-network-interface-1.1.1

## Version 1.1.1
- Release date: July 20, 2021

### Changed
- Add SMSC unique id in DeliverResp packet

## Version 1.1.0
- Release date: July 19, 2021

### Changed
- Add reverse routing for network interface

## Version 1.0.0

- Release date: July 14, 2020
- Used library: tags-4.0.3
- Protobuf interface version: master

### What's new in 1.0.0

- Release first version
