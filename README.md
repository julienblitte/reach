# reach
Network command line helper to assign a second local IPv4 address on another subnet easily.
This is mostly an IT or device installer tool.

# Usage
```
Usage: reach <ip>[/<mask>] [<interface>] - configure adapter to reach an IPv4
  Add a secondary ipv4 address to Interface to reach target IP.
  If Interface has dhcp configuration, the configuration is first switched to static IP configuration
  (IP address, gateway, mask and DNS servers are preserved).
  Note: if IP is already reachable from Interface, no change is applied.

Usage: reach dhcp [<interface>] - switch configuration to dhcp
  The Interface is switched from static to dhcp configuration, including gateway and DNS servers.

Usage: reach static [<interface>] - switch configuration to static from dhcp information
  The Interface is switched from dhcp to static configuration, using dhcp providing information.
```

# Example
Show help and list interface
`C:\> reach`

You want to access to equipment 192.168.1.100
`C:\> reach 192.168.1.100`

You want to restore your interface to dhcp
`C:\> reach dhcp`

You want you second interface to access to equipment 192.168.1.100
`C:\> reach 192.168.1.100 2`

You want to restore your second interface to dhcp
`C:\> reach dhcp 2`

You want to keep dhcp information in order to switch IPv4
`C:\> reach static`

# FAQ
1. What this tool is about?
This tool helps to access multiple subnets on IPv4 *un-routed* networks to reach a specific device.
This will add a second IP address on relevant subnet and keeps DHCP provided network information on your interface so you won't lose internet connection.

2. Do I need this tool?
Probably not, unlesss you undertand the answer at question 1.

3. Why I should simply do not use Windows interface "ncpa.cpl" to change parameters?
Inefficient, slow, unsafe.
- Inefficient: Does not preserve DHCP provided information when switching to Static configuration, you need to memorize IPv4, Mask, Gateway, DNS and reconfigure it.
- Slow: Even if 0.01% of users will use keyboard, you will probably use the mouse to navigate into parameters and lose 10 minutes of your precious lifetime if you want to setup both subnets properly.
- Unsafe: You might very probably lose internet connection, assuming you have it and if you are not careful (default gateway, DNS) or potentially create overlaps between subnets.

4. Why I cannot use symply "netsh" to change parameters?
Hard to memorize but you are welcome to type the 5 or 6 commands to make the switch manualy like Neo.

5. I cannot find the tool, where it is installed?
It's a command line tool, such as "ipconfig". You need first to open command line interface. Try to go into Start Menu/Execute and type "cmd" as first.

6. Does this work on routed networks (with VLANs, routed subnets, etc.)?
No, this won't work. The tool won't get there is routed local subnets.
This tool assumes you are on a basic Layer-2 network with only one potential internet router to reach internet.

7. How this tool works behind the scene, is it magic?
By desing, Your system accepts serveral IPv4 on same interface, if adresses are on different subnets.
Preserving the configuration from DHCP, and switching to Static configuration will let you have 2 valid IPv4 different addresses at the exact same time on 2 subnets.
Therefore you can, at the same time, configure your device and keep your internet connection.

8. How the new IPv4 is chosen for the new subnet?
Automatically computed based on:
- the new subnet
- your current IPv4 on current subnet
- the device you want to reach (collision avoidance for *only for this IPv4*)

9. Can the tool give an IPv4 address that is already in use?
Yes. Be careful, by default the tool will try to imitate your IPv4 Address and duplicate the same numbers on new subnets but it won't check that this IPv4 address is free before.
Only the target address if avoided.

10. Help! My network does not work anymore after I used this tool.
Switch back configuration to dhcp.
Go into Start Menu/Execute, type "cmd". Once the command line is opened type "reach dhcp".
