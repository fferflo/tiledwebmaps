# Change Log

## [0.2.0]

Large refactor of the library.

### Added

- Added option for storing layout in ``layout.yaml`.
- Added scripts for bulk downloading imagery.
- Added ``min_zoom`` and ``max_zoom`` property to all tileloaders.

### Changed

- Removed cosy1 dependency.
- Improved tile loading speed.



## [0.1.2]

### Changed

- Improved error handling and reporting.

### Fixed

- Fixed bug that caused "Illegal instruction (core dumped)" on some devices. (Remove -march=native)