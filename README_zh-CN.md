# Major Privacy

<p align='center'>
<a href='./README.md'>EN | 中文</a>
</p>

Major Privacy 是一款面向 Windows 的高级隐私保护工具，是 [PrivateWin10](https://github.com/DavidXanatos/priv10) 项目的延续。该工具通过自定义的内核隔离驱动，实现了大量全新功能。理论上，Major Privacy 属于 HIPS（主机入侵防御系统）。驱动能够监控和过滤文件/注册表访问，并保护进程免受其他进程的操控。它利用 Windows 内置机制限制网络流量，并集成了便捷的隐私增强调整项。此外，还实现了基于规则的软件限制机制，即阻止未授权或不受欢迎的应用程序运行的能力。借助 KernelIsolator 驱动的进程保护功能，Major Privacy 能够保护非特权用户进程免受破坏及数据外泄，即便攻击线程拥有系统或管理员权限。其进程保护能力，与文件和文件夹访问权限控制相结合，可有效保护个人数据。此外，Major Privacy 还能创建存储于加密容器文件中的受保护卷，只有在用户提供正确密码并且工具正在主动过滤文件系统访问时，才能访问机密数据。隐私代理会记录文件、注册表与网络访问，并为主机系统上的进程活动提供完整日志与可视化界面，使用户能够审查应用程序是否仅执行了其指定的操作。Major Privacy 不仅为常规恶意软件提供防线，还能防范那些“合法但越界”的软件，确保用户可安全使用软件，且相关公司无法访问用户未明确授权的信息。

## 🤝 许可协议

本工具为开源项目，但许可协议相比典型的开源许可证更为严格。
MajorPrivacy 具有自由（freedom）的开放性，但不等同于免费。这意味着如果您在使用本软件，需从 [xanasoft.com](https://xanasoft.com) 获取授权许可。界面会不定期进行提醒。
如需公开分发自行编译的二进制文件，请参阅 LICENSE 文件获取详细信息。

## ⏬ 下载

[最新发布版本](https://github.com/xanasoft/MajorPrivacy/releases/latest)

## 🚀 功能特性

- 进程保护 —— 在安全隔离环境中保护用户进程，防止被其他进程（包括提权和系统级进程）干扰  
- 软件限制 —— 阻止不受欢迎的进程启动及 DLL 加载  
- 文件/文件夹保护 —— 防止未授权进程访问指定文件和文件夹  
- 注册表保护 —— 类似于文件/文件夹保护的机制，但对象为注册表项（测试版暂未启用）  
- 网络防火墙 —— 管理输出/输入通讯的高级网络防火墙  
- DNS 监测 —— 监控 DNS 缓存并跟踪事件，识别运行进程访问的域名  
- DNS 过滤 —— 通过一组预定义的黑名单（兼容 Pi Home），以及自定义规则，过滤所有 DNS 请求  
- 代理注入 —— 强制任意进程使用预设网络代理（尚未实现）  
- 安全驱动器 —— 创建完全受文件/文件夹保护机制守护的加密卷，防止未授权访问机密数据（即使在内核模式下，尚未实现）  
- 调优引擎 —— 通过禁用不需的遥测和云端功能，强化 Windows 配置  
- 完整性级别控制 —— 可将进程在安全隔离区内的完整性级别设置为“Protected”，从而隔离用户对象（包括窗口）与其他进程

## 📌 项目支持 / 赞助

[<img align="left" height="64" width="64" src="https://github.com/sandboxie-plus/Sandboxie/raw/master/.github/images/Icons8_logo.png">](https://icons8.de/)特别感谢 [Icons8](https://icons8.de/) 为本项目提供图标  
<br>
<br>
<br>

## ❤️ 支持本项目

1. 订阅 [Patreon](https://www.patreon.com/DavidXanatos)  
2. 通过 [PayPal 或信用卡](https://sandboxie-plus.com/go.php?to=donate) 捐助  
3. 使用加密货币捐助  

BTC: bc1qwnnacet3x3569w8hcns2dwh42jxa73sar4cwe5
ETH: 0xBf08c3c47C5175015cEF4E32fB2315c9111F5305
BNB on BCS: 0xBf08c3c47C5175015cEF4E32fB2315c9111F5305
LTC: LTqXK1UEri1FCv7fNn9bcFhsrh78SaNdSM
BCH: qpkjwn82pz3uj2jdzhv43tmaej2ftzyp8qe0er6ld4
