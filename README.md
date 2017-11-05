# linux-varlink

Varlink kernel protocol driver module.
```
$ modinfo drivers/varlink/varlink.ko
filename:       /src/varlink/linux-varlink/drivers/varlink/varlink.ko
description:    Varlink Protocol Driver
license:        GPL
depends:        
name:           varlink
vermagic:       4.13.10-200.fc26.x86_64 SMP mod_unload 
```

Example driver using the varlink module. It creates a device node at /dev/org.kernel.example and offers two interfaces.
```
$ modinfo drivers/example/example.ko
filename:       /src/varlink/linux-varlink/drivers/example/example.ko
description:    Varlink Example
license:        GPL
depends:        varlink
name:           example
vermagic:       4.13.10-200.fc26.x86_64 SMP mod_unload 
```

Information about the `example` kernel driver module.
```
$ varlink info device:/dev/org.kernel.example
Vendor: Linux
Product: Varlink Example
Version: 1.0
URL: http://kernel.org
Interfaces:
  org.kernel.devices.usb
  org.kernel.sysinfo
```

Interface with `uname` information.
```js
$ varlink call device:/dev/org.kernel.example/org.kernel.sysinfo.GetInfo
{
  "domainname": "(none)",
  "machine": "x86_64",
  "nodename": "ank",
  "release": "4.13.10-200.fc26.x86_64",
  "sysname": "Linux",
  "version": "#1 SMP Fri Oct 27 15:34:40 UTC 2017"
}
```

Interface to enumerate and monitor USB devices, --more subscribes to changes. The log below shows events for the mouse removed and added back.
```js
$ varlink call --more device:/dev/org.kernel.example/org.kernel.devices.usb.Monitor
{
  "devices": [
    {
      "bus_nr": 1,
      "device_nr": 1,
      "manufacturer": "Linux 4.13.10-200.fc26.x86_64 xhci-hcd",
      "product": "xHCI Host Controller",
      "product_id": 2,
      "serial": "0000:00:14.0",
      "vendor_id": 7531
    },
    {
      "bus_nr": 2,
      "device_nr": 1,
      "manufacturer": "Linux 4.13.10-200.fc26.x86_64 xhci-hcd",
      "product": "xHCI Host Controller",
      "product_id": 3,
      "serial": "0000:00:14.0",
      "vendor_id": 7531
    },
    {
      "bus_nr": 1,
      "device_nr": 3,
      "manufacturer": "Yubico",
      "product": "Yubico Yubikey II",
      "product_id": 16,
      "serial": "0002883373",
      "vendor_id": 4176
    },
    {
      "bus_nr": 1,
      "device_nr": 5,
      "manufacturer": "Chicony Electronics Co.,Ltd.",
      "product": "Integrated Camera",
      "product_id": 46385,
      "serial": "0001",
      "vendor_id": 1266
    },
    {
      "bus_nr": 1,
      "device_nr": 6,
      "manufacturer": "",
      "product": "",
      "product_id": 144,
      "serial": "00ea88d95461",
      "vendor_id": 5002
    },
    {
      "bus_nr": 1,
      "device_nr": 7,
      "manufacturer": "Logitech",
      "product": "USB Laser Mouse",
      "product_id": 49257,
      "serial": "",
      "vendor_id": 1133
    }
  ],
  "event": "current"
}
{
  "devices": [
    {
      "bus_nr": 1,
      "device_nr": 7,
      "manufacturer": "Logitech",
      "product": "USB Laser Mouse",
      "product_id": 49257,
      "serial": "",
      "vendor_id": 1133
    }
  ],
  "event": "remove"
}
{
  "devices": [
    {
      "bus_nr": 1,
      "device_nr": 8,
      "manufacturer": "Logitech",
      "product": "USB Laser Mouse",
      "product_id": 49257,
      "serial": "",
      "vendor_id": 1133
    }
  ],
  "event": "add"
}
```

Simple read/write operations on the device nodes, exchanging JSON.
```
$ strace -s 1000 varlink call device:/dev/org.kernel.example/org.kernel.sysinfo.GetInfo
[...]
open("/dev/org.kernel.example", O_RDWR|O_CLOEXEC) = 5
write(5, "{\"method\":\"org.kernel.sysinfo.GetInfo\"}\0", 40) = 0
read(5, "{\"parameters\":{\"domainname\":\"(none)\",\"machine\":\"x86_64\",\"nodename\":\"ank\",\"release\":\"4.13.10-200.fc26.x86_64\",\"sysname\":\"Linux\",\"version\":\"#1 SMP Fri Oct 27 15:34:40 UTC 2017\"}}\0", 16777216) = 177
[...]
```
