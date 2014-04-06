About
-----

**cecho - colored echo for Windows command processor [Apr  6 2014]**  
**Copyright (C) 2014 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.**

This program is free software; you can redistribute it and/or modify  
it under the terms of the GNU General Public License as published by  
the Free Software Foundation; either version 2 of the License, or  
(at your option) any later version.

This program is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of  
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along  
with this program; if not, write to the Free Software Foundation, Inc.,  
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


Usage
-----

```
cecho.exe <forground_color> [<background_color>] <echo_text>
```

Color names
-----------

```
0 -->        black  
1 -->        white  
2 -->         grey  
3 -->          red  
4 -->     dark_red  
5 -->        green  
6 -->   dark_green  
7 -->         blue  
8 -->    dark_blue  
9 -->       yellow  
a -->  dark_yellow  
b -->      magenta  
c --> dark_magenta  
d -->         cyan  
e -->    dark_cyan
```

Escape chars
------------

```
\n --> Line feed  
\t --> Tabulator  
\\ --> Backslash
```
   
Example
-------

```
cecho.exe blue yellow "Hello world!\nGoodbye cruel world!"
```
