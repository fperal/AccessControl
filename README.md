# AccessControl

This project control an electronics lock to give access to any place trough a door using RFID tags.
It is based in arduino yun and RMD6300
It mantais a file with a database (file "users") with the format tag_id:username  with the authorized tags in the linux part of the arduino yun and a access log.
It has too a calendar file (file "calendar") with format HH:MM/O   or HH:MM/C  setting different hours in wich the door is open (it can be openned pushing a push button with no need of RFID tag)
It has a log file (file "accesslog") where all acces are logged.
