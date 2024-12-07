# MajorPrivacy
Major Privacy is an advanced privacy tool for windows, it is a continuation of the [PrivateWin10](https://github.com/DavidXanatos/priv10) project, it brings a multitude of new functionality made possible through the use of a custom kernel isolation driver. Conceptually Major Privacy is a HIPS, the driver can monitor/filter, file/registry access as well as protect processes from being manipulated by other processes. It leverages the windows built in mechanisms to restrict network traffic and brings a convenient collection of privacy enhancing tweaks. It implements an own rule-based software restriction mechanism, i.e. the ability to prevent unauthorized or undesired applications from running. Using the process protection feature of the KernelIsolator driver Major Privacy can protect unprivileged user processes from being compromised and their secrets exfiltrated, even from threads running with system or administrative privileges. The ability to protect processes in combination with its ability to restrict access to files and folders helps to protect personal data. Furthermore, Major Privacy is capable of creating protected volumes located in encrypted container files, such that access to confidential data is only possible when the user provided the correct password and the tool is actively filtering filesystem accesses. The privacy agent logs file, registry and network access and provides comprehensive logs and visualizations of process activity on the host system, enabling the users to check if their applications are only doing what they want them to do. Major Privacy is designed to not only provide a line of defense against regular malware but also defend the user from legitimate but overreaching software, such that the software can be safely used and the companies behind it won’t be able to access anything the user did not choose to make available.

## 🤝 Licensing 
This tool is open source; however, the license is a bit more restrictive than typical open source licenses. 
MajorPrivacy is free like in freedom not like in free beer, meaning if you are using it you are expected to get a license from xanasoft.com, the UI will remind you of that from time to time.
If you want to publicly distribute self-compiled binaries review the LICENSE file for details.

## ⏬ Download

[Latest Release](https://github.com/xanasoft/MajorPrivacy/releases/latest)

## 🚀 Features

* Process Protection - Protect user processes in secure enclaves from other processes including elevated and system processes.
* Software Restriction - Block undesired processes from starting and undesired Dll’s from loading.
* File/Folder Protection - Protect selected files and folders from being accessed by unauthorized processes.
* Registry Protection - like File/Folder Protection just for registry keys. (not enabled in beta build)
* Network Firewall - Advanced network firewall to manage outbound and inbound communication.
* DNS Inspection - Monitor DNS cache and trace events to determine which domains are being accessed by running processes.
* DNS Fitlering - Filter all DNS requests with a set of pre-defined block lists (pi home compatible) and own custom rules. (not yet implemented)
* Proxy Injection - Force arbitrary processes to use pre-defined network proxies. (not yet implemented)
* Secure Drives - Create encrypted volumes stored in disk images fully guarded by File/Folder protection to prevent unauthorized access to confidential data.
* Tweak Engine - Hardens windows configuration by disabling undesired telemetry and cloud features.
* Integrity level control - set the integrity level of processes in a secure enclave to "Protected" to isolate user objects, including their windows, from other processes(not yet implemented)
* Isoated Desktop - Run protected processes in a isoated desktop to prevent their windows from being controlled by untrusted programs.(not yet introduced)
* Time-Effect Rules - Grant processes only within certain period.(not yet implemented)
* Window Content Protection - Prevent protected processes from being screenshotted(not yet introduced)

## 📌 Project support / sponsorship

[<img align="left" height="64" width="64" src="https://github.com/sandboxie-plus/Sandboxie/raw/master/.github/images/Icons8_logo.png">](https://icons8.de/)Thank you [Icons8](https://icons8.de/) for providing icons for the project.
<br>
<br>
<br>

## ❤️ Support the project

1. Get a [Patreon subscription](https://www.patreon.com/DavidXanatos) <br>

2. Donate with [PayPal or Credit Card](https://sandboxie-plus.com/go.php?to=donate) <br>

3. Donate with cryptocurrencies <br>

BTC: bc1qwnnacet3x3569w8hcns2dwh42jxa73sar4cwe5 <br>
ETH: 0xBf08c3c47C5175015cEF4E32fB2315c9111F5305 <br>
BNB on BCS: 0xBf08c3c47C5175015cEF4E32fB2315c9111F5305 <br>
LTC: LTqXK1UEri1FCv7fNn9bcFhsrh78SaNdSM <br>
BCH: qpkjwn82pz3uj2jdzhv43tmaej2ftzyp8qe0er6ld4 <br>
