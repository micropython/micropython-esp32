# Change Log

## [Unreleased](https://github.com/SHA2017-badge/micropython-esp32/tree/HEAD)

[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.9.1...HEAD)

**Closed issues:**

- '123' on the OSK crashes the badge [\#49](https://github.com/SHA2017-badge/micropython-esp32/issues/49)
- 'shift' on the OSK restarts the app [\#47](https://github.com/SHA2017-badge/micropython-esp32/issues/47)

**Merged pull requests:**

- NVS as config store [\#53](https://github.com/SHA2017-badge/micropython-esp32/pull/53) ([annejan](https://github.com/annejan))
- small bug fix in flashfbdev.py/modesp.c [\#52](https://github.com/SHA2017-badge/micropython-esp32/pull/52) ([annejan](https://github.com/annejan))
- Preliminary support for callbacks for buttons [\#51](https://github.com/SHA2017-badge/micropython-esp32/pull/51) ([raboof](https://github.com/raboof))
- Support capitals in prompt\_text \(fixes \#47\) [\#50](https://github.com/SHA2017-badge/micropython-esp32/pull/50) ([raboof](https://github.com/raboof))
- Correctly detect presence of keyword parameters [\#46](https://github.com/SHA2017-badge/micropython-esp32/pull/46) ([raboof](https://github.com/raboof))
- Support dialog.prompt\_text with on-screen keyboard [\#45](https://github.com/SHA2017-badge/micropython-esp32/pull/45) ([raboof](https://github.com/raboof))
- Wear levelling proposed upstream [\#44](https://github.com/SHA2017-badge/micropython-esp32/pull/44) ([annejan](https://github.com/annejan))
- Fix dialog.notice and .prompt\_boolean [\#43](https://github.com/SHA2017-badge/micropython-esp32/pull/43) ([raboof](https://github.com/raboof))
- Fix launching the installer [\#42](https://github.com/SHA2017-badge/micropython-esp32/pull/42) ([raboof](https://github.com/raboof))
- OTA attempt and ussl entropy fix [\#41](https://github.com/SHA2017-badge/micropython-esp32/pull/41) ([annejan](https://github.com/annejan))
- Reduce flushes [\#40](https://github.com/SHA2017-badge/micropython-esp32/pull/40) ([raboof](https://github.com/raboof))
- SSL validation for sha2017.org subdomains [\#38](https://github.com/SHA2017-badge/micropython-esp32/pull/38) ([annejan](https://github.com/annejan))
- Cleanup of build process [\#37](https://github.com/SHA2017-badge/micropython-esp32/pull/37) ([annejan](https://github.com/annejan))
- badge.eink\_busy\(\) and badge.eink\_busy\_wait\(\) [\#36](https://github.com/SHA2017-badge/micropython-esp32/pull/36) ([annejan](https://github.com/annejan))
- Only flush once per keypress [\#35](https://github.com/SHA2017-badge/micropython-esp32/pull/35) ([raboof](https://github.com/raboof))
- Destroy list before launching app [\#34](https://github.com/SHA2017-badge/micropython-esp32/pull/34) ([raboof](https://github.com/raboof))
- Sebastius [\#33](https://github.com/SHA2017-badge/micropython-esp32/pull/33) ([sebastius](https://github.com/sebastius))
- Lets boot straight to launcher instead of a demo app. [\#32](https://github.com/SHA2017-badge/micropython-esp32/pull/32) ([sebastius](https://github.com/sebastius))
- Fix for len= global overwrite [\#31](https://github.com/SHA2017-badge/micropython-esp32/pull/31) ([aczid](https://github.com/aczid))
- Added required tjpegd source file for JPG decoder [\#30](https://github.com/SHA2017-badge/micropython-esp32/pull/30) ([aczid](https://github.com/aczid))
- Enabled support for JPEG images [\#29](https://github.com/SHA2017-badge/micropython-esp32/pull/29) ([aczid](https://github.com/aczid))
- Testable BMP/PNG support [\#28](https://github.com/SHA2017-badge/micropython-esp32/pull/28) ([aczid](https://github.com/aczid))
- Updated UGFX widget bindings [\#27](https://github.com/SHA2017-badge/micropython-esp32/pull/27) ([aczid](https://github.com/aczid))
- .search\(query\), an empty query lists all eggs [\#26](https://github.com/SHA2017-badge/micropython-esp32/pull/26) ([f0x52](https://github.com/f0x52))
- .search .list [\#25](https://github.com/SHA2017-badge/micropython-esp32/pull/25) ([f0x52](https://github.com/f0x52))
- Added listing, dumping and loading of fonts [\#24](https://github.com/SHA2017-badge/micropython-esp32/pull/24) ([aczid](https://github.com/aczid))
- Master [\#23](https://github.com/SHA2017-badge/micropython-esp32/pull/23) ([annejan](https://github.com/annejan))
- Only link led driver when the driver is enabled through the device config [\#22](https://github.com/SHA2017-badge/micropython-esp32/pull/22) ([aczid](https://github.com/aczid))
- Unix version works on macOS now [\#21](https://github.com/SHA2017-badge/micropython-esp32/pull/21) ([annejan](https://github.com/annejan))
- Dev ussl nonblocking [\#20](https://github.com/SHA2017-badge/micropython-esp32/pull/20) ([annejan](https://github.com/annejan))
- Upstream updates [\#18](https://github.com/SHA2017-badge/micropython-esp32/pull/18) ([annejan](https://github.com/annejan))
- esp32/main.c: Added machine.reset\_cause\(\) [\#17](https://github.com/SHA2017-badge/micropython-esp32/pull/17) ([annejan](https://github.com/annejan))
- Fixed button callbacks to start from 1-index not 0 [\#16](https://github.com/SHA2017-badge/micropython-esp32/pull/16) ([aczid](https://github.com/aczid))
- help\(\) function intro text [\#15](https://github.com/SHA2017-badge/micropython-esp32/pull/15) ([aczid](https://github.com/aczid))
- Merge new serial features from micropython upstream [\#14](https://github.com/SHA2017-badge/micropython-esp32/pull/14) ([Roosted7](https://github.com/Roosted7))
- Using mutex-locked version of ginputToggleWakeup [\#13](https://github.com/SHA2017-badge/micropython-esp32/pull/13) ([aczid](https://github.com/aczid))
- Added symlinks for sdcard / vibrator drivers [\#12](https://github.com/SHA2017-badge/micropython-esp32/pull/12) ([aczid](https://github.com/aczid))
- UGFX input driver for micropython + SDL keyboard driver [\#11](https://github.com/SHA2017-badge/micropython-esp32/pull/11) ([aczid](https://github.com/aczid))
- Added modules woezel and urequests to unix build [\#10](https://github.com/SHA2017-badge/micropython-esp32/pull/10) ([aczid](https://github.com/aczid))

## [v1.9.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.9.1) (2017-06-11)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.9...v1.9.1)

**Merged pull requests:**

- More fixes for Unix build [\#9](https://github.com/SHA2017-badge/micropython-esp32/pull/9) ([aczid](https://github.com/aczid))
- Added mocks for modbadge / modnetwork [\#8](https://github.com/SHA2017-badge/micropython-esp32/pull/8) ([aczid](https://github.com/aczid))
- Made the demo thingy properly center text.  [\#7](https://github.com/SHA2017-badge/micropython-esp32/pull/7) ([Peetz0r](https://github.com/Peetz0r))
- Added support for compiling micropython on Linux with SDL-backed ugfx [\#6](https://github.com/SHA2017-badge/micropython-esp32/pull/6) ([aczid](https://github.com/aczid))
- Roosted7's update to latest ESP-IDF [\#5](https://github.com/SHA2017-badge/micropython-esp32/pull/5) ([annejan](https://github.com/annejan))
- Proper deepsleep as proposed upstream  [\#4](https://github.com/SHA2017-badge/micropython-esp32/pull/4) ([annejan](https://github.com/annejan))
- esp32/modsocket: Make read/write return None when in non-blocking mode. [\#3](https://github.com/SHA2017-badge/micropython-esp32/pull/3) ([annejan](https://github.com/annejan))
- Add a mp api that allows for using the RTC non-volatile memory. [\#2](https://github.com/SHA2017-badge/micropython-esp32/pull/2) ([rnplus](https://github.com/rnplus))
- Updated from micropython-esp32 [\#1](https://github.com/SHA2017-badge/micropython-esp32/pull/1) ([annejan](https://github.com/annejan))

## [v1.9](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.9) (2017-05-26)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.7...v1.9)

**Merged pull requests:**

- Dev machine unique [\#19](https://github.com/SHA2017-badge/micropython-esp32/pull/19) ([annejan](https://github.com/annejan))

## [v1.8.7](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.7) (2017-01-08)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.6...v1.8.7)

## [v1.8.6](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.6) (2016-11-10)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.5...v1.8.6)

## [v1.8.5](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.5) (2016-10-17)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.4...v1.8.5)

## [v1.8.4](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.4) (2016-09-09)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.3...v1.8.4)

## [v1.8.3](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.3) (2016-08-09)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.2...v1.8.3)

## [v1.8.2](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.2) (2016-07-10)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8.1...v1.8.2)

## [v1.8.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8.1) (2016-06-03)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.8...v1.8.1)

## [v1.8](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.8) (2016-05-03)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.7...v1.8)

## [v1.7](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.7) (2016-04-11)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.6...v1.7)

## [v1.6](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.6) (2016-01-31)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.5.2...v1.6)

## [v1.5.2](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.5.2) (2015-12-31)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.5.1...v1.5.2)

## [v1.5.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.5.1) (2015-11-23)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.5...v1.5.1)

## [v1.5](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.5) (2015-10-21)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.6...v1.5)

## [v1.4.6](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.6) (2015-09-23)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.5...v1.4.6)

## [v1.4.5](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.5) (2015-08-11)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.4...v1.4.5)

## [v1.4.4](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.4) (2015-06-15)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.3...v1.4.4)

## [v1.4.3](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.3) (2015-05-16)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.2...v1.4.3)

## [v1.4.2](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.2) (2015-04-21)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4.1...v1.4.2)

## [v1.4.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4.1) (2015-04-04)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.4...v1.4.1)

## [v1.4](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.4) (2015-03-29)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.10...v1.4)

## [v1.3.10](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.10) (2015-02-13)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.9...v1.3.10)

## [v1.3.9](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.9) (2015-01-25)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.8...v1.3.9)

## [v1.3.8](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.8) (2014-12-28)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.7...v1.3.8)

## [v1.3.7](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.7) (2014-11-28)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.6...v1.3.7)

## [v1.3.6](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.6) (2014-11-04)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.5...v1.3.6)

## [v1.3.5](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.5) (2014-10-26)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.4...v1.3.5)

## [v1.3.4](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.4) (2014-10-21)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.3...v1.3.4)

## [v1.3.3](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.3) (2014-10-02)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.2...v1.3.3)

## [v1.3.2](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.2) (2014-09-25)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3.1...v1.3.2)

## [v1.3.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3.1) (2014-08-28)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.3...v1.3.1)

## [v1.3](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.3) (2014-08-12)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.2...v1.3)

## [v1.2](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.2) (2014-07-13)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.1.1...v1.2)

## [v1.1.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.1.1) (2014-06-15)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.1...v1.1.1)

## [v1.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.1) (2014-06-12)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.0.1...v1.1)

## [v1.0.1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.0.1) (2014-05-11)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.0...v1.0.1)

## [v1.0](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.0) (2014-05-03)
[Full Changelog](https://github.com/SHA2017-badge/micropython-esp32/compare/v1.0-rc1...v1.0)

## [v1.0-rc1](https://github.com/SHA2017-badge/micropython-esp32/tree/v1.0-rc1) (2014-05-03)


\* *This Change Log was automatically generated by [github_changelog_generator](https://github.com/skywinder/Github-Changelog-Generator)*