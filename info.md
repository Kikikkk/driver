```sh
set INCLUDE=C:\WinDDK\7600.16385.0\inc\ddk;%INCLUDE%
```

```sh
sc delete MyDriver
sc create MyDriver type= kernel binPath= "Z:\driver\objchk_wxp_x86\i386\SimpleDriver.sys"
sc start MyDriver
```

