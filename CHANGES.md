#CHANGES
=======
This file contains a brief summary of changes made from previous versions of
the connector.


1.0.5 - November 2015
----------------------------
- Based on [MySQL Connector/Arduino 1.0.4ga](https://launchpad.net/mysql-arduino).
- Added support for ESP8266
- Added code to automatically detect if using AVR Arduino, SAM, SAMD, or ESP8266 based on board selection in Arduino IDE. 
  - Works with Arduino AVR boards, SAM boards (Arduino Due), SAMD boards (Arduino Zero) and ESP8266.
  - Code verified using Arduino IDE 1.6.5.
- WITH_SELECT is turned *ON* by default.

[Chad Cormier Roussel's branch](https://github.com/chadouming/mysql-connector)
---------------------------------------
1.0.4 - July 2015
--------------------
* Modified to run solely on ESP8266.
* Modified to use Hash library in place of SHA1.
* Based on 1.0.3rc of Dr. Bell's original library.


[Original by Dr. Charles Bell](https://launchpad.net/mysql-arduino)
--------------------------------------------
1.0.4ga - July 2015
--------------------
* Fixed a defect in the get_next_row() method.
* Added the reference manual. Yippee!

1.0.3rc - March 2015
--------------------
* Code has been changed slightly to help with long latency issues over
  wifi and slow connections.
* A new cleanup method was added to cleanup a final OK packet after a
  stored procedure call with a result.
* Code now compiles without errors for the latest Beta Arduino IDE 
  (const error).

1.0.2b - April 2014
-------------------
* The WITH_SELECT is turned *OFF* by default. If you want to use select
  queries, be sure to uncomment this in the mysql.h file.
* Reduced memory use! The library has been trimmed to save memory.
  - All static strings moved to PROGMEM strings
  - Unused structures removed (e.g. ok_packet)
  - Moved more methods not needed to optional compilation

1.0.1b - February 2014
----------------------
* Added disconnect() method for disconnecting from server.
* Improved error handling for dropped packets.
* Better error handling for lost connections and out of memory detection.

1.0.0b - October 2013
---------------------
* Improved support for result sets (select queries)
* Added conditional compile for use with select queries. If you don't use
  select queries, comment out WITH_SELECT in mysql.h to save some memory.
* Added support for WiFi shield using conditional compilation.
* New version() method to check version of the connector.
* Simplified, single-file download

Initial Release - April 2013
----------------------------
* Basic query capability
* Basic result set handling
